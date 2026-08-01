#include "monitor/action_log.hpp"

#include <spdlog/spdlog.h>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

#include <chrono>
#include <ctime>
#include <utility>

namespace cleaner::monitor {

namespace {

QString next_connection_name() {
    static int counter = 0;
    return QStringLiteral("cleaner_actions_%1").arg(counter++);
}

std::string now_iso8601() {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::tm parts{};
    ::localtime_s(&parts, &now);

    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &parts);
    return buffer;
}

int to_int(ActionKind kind) {
    switch (kind) {
    case ActionKind::Deleted:
        return 0;
    case ActionKind::Quarantined:
        return 1;
    case ActionKind::Restored:
        return 2;
    case ActionKind::CommandRun:
        return 3;
    }
    return 0;
}

ActionKind to_kind(int value) {
    switch (value) {
    case 1:
        return ActionKind::Quarantined;
    case 2:
        return ActionKind::Restored;
    case 3:
        return ActionKind::CommandRun;
    default:
        return ActionKind::Deleted;
    }
}

ActionRecord read_row(const QSqlQuery& query) {
    ActionRecord record;
    record.id = query.value(0).toLongLong();
    record.at = query.value(1).toString().toStdString();
    record.kind = to_kind(query.value(2).toInt());
    record.reason = query.value(3).toString().toStdString();
    record.target = query.value(4).toString().toStdString();
    record.item_count = static_cast<std::size_t>(query.value(5).toULongLong());
    record.bytes = query.value(6).toULongLong();
    record.skipped_count = static_cast<std::size_t>(query.value(7).toULongLong());
    record.reversible = query.value(8).toInt() != 0;
    return record;
}

constexpr auto kColumns =
    "id, at, kind, reason, target, item_count, bytes, skipped_count, reversible";

}

struct ActionLog::Impl {
    QString connection_name;
    bool ready = false;

    [[nodiscard]] QSqlDatabase database() const {
        return QSqlDatabase::database(connection_name);
    }
};

ActionLog::ActionLog(std::filesystem::path database_path) : impl_(std::make_unique<Impl>()) {
    impl_->connection_name = next_connection_name();

    std::error_code error;
    std::filesystem::create_directories(database_path.parent_path(), error);

    auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), impl_->connection_name);
    database.setDatabaseName(QString::fromStdWString(database_path.wstring()));

    if (!database.open()) {
        spdlog::error("nao foi possivel abrir o registro de acoes: {}",
                      database.lastError().text().toStdString());
        return;
    }

    QSqlQuery query(database);
    query.exec(QStringLiteral("PRAGMA journal_mode=WAL"));

    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS action ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  at TEXT NOT NULL,"
            "  kind INTEGER NOT NULL,"
            "  reason TEXT NOT NULL,"
            "  target TEXT NOT NULL,"
            "  item_count INTEGER NOT NULL,"
            "  bytes INTEGER NOT NULL,"
            "  skipped_count INTEGER NOT NULL,"
            "  reversible INTEGER NOT NULL)"))) {
        spdlog::error("nao foi possivel preparar o registro de acoes: {}",
                      query.lastError().text().toStdString());
        return;
    }

    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS action_by_time ON action(at DESC)"));

    impl_->ready = true;
}

ActionLog::~ActionLog() {
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

bool ActionLog::ok() const {
    return impl_ != nullptr && impl_->ready;
}

std::int64_t ActionLog::record(const ActionRecord& action) {
    if (!ok()) {
        // Falhar em registrar nao pode passar batido: o registro e a unica
        // rastreabilidade de uma acao que nao tem volta.
        spdlog::error("acao nao registrada: o historico esta indisponivel");
        return 0;
    }

    QSqlQuery query(impl_->database());
    query.prepare(QStringLiteral(
        "INSERT INTO action (at, kind, reason, target, item_count, bytes, skipped_count,"
        " reversible) VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

    query.addBindValue(QString::fromStdString(action.at.empty() ? now_iso8601() : action.at));
    query.addBindValue(to_int(action.kind));
    query.addBindValue(QString::fromStdString(action.reason));
    query.addBindValue(QString::fromStdString(action.target));
    query.addBindValue(static_cast<qlonglong>(action.item_count));
    query.addBindValue(static_cast<qlonglong>(action.bytes));
    query.addBindValue(static_cast<qlonglong>(action.skipped_count));
    query.addBindValue(action.reversible ? 1 : 0);

    if (!query.exec()) {
        spdlog::error("falha ao registrar acao: {}", query.lastError().text().toStdString());
        return 0;
    }

    return query.lastInsertId().toLongLong();
}

std::vector<ActionRecord> ActionLog::recent(int limit) const {
    if (!ok()) {
        return {};
    }

    QSqlQuery query(impl_->database());
    query.prepare(QString::fromLatin1("SELECT %1 FROM action ORDER BY at DESC, id DESC LIMIT ?")
                      .arg(QString::fromLatin1(kColumns)));
    query.addBindValue(limit);

    std::vector<ActionRecord> records;
    if (query.exec()) {
        while (query.next()) {
            records.push_back(read_row(query));
        }
    }
    return records;
}

std::vector<ActionRecord> ActionLog::search(const std::string& term, int limit) const {
    if (!ok()) {
        return {};
    }

    QSqlQuery query(impl_->database());
    query.prepare(QString::fromLatin1(
                      "SELECT %1 FROM action WHERE target LIKE ? OR reason LIKE ?"
                      " ORDER BY at DESC, id DESC LIMIT ?")
                      .arg(QString::fromLatin1(kColumns)));

    const auto pattern = QStringLiteral("%%%1%%").arg(QString::fromStdString(term));
    query.addBindValue(pattern);
    query.addBindValue(pattern);
    query.addBindValue(limit);

    std::vector<ActionRecord> records;
    if (query.exec()) {
        while (query.next()) {
            records.push_back(read_row(query));
        }
    }
    return records;
}

std::uint64_t ActionLog::total_freed_bytes() const {
    if (!ok()) {
        return 0;
    }

    QSqlQuery query(impl_->database());

    // Restauracao devolve arquivo ao disco: contar como espaco liberado
    // inflaria o total.
    if (!query.exec(QStringLiteral("SELECT COALESCE(SUM(bytes), 0) FROM action WHERE kind != 2")) ||
        !query.next()) {
        return 0;
    }

    return query.value(0).toULongLong();
}

}
