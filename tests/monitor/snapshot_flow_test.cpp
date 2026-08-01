#include <catch2/catch_test_macros.hpp>
#include <monitor/growth_report.hpp>
#include <monitor/snapshot_store.hpp>
#include <monitor/snapshot_taker.hpp>

#include "../scanner/temporary_tree.hpp"

#include <algorithm>
#include <string>

using cleaner::monitor::SnapshotOptions;
using cleaner::monitor::SnapshotStore;
using cleaner::monitor::SnapshotTaker;
using cleaner::monitor::build_growth_report;
using cleaner::testing::TemporaryTree;

namespace {

constexpr std::uint64_t kMegabyte = 1024ULL * 1024ULL;

}

// O caso que o usuario descreveu, em miniatura: tira um retrato, o disco cresce,
// tira outro e o relatorio aponta onde foi parar o espaco.
TEST_CASE("dois retratos apontam a pasta que cresceu", "[snapshot_flow]") {
    TemporaryTree tree;
    tree.add_file("estavel/base.bin", 1 * kMegabyte);
    tree.add_file("cresce/inicial.bin", 1 * kMegabyte);

    const auto database = tree.root() / "banco" / "retratos.sqlite";
    SnapshotStore store{database};
    REQUIRE(store.ok());

    const SnapshotTaker taker{SnapshotOptions{
        .always_keep_depth = 10,
        .deep_folder_threshold = 0,
        .excluded_paths = {(tree.root() / "banco").string()},
    }};

    auto antes = taker.take(tree.root(), "C:");
    antes.taken_at = "2026-07-30T10:00:00";
    const auto antes_id = store.save(antes);
    REQUIRE(antes_id > 0);

    // O crescimento acontece numa pasta so, acima do piso de ruido do relatorio.
    tree.add_file("cresce/novo.bin", 30 * kMegabyte);

    auto depois = taker.take(tree.root(), "C:");
    depois.taken_at = "2026-07-31T10:00:00";
    const auto depois_id = store.save(depois);
    REQUIRE(depois_id > 0);

    const auto diff = store.compare(antes_id, depois_id);
    REQUIRE(diff.has_value());

    const auto report = build_growth_report(*diff);

    REQUIRE_FALSE(report.items.empty());

    // A pasta que cresceu aparece em primeiro lugar, com o tamanho certo.
    const auto& maior = report.items.front();
    CHECK(maior.path.find("cresce") != std::string::npos);
    CHECK(maior.exclusive_bytes >= static_cast<std::int64_t>(30 * kMegabyte));

    // A pasta que nao mudou nao entra no relatorio.
    const bool estavel_listada =
        std::any_of(report.items.begin(), report.items.end(), [](const auto& item) {
            return item.path.find("estavel") != std::string::npos;
        });
    CHECK_FALSE(estavel_listada);
}

// O banco de retratos guarda dados a cada varredura. Sem exclui-lo, ele
// apareceria como pasta que cresceu — o monitor acusando a si mesmo.
TEST_CASE("o banco de retratos nao aparece como crescimento", "[snapshot_flow]") {
    TemporaryTree tree;
    tree.add_file("normal/a.bin", 1 * kMegabyte);

    const auto database = tree.root() / "banco" / "retratos.sqlite";
    SnapshotStore store{database};
    REQUIRE(store.ok());

    const SnapshotTaker taker{SnapshotOptions{
        .always_keep_depth = 10,
        .deep_folder_threshold = 0,
        .excluded_paths = {(tree.root() / "banco").string()},
    }};

    auto antes = taker.take(tree.root(), "C:");
    antes.taken_at = "2026-07-30T10:00:00";
    const auto antes_id = store.save(antes);

    auto depois = taker.take(tree.root(), "C:");
    depois.taken_at = "2026-07-31T10:00:00";
    const auto depois_id = store.save(depois);

    const auto diff = store.compare(antes_id, depois_id);
    REQUIRE(diff.has_value());

    const bool banco_listado =
        std::any_of(diff->changes.begin(), diff->changes.end(), [](const auto& change) {
            return change.path.find("banco") != std::string::npos;
        });
    CHECK_FALSE(banco_listado);
}
