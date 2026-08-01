#include "monitor/snapshot_store.hpp"

#include <spdlog/spdlog.h>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

#include <algorithm>
#include <map>
#include <utility>

namespace cleaner::monitor {

namespace {

QString to_qt(const std::string& text) {
    return QString::fromStdString(text);
}

std::string from_qt(const QVariant& value) {
    return value.toString().toStdString();
}

/// Cada instancia usa uma conexao propria: o Qt identifica conexoes por nome, e
/// duas instancias com o mesmo nome brigariam pelo mesmo banco.
QString next_connection_name() {
    static int counter = 0;
    return QStringLiteral("cleaner_monitor_%1").arg(counter++);
}

}

struct SnapshotStore::Impl {
    QString connection_name;
    std::filesystem::path path;
    bool ready = false;

    [[nodiscard]] QSqlDatabase database() const {
        return QSqlDatabase::database(connection_name);
    }
};

SnapshotStore::SnapshotStore(std::filesystem::path database_path)
    : impl_(std::make_unique<Impl>()) {
    impl_->path = std::move(database_path);
    impl_->connection_name = next_connection_name();

    std::error_code error;
    std::filesystem::create_directories(impl_->path.parent_path(), error);

    auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), impl_->connection_name);

    // O caminho vai como wstring: converter para texto estreito quebra em
    // perfil com acento, defeito que ja apareceu tres vezes neste projeto.
    database.setDatabaseName(QString::fromStdWString(impl_->path.wstring()));

    if (!database.open()) {
        spdlog::error("nao foi possivel abrir o banco de retratos: {}",
                      database.lastError().text().toStdString());
        return;
    }

    QSqlQuery query(database);

    // WAL deixa a leitura acontecer enquanto um retrato esta sendo gravado, e a
    // gravacao de um retrato inteiro demora.
    query.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    query.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));

    const bool created =
        query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS snapshot ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  taken_at TEXT NOT NULL,"
            "  volume TEXT NOT NULL,"
            "  total_bytes INTEGER NOT NULL,"
            "  free_bytes INTEGER NOT NULL,"
            "  kind TEXT NOT NULL,"
            "  app_version TEXT NOT NULL,"
            "  complete INTEGER NOT NULL)")) &&
        query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS folder ("
            "  snapshot_id INTEGER NOT NULL REFERENCES snapshot(id) ON DELETE CASCADE,"
            "  path TEXT NOT NULL,"
            "  logical_bytes INTEGER NOT NULL,"
            "  allocated_bytes INTEGER NOT NULL,"
            "  file_count INTEGER NOT NULL,"
            "  depth INTEGER NOT NULL)")) &&
        query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS tracked_file ("
            "  snapshot_id INTEGER NOT NULL REFERENCES snapshot(id) ON DELETE CASCADE,"
            "  path TEXT NOT NULL,"
            "  logical_bytes INTEGER NOT NULL,"
            "  allocated_bytes INTEGER NOT NULL,"
            "  modified_at TEXT)")) &&
        // A comparacao junta as duas pontas por caminho: sem indice ela varre
        // dezenas de milhares de linhas duas vezes.
        query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS folder_by_snapshot ON folder(snapshot_id, path)")) &&
        query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS snapshot_by_volume ON snapshot(volume, taken_at)"));

    if (!created) {
        spdlog::error("nao foi possivel preparar o banco de retratos: {}",
                      query.lastError().text().toStdString());
        return;
    }

    impl_->ready = true;
}

SnapshotStore::~SnapshotStore() {
    if (impl_ == nullptr) {
        return;
    }

    const QString name = impl_->connection_name;
    {
        auto database = QSqlDatabase::database(name, false);
        if (database.isOpen()) {
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(name);
}

bool SnapshotStore::ok() const {
    return impl_ != nullptr && impl_->ready;
}

std::int64_t SnapshotStore::save(const Snapshot& snapshot) {
    if (!ok()) {
        return 0;
    }

    auto database = impl_->database();

    // Um retrato inteiro numa transacao so: dezenas de milhares de linhas em
    // transacoes separadas levariam minutos.
    database.transaction();

    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO snapshot (taken_at, volume, total_bytes, free_bytes, kind, app_version,"
        " complete) VALUES (?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(to_qt(snapshot.taken_at));
    query.addBindValue(to_qt(snapshot.volume));
    query.addBindValue(static_cast<qlonglong>(snapshot.total_bytes));
    query.addBindValue(static_cast<qlonglong>(snapshot.free_bytes));
    query.addBindValue(to_qt(snapshot.kind));
    query.addBindValue(to_qt(snapshot.app_version));
    query.addBindValue(snapshot.complete ? 1 : 0);

    if (!query.exec()) {
        spdlog::error("falha ao gravar o retrato: {}", query.lastError().text().toStdString());
        database.rollback();
        return 0;
    }

    const auto id = query.lastInsertId().toLongLong();

    QSqlQuery folders(database);
    folders.prepare(QStringLiteral(
        "INSERT INTO folder (snapshot_id, path, logical_bytes, allocated_bytes, file_count, depth)"
        " VALUES (?, ?, ?, ?, ?, ?)"));

    for (const auto& folder : snapshot.folders) {
        folders.addBindValue(static_cast<qlonglong>(id));
        folders.addBindValue(to_qt(folder.path));
        folders.addBindValue(static_cast<qlonglong>(folder.logical_bytes));
        folders.addBindValue(static_cast<qlonglong>(folder.allocated_bytes));
        folders.addBindValue(static_cast<qlonglong>(folder.file_count));
        folders.addBindValue(folder.depth);

        if (!folders.exec()) {
            spdlog::error("falha ao gravar pasta do retrato: {}",
                          folders.lastError().text().toStdString());
            database.rollback();
            return 0;
        }
    }

    QSqlQuery files(database);
    files.prepare(QStringLiteral(
        "INSERT INTO tracked_file (snapshot_id, path, logical_bytes, allocated_bytes, modified_at)"
        " VALUES (?, ?, ?, ?, ?)"));

    for (const auto& file : snapshot.files) {
        files.addBindValue(static_cast<qlonglong>(id));
        files.addBindValue(to_qt(file.path));
        files.addBindValue(static_cast<qlonglong>(file.logical_bytes));
        files.addBindValue(static_cast<qlonglong>(file.allocated_bytes));
        files.addBindValue(to_qt(file.modified_at));

        if (!files.exec()) {
            database.rollback();
            return 0;
        }
    }

    if (!database.commit()) {
        spdlog::error("falha ao concluir a gravacao do retrato");
        return 0;
    }

    return id;
}

namespace {

Snapshot read_row(const QSqlQuery& query) {
    Snapshot snapshot;
    snapshot.id = query.value(0).toLongLong();
    snapshot.taken_at = from_qt(query.value(1));
    snapshot.volume = from_qt(query.value(2));
    snapshot.total_bytes = query.value(3).toULongLong();
    snapshot.free_bytes = query.value(4).toULongLong();
    snapshot.kind = from_qt(query.value(5));
    snapshot.app_version = from_qt(query.value(6));
    snapshot.complete = query.value(7).toInt() != 0;
    return snapshot;
}

constexpr auto kSnapshotColumns =
    "id, taken_at, volume, total_bytes, free_bytes, kind, app_version, complete";

}

std::vector<Snapshot> SnapshotStore::list(int limit) const {
    if (!ok()) {
        return {};
    }

    QSqlQuery query(impl_->database());
    query.prepare(QString::fromLatin1("SELECT %1 FROM snapshot ORDER BY taken_at DESC LIMIT ?")
                      .arg(QString::fromLatin1(kSnapshotColumns)));
    query.addBindValue(limit);

    std::vector<Snapshot> snapshots;
    if (query.exec()) {
        while (query.next()) {
            snapshots.push_back(read_row(query));
        }
    }
    return snapshots;
}

std::optional<Snapshot> SnapshotStore::latest(const std::string& volume) const {
    if (!ok()) {
        return std::nullopt;
    }

    QSqlQuery query(impl_->database());
    query.prepare(QString::fromLatin1("SELECT %1 FROM snapshot WHERE volume = ? AND complete = 1"
                                      " ORDER BY taken_at DESC LIMIT 1")
                      .arg(QString::fromLatin1(kSnapshotColumns)));
    query.addBindValue(to_qt(volume));

    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }

    Snapshot snapshot = read_row(query);

    QSqlQuery folders(impl_->database());
    folders.prepare(QStringLiteral(
        "SELECT path, logical_bytes, allocated_bytes, file_count, depth FROM folder"
        " WHERE snapshot_id = ?"));
    folders.addBindValue(static_cast<qlonglong>(snapshot.id));

    if (folders.exec()) {
        while (folders.next()) {
            snapshot.folders.push_back(FolderSize{
                .path = from_qt(folders.value(0)),
                .logical_bytes = folders.value(1).toULongLong(),
                .allocated_bytes = folders.value(2).toULongLong(),
                .file_count = static_cast<std::size_t>(folders.value(3).toULongLong()),
                .depth = folders.value(4).toInt(),
            });
        }
    }

    return snapshot;
}

std::optional<Snapshot> SnapshotStore::closest_to(const std::string& volume,
                                                  const std::string& taken_at) const {
    if (!ok()) {
        return std::nullopt;
    }

    QSqlQuery query(impl_->database());
    query.prepare(QString::fromLatin1(
                      "SELECT %1 FROM snapshot WHERE volume = ? AND complete = 1"
                      " ORDER BY ABS(JULIANDAY(taken_at) - JULIANDAY(?)) ASC LIMIT 1")
                      .arg(QString::fromLatin1(kSnapshotColumns)));
    query.addBindValue(to_qt(volume));
    query.addBindValue(to_qt(taken_at));

    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    return read_row(query);
}

std::optional<SnapshotDiff> SnapshotStore::compare(std::int64_t from_id,
                                                   std::int64_t to_id) const {
    if (!ok()) {
        return std::nullopt;
    }

    const auto load_header = [this](std::int64_t id) -> std::optional<Snapshot> {
        QSqlQuery query(impl_->database());
        query.prepare(QString::fromLatin1("SELECT %1 FROM snapshot WHERE id = ?")
                          .arg(QString::fromLatin1(kSnapshotColumns)));
        query.addBindValue(static_cast<qlonglong>(id));

        if (!query.exec() || !query.next()) {
            return std::nullopt;
        }
        return read_row(query);
    };

    const auto before = load_header(from_id);
    const auto after = load_header(to_id);

    if (!before || !after) {
        return std::nullopt;
    }

    // Retrato parcial nao serve de referencia: o que nao foi visitado apareceria
    // como espaco que sumiu.
    if (!before->complete || !after->complete) {
        spdlog::warn("comparacao recusada: um dos retratos esta incompleto");
        return std::nullopt;
    }

    SnapshotDiff diff;
    diff.from_taken_at = before->taken_at;
    diff.to_taken_at = after->taken_at;
    diff.free_space_delta =
        static_cast<std::int64_t>(after->free_bytes) - static_cast<std::int64_t>(before->free_bytes);

    const auto load_folders = [this](std::int64_t id) {
        std::map<std::string, std::uint64_t> sizes;

        QSqlQuery query(impl_->database());
        query.prepare(QStringLiteral("SELECT path, allocated_bytes FROM folder WHERE snapshot_id = ?"));
        query.addBindValue(static_cast<qlonglong>(id));

        if (query.exec()) {
            while (query.next()) {
                sizes.emplace(from_qt(query.value(0)), query.value(1).toULongLong());
            }
        }
        return sizes;
    };

    const auto before_sizes = load_folders(from_id);
    const auto after_sizes = load_folders(to_id);

    for (const auto& [path, after_bytes] : after_sizes) {
        const auto found = before_sizes.find(path);
        const bool appeared = found == before_sizes.end();
        const std::uint64_t before_bytes = appeared ? 0 : found->second;

        if (!appeared && before_bytes == after_bytes) {
            continue;
        }

        diff.changes.push_back(FolderChange{
            .path = path,
            .before_bytes = before_bytes,
            .after_bytes = after_bytes,
            .delta_bytes =
                static_cast<std::int64_t>(after_bytes) - static_cast<std::int64_t>(before_bytes),
            .appeared = appeared,
        });
    }

    // Pasta que sumiu tambem e resposta: explica espaco que voltou.
    for (const auto& [path, before_bytes] : before_sizes) {
        if (after_sizes.contains(path)) {
            continue;
        }

        diff.changes.push_back(FolderChange{
            .path = path,
            .before_bytes = before_bytes,
            .after_bytes = 0,
            .delta_bytes = -static_cast<std::int64_t>(before_bytes),
            .disappeared = true,
        });
    }

    std::sort(diff.changes.begin(), diff.changes.end(),
              [](const FolderChange& left, const FolderChange& right) {
                  return left.delta_bytes > right.delta_bytes;
              });

    return diff;
}

std::size_t SnapshotStore::apply_retention() {
    if (!ok()) {
        return 0;
    }

    auto database = impl_->database();

    // Guarda tudo do ultimo dia, um por dia no ultimo mes e um por semana antes
    // disso. O mais recente de cada grupo e o que fica.
    QSqlQuery query(database);
    const bool ok_query = query.exec(QStringLiteral(
        "DELETE FROM snapshot WHERE id NOT IN ("
        "  SELECT id FROM snapshot WHERE JULIANDAY('now') - JULIANDAY(taken_at) <= 1"
        "  UNION"
        "  SELECT MAX(id) FROM snapshot"
        "    WHERE JULIANDAY('now') - JULIANDAY(taken_at) <= 30"
        "    GROUP BY DATE(taken_at)"
        "  UNION"
        "  SELECT MAX(id) FROM snapshot"
        "    GROUP BY STRFTIME('%Y-%W', taken_at)"
        "  UNION"
        "  SELECT MAX(id) FROM snapshot"
        ")"));

    if (!ok_query) {
        spdlog::warn("falha ao aplicar retencao: {}", query.lastError().text().toStdString());
        return 0;
    }

    const auto removed = static_cast<std::size_t>(query.numRowsAffected());
    if (removed == 0) {
        return 0;
    }

    // Sem chave estrangeira ativa por padrao no SQLite, as filhas ficam para
    // tras e o banco cresce sozinho.
    QSqlQuery cleanup(database);
    cleanup.exec(QStringLiteral(
        "DELETE FROM folder WHERE snapshot_id NOT IN (SELECT id FROM snapshot)"));
    cleanup.exec(QStringLiteral(
        "DELETE FROM tracked_file WHERE snapshot_id NOT IN (SELECT id FROM snapshot)"));

    // Sem isto o arquivo mantem o tamanho de antes da limpeza, e um monitor de
    // espaco que nao devolve espaco seria contraditorio.
    cleanup.exec(QStringLiteral("VACUUM"));

    return removed;
}

std::uint64_t SnapshotStore::database_size_bytes() const {
    if (!ok()) {
        return 0;
    }

    std::error_code error;
    std::uint64_t total = std::filesystem::file_size(impl_->path, error);
    if (error) {
        return 0;
    }

    // O modo WAL mantem parte do conteudo em arquivos ao lado.
    for (const auto* suffix : {"-wal", "-shm"}) {
        std::filesystem::path companion = impl_->path;
        companion += suffix;

        if (const auto size = std::filesystem::file_size(companion, error); !error) {
            total += size;
        }
    }

    return total;
}

}
