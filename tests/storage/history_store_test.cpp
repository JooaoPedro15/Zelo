#include <catch2/catch_test_macros.hpp>
#include <storage/history_store.hpp>

#include <fstream>
#include <string>

using cleaner::core::Confidence;
using cleaner::core::Evidence;
using cleaner::core::HealthCategory;
using cleaner::core::HealthDeduction;
using cleaner::core::HealthScore;
using cleaner::core::Recommendation;
using cleaner::core::RiskLevel;
using cleaner::storage::HistoryStore;
using cleaner::storage::StoredSession;

namespace {

/// Diretorio descartavel para nao encostar no historico real da maquina.
class TemporaryDirectory {
public:
    TemporaryDirectory() {
        static int counter = 0;
        path_ = std::filesystem::temp_directory_path() /
                ("cleaner-history-" + std::to_string(counter++));

        std::error_code error;
        std::filesystem::remove_all(path_, error);
        std::filesystem::create_directories(path_, error);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    TemporaryDirectory(TemporaryDirectory&&) = delete;
    TemporaryDirectory& operator=(TemporaryDirectory&&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

StoredSession a_session(const std::string& id) {
    StoredSession session;
    session.id = id;
    session.started_at = "2026-07-27T14:30:00";
    session.app_version = "0.1.0";

    session.result.recommendations.push_back(Recommendation{
        .id = "storage.low-free-space",
        .rule_id = "storage.low-free-space",
        .rule_version = 1,
        .title = "Pouco espaco livre no disco C:",
        .risk = RiskLevel::Green,
        .confidence = Confidence::from_signals({{"espaco medido no volume", 0.95}}),
        .evidence = {Evidence{.source = "volume C:", .description = "espaco livre", .value = "4%"}},
        .limitations = "Nao diz sozinho que o computador esta lento.",
    });
    session.result.health = HealthScore::from_deductions(
        {HealthDeduction{HealthCategory::Storage, 10, "disco C: com pouco espaco"}});

    return session;
}

}

TEST_CASE("a sessao gravada e lida de volta igual", "[history_store]") {
    TemporaryDirectory directory;
    const HistoryStore store{directory.path()};

    store.save(a_session("2026-07-27_1430_quick"));

    const auto loaded = store.load("2026-07-27_1430_quick");

    REQUIRE(loaded.has_value());
    CHECK(loaded->app_version == "0.1.0");
    REQUIRE(loaded->result.recommendations.size() == 1);
    CHECK(loaded->result.recommendations.front().title == "Pouco espaco livre no disco C:");
    CHECK(loaded->result.health.of(HealthCategory::Storage) == 90);
}

TEST_CASE("as sessoes vem da mais recente para a mais antiga", "[history_store]") {
    TemporaryDirectory directory;
    const HistoryStore store{directory.path()};

    store.save(a_session("2026-07-25_0900_quick"));
    store.save(a_session("2026-07-27_1430_quick"));
    store.save(a_session("2026-07-26_1200_quick"));

    const auto sessions = store.load_all();

    REQUIRE(sessions.size() == 3);
    CHECK(sessions.at(0).id == "2026-07-27_1430_quick");
    CHECK(sessions.at(1).id == "2026-07-26_1200_quick");
    CHECK(sessions.at(2).id == "2026-07-25_0900_quick");
}

// Gravar por cima nao pode deixar sobra da versao anterior no arquivo.
TEST_CASE("regravar a mesma sessao substitui o arquivo inteiro", "[history_store]") {
    TemporaryDirectory directory;
    const HistoryStore store{directory.path()};

    StoredSession session = a_session("2026-07-27_1430_quick");
    for (int index = 0; index < 20; ++index) {
        session.result.recommendations.push_back(session.result.recommendations.front());
    }
    store.save(session);

    store.save(a_session("2026-07-27_1430_quick"));

    const auto loaded = store.load("2026-07-27_1430_quick");
    REQUIRE(loaded.has_value());
    CHECK(loaded->result.recommendations.size() == 1);
}

// Historico estragado nao pode impedir o aplicativo de abrir nem de gravar a
// proxima analise.
TEST_CASE("arquivo ilegivel e posto de quarentena sem derrubar a leitura",
          "[history_store]") {
    TemporaryDirectory directory;
    const HistoryStore store{directory.path()};

    store.save(a_session("2026-07-27_1430_quick"));

    std::ofstream broken(directory.path() / "2026-07-26_1200_quick.json", std::ios::binary);
    broken << "{ isto nao e json valido";
    broken.close();

    CHECK(store.load_all().size() == 1);
    CHECK(store.quarantine_unreadable() == 1);

    CHECK(std::filesystem::exists(directory.path() / "2026-07-26_1200_quick.corrupt"));
    CHECK_FALSE(std::filesystem::exists(directory.path() / "2026-07-26_1200_quick.json"));
    CHECK(store.load_all().size() == 1);
}

TEST_CASE("a retencao mantem apenas as sessoes mais recentes", "[history_store]") {
    TemporaryDirectory directory;
    const HistoryStore store{directory.path()};

    for (int day = 1; day <= 6; ++day) {
        store.save(a_session("2026-07-0" + std::to_string(day) + "_1000_quick"));
    }

    store.apply_retention(3);

    const auto sessions = store.load_all();
    REQUIRE(sessions.size() == 3);
    CHECK(sessions.at(0).id == "2026-07-06_1000_quick");
    CHECK(sessions.at(2).id == "2026-07-04_1000_quick");
}

TEST_CASE("a gravacao nao deixa arquivo temporario para tras", "[history_store]") {
    TemporaryDirectory directory;
    const HistoryStore store{directory.path()};

    store.save(a_session("2026-07-27_1430_quick"));

    for (const auto& entry : std::filesystem::directory_iterator(directory.path())) {
        CHECK(entry.path().extension() != ".tmp");
    }
}

// O programa se chamou Zelo antes de se chamar Cleaner. Sem adotar a pasta
// antiga, o historico do usuario continuaria no disco e sumiria da tela.
TEST_CASE("a pasta de dados do nome anterior e adotada", "[history_store]") {
    TemporaryDirectory base;

    const auto antiga = base.path() / "Zelo";
    std::filesystem::create_directories(antiga / "monitor");
    std::ofstream{antiga / "monitor" / "retratos.sqlite"} << "dados";

    CHECK(cleaner::storage::adopt_previous_data_directory(base.path(), "Zelo", "Cleaner"));

    CHECK_FALSE(std::filesystem::exists(antiga));
    CHECK(std::filesystem::exists(base.path() / "Cleaner" / "monitor" / "retratos.sqlite"));
}

// Mesclar dois historicos produziria um terceiro que nunca aconteceu.
TEST_CASE("com a pasta nova ja em uso, a antiga fica onde esta", "[history_store]") {
    TemporaryDirectory base;

    std::filesystem::create_directories(base.path() / "Zelo");
    std::filesystem::create_directories(base.path() / "Cleaner");

    CHECK_FALSE(cleaner::storage::adopt_previous_data_directory(base.path(), "Zelo", "Cleaner"));
    CHECK(std::filesystem::exists(base.path() / "Zelo"));
}

TEST_CASE("sem pasta antiga nao ha nada a adotar", "[history_store]") {
    TemporaryDirectory base;

    CHECK_FALSE(cleaner::storage::adopt_previous_data_directory(base.path(), "Zelo", "Cleaner"));
}

TEST_CASE("sessao inexistente devolve vazio em vez de lancar", "[history_store]") {
    TemporaryDirectory directory;
    const HistoryStore store{directory.path()};

    CHECK_FALSE(store.load("nao-existe").has_value());
    CHECK(store.load_all().empty());
}
