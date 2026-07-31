#include <catch2/catch_test_macros.hpp>
#include <monitor/snapshot_store.hpp>

#include <string>

using zelo::monitor::FolderSize;
using zelo::monitor::Snapshot;
using zelo::monitor::SnapshotStore;

namespace {

class TempDatabase {
public:
    TempDatabase() {
        std::error_code error;
        std::filesystem::create_directories(path_.parent_path(), error);
        std::filesystem::remove(path_, error);
    }

    ~TempDatabase() {
        std::error_code error;
        std::filesystem::remove_all(path_.parent_path(), error);
    }

    TempDatabase(const TempDatabase&) = delete;
    TempDatabase& operator=(const TempDatabase&) = delete;
    TempDatabase(TempDatabase&&) = delete;
    TempDatabase& operator=(TempDatabase&&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    static int counter() {
        static int value = 0;
        return value++;
    }

    // Com acento de proposito: o perfil de quem usa o Zelo costuma ter um, e
    // essa classe de defeito ja apareceu tres vezes no projeto.
    std::filesystem::path path_ = std::filesystem::temp_directory_path() /
                                  ("zelo-monitor-João-" + std::to_string(counter())) /
                                  "snapshots.sqlite";
};

Snapshot make_snapshot(const std::string& taken_at, std::uint64_t free_bytes,
                       const std::vector<FolderSize>& folders) {
    Snapshot snapshot;
    snapshot.taken_at = taken_at;
    snapshot.volume = "C:";
    snapshot.total_bytes = 500ULL * 1024 * 1024 * 1024;
    snapshot.free_bytes = free_bytes;
    snapshot.kind = "manual";
    snapshot.app_version = "0.3.0";
    snapshot.complete = true;
    snapshot.folders = folders;
    return snapshot;
}

}

TEST_CASE("o banco abre em caminho com acento", "[snapshot_store]") {
    const TempDatabase database;
    const SnapshotStore store{database.path()};

    REQUIRE(store.ok());
    CHECK(std::filesystem::exists(database.path()));
}

TEST_CASE("um retrato guardado volta inteiro", "[snapshot_store]") {
    const TempDatabase database;
    SnapshotStore store{database.path()};
    REQUIRE(store.ok());

    const auto id = store.save(make_snapshot(
        "2026-07-31T10:00:00", 10ULL * 1024 * 1024 * 1024,
        {{.path = "C:\\Users\\João\\AppData", .logical_bytes = 5000, .allocated_bytes = 6000,
          .file_count = 12, .depth = 1}}));

    REQUIRE(id > 0);

    const auto latest = store.latest("C:");
    REQUIRE(latest.has_value());
    CHECK(latest->taken_at == "2026-07-31T10:00:00");
    CHECK(latest->free_bytes == 10ULL * 1024 * 1024 * 1024);

    REQUIRE(latest->folders.size() == 1);
    CHECK(latest->folders.front().path == "C:\\Users\\João\\AppData");
    CHECK(latest->folders.front().allocated_bytes == 6000);
}

// A pergunta central do monitoramento. Sem isto, o resto nao serve para nada.
TEST_CASE("a comparacao mostra o que cresceu e o que encolheu", "[snapshot_store]") {
    const TempDatabase database;
    SnapshotStore store{database.path()};
    REQUIRE(store.ok());

    const auto antes = store.save(make_snapshot(
        "2026-07-30T10:00:00", 10ULL * 1024 * 1024 * 1024,
        {{.path = "C:\\A", .allocated_bytes = 1000},
         {.path = "C:\\B", .allocated_bytes = 5000},
         {.path = "C:\\Some", .allocated_bytes = 700}}));

    const auto depois = store.save(make_snapshot(
        "2026-07-31T10:00:00", 7ULL * 1024 * 1024 * 1024,
        {{.path = "C:\\A", .allocated_bytes = 4000},
         {.path = "C:\\B", .allocated_bytes = 2000},
         {.path = "C:\\Nova", .allocated_bytes = 900}}));

    const auto diff = store.compare(antes, depois);
    REQUIRE(diff.has_value());

    CHECK(diff->free_space_delta == -3LL * 1024 * 1024 * 1024);

    REQUIRE(diff->changes.size() == 4);

    // Maior crescimento primeiro.
    CHECK(diff->changes.front().path == "C:\\A");
    CHECK(diff->changes.front().delta_bytes == 3000);

    const auto find = [&diff](const std::string& path) {
        return std::find_if(diff->changes.begin(), diff->changes.end(),
                            [&path](const auto& change) { return change.path == path; });
    };

    const auto nova = find("C:\\Nova");
    REQUIRE(nova != diff->changes.end());
    CHECK(nova->appeared);
    CHECK(nova->delta_bytes == 900);

    const auto sumiu = find("C:\\Some");
    REQUIRE(sumiu != diff->changes.end());
    CHECK(sumiu->disappeared);
    CHECK(sumiu->delta_bytes == -700);

    const auto encolheu = find("C:\\B");
    REQUIRE(encolheu != diff->changes.end());
    CHECK(encolheu->delta_bytes == -3000);
}

// Um retrato parcial cruzado com um inteiro mostraria como reducao aquilo que
// apenas nao foi visitado — e mandaria o usuario procurar espaco que nunca
// sumiu.
TEST_CASE("retrato incompleto nao entra em comparacao", "[snapshot_store]") {
    const TempDatabase database;
    SnapshotStore store{database.path()};
    REQUIRE(store.ok());

    auto parcial = make_snapshot("2026-07-30T10:00:00", 1000, {{.path = "C:\\A"}});
    parcial.complete = false;

    const auto incompleto = store.save(parcial);
    const auto completo = store.save(make_snapshot("2026-07-31T10:00:00", 900, {{.path = "C:\\A"}}));

    CHECK_FALSE(store.compare(incompleto, completo).has_value());
    CHECK_FALSE(store.latest("C:")->complete == false);
}

TEST_CASE("comparar identificadores inexistentes nao quebra", "[snapshot_store]") {
    const TempDatabase database;
    SnapshotStore store{database.path()};
    REQUIRE(store.ok());

    CHECK_FALSE(store.compare(1, 2).has_value());
}

// O monitor nao pode virar mais um motivo de disco cheio.
TEST_CASE("a retencao remove retratos antigos", "[snapshot_store]") {
    const TempDatabase database;
    SnapshotStore store{database.path()};
    REQUIRE(store.ok());

    // Dois no mesmo dia, muito antigos: a retencao guarda um por dia.
    store.save(make_snapshot("2020-01-01T08:00:00", 1000, {{.path = "C:\\A"}}));
    store.save(make_snapshot("2020-01-01T20:00:00", 1000, {{.path = "C:\\A"}}));
    store.save(make_snapshot("2026-07-31T10:00:00", 1000, {{.path = "C:\\A"}}));

    CHECK(store.apply_retention() >= 1);

    const auto restantes = store.list();
    CHECK(restantes.size() == 2);

    // O mais recente nunca e descartado.
    CHECK(restantes.front().taken_at == "2026-07-31T10:00:00");
}

TEST_CASE("o banco informa o proprio tamanho", "[snapshot_store]") {
    const TempDatabase database;
    SnapshotStore store{database.path()};
    REQUIRE(store.ok());

    store.save(make_snapshot("2026-07-31T10:00:00", 1000, {{.path = "C:\\A"}}));

    CHECK(store.database_size_bytes() > 0);
}
