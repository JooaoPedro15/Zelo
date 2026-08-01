#include <catch2/catch_test_macros.hpp>
#include <monitor/action_log.hpp>

#include <string>

using zelo::monitor::ActionKind;
using zelo::monitor::ActionLog;
using zelo::monitor::ActionRecord;

namespace {

class TempDatabase {
public:
    TempDatabase() {
        std::error_code error;
        std::filesystem::create_directories(path_.parent_path(), error);
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

    std::filesystem::path path_ = std::filesystem::temp_directory_path() /
                                  ("zelo-acoes-João-" + std::to_string(counter())) / "acoes.sqlite";
};

}

TEST_CASE("uma acao registrada volta com tudo que importa", "[action_log]") {
    const TempDatabase database;
    ActionLog log{database.path()};
    REQUIRE(log.ok());

    REQUIRE(log.record(ActionRecord{
                .kind = ActionKind::Deleted,
                .reason = "Cache de pacotes Python (uv)",
                .target = "C:\\Users\\João\\AppData\\Local\\uv\\cache",
                .item_count = 361384,
                .bytes = 11ULL * 1024 * 1024 * 1024,
                .skipped_count = 12,
                .reversible = false,
            }) > 0);

    const auto recent = log.recent();
    REQUIRE(recent.size() == 1);

    const auto& action = recent.front();
    CHECK(action.reason == "Cache de pacotes Python (uv)");
    CHECK(action.target == "C:\\Users\\João\\AppData\\Local\\uv\\cache");
    CHECK(action.item_count == 361384);
    CHECK(action.skipped_count == 12);
    CHECK_FALSE(action.at.empty());

    // O historico nunca promete devolver o que foi apagado de vez.
    CHECK_FALSE(action.reversible);
}

TEST_CASE("o historico e pesquisavel", "[action_log]") {
    const TempDatabase database;
    ActionLog log{database.path()};
    REQUIRE(log.ok());

    log.record(ActionRecord{.reason = "Cache do Chrome", .target = "C:\\Chrome\\Cache"});
    log.record(ActionRecord{.reason = "Cache do npm", .target = "C:\\npm-cache"});

    CHECK(log.search("chrome").size() == 1);
    CHECK(log.search("npm").size() == 1);
    CHECK(log.search("cache").size() == 2);
    CHECK(log.search("inexistente").empty());
}

// Restaurar devolve arquivo ao disco. Somar isso ao espaco liberado daria um
// numero que nunca aconteceu.
TEST_CASE("restauracao nao conta como espaco liberado", "[action_log]") {
    const TempDatabase database;
    ActionLog log{database.path()};
    REQUIRE(log.ok());

    log.record(ActionRecord{.kind = ActionKind::Deleted, .target = "C:\\a", .bytes = 1000});
    log.record(ActionRecord{.kind = ActionKind::Restored, .target = "C:\\b", .bytes = 400});

    CHECK(log.total_freed_bytes() == 1000);
}

TEST_CASE("o registro sobrevive a reabertura", "[action_log]") {
    const TempDatabase database;

    {
        ActionLog log{database.path()};
        REQUIRE(log.ok());
        log.record(ActionRecord{.reason = "teste", .target = "C:\\alvo", .bytes = 50});
    }

    const ActionLog reaberto{database.path()};
    REQUIRE(reaberto.ok());

    REQUIRE(reaberto.recent().size() == 1);
    CHECK(reaberto.recent().front().target == "C:\\alvo");
}
