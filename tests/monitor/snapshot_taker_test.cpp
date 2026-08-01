#include <catch2/catch_test_macros.hpp>
#include <monitor/snapshot_taker.hpp>

#include "../scanner/temporary_tree.hpp"

#include <algorithm>
#include <string>

using cleaner::monitor::FolderSize;
using cleaner::monitor::SnapshotOptions;
using cleaner::monitor::SnapshotTaker;
using cleaner::monitor::accumulate_subtrees;
using cleaner::testing::TemporaryTree;

namespace {

constexpr std::uint64_t kMegabyte = 1024ULL * 1024ULL;

const FolderSize* find(const std::vector<FolderSize>& folders, const std::string& suffix) {
    const auto found = std::find_if(folders.begin(), folders.end(), [&suffix](const auto& folder) {
        return folder.path.size() >= suffix.size() &&
               folder.path.compare(folder.path.size() - suffix.size(), suffix.size(), suffix) == 0;
    });
    return found == folders.end() ? nullptr : &*found;
}

}

// O numero que o usuario quer ver e o da subarvore: "AppData\Local\Google esta
// ocupando 2 GB" inclui tudo que ha dentro, nao so os arquivos soltos ali.
TEST_CASE("o total de uma pasta inclui o que esta nas subpastas", "[snapshot_taker]") {
    const std::vector<FolderSize> direct{
        {.path = "C:\\raiz", .allocated_bytes = 100, .file_count = 1, .depth = 0},
        {.path = "C:\\raiz\\a", .allocated_bytes = 200, .file_count = 2, .depth = 1},
        {.path = "C:\\raiz\\a\\b", .allocated_bytes = 400, .file_count = 4, .depth = 2},
        {.path = "C:\\raiz\\c", .allocated_bytes = 50, .file_count = 1, .depth = 1},
    };

    const auto totals = accumulate_subtrees(direct);

    CHECK(find(totals, "raiz")->allocated_bytes == 750);
    CHECK(find(totals, "raiz\\a")->allocated_bytes == 600);
    CHECK(find(totals, "raiz\\a\\b")->allocated_bytes == 400);
    CHECK(find(totals, "raiz\\c")->allocated_bytes == 50);

    // A contagem de arquivos sobe junto.
    CHECK(find(totals, "raiz")->file_count == 8);
}

// Pasta intermediaria sem arquivo solto ainda e o caminho por onde a soma sobe.
TEST_CASE("pasta vazia no meio do caminho nao interrompe a soma", "[snapshot_taker]") {
    const std::vector<FolderSize> direct{
        {.path = "C:\\raiz", .allocated_bytes = 0, .depth = 0},
        {.path = "C:\\raiz\\vazia", .allocated_bytes = 0, .depth = 1},
        {.path = "C:\\raiz\\vazia\\cheia", .allocated_bytes = 900, .depth = 2},
    };

    const auto totals = accumulate_subtrees(direct);

    CHECK(find(totals, "raiz")->allocated_bytes == 900);
    CHECK(find(totals, "raiz\\vazia")->allocated_bytes == 900);
}

TEST_CASE("o retrato mede uma arvore de verdade", "[snapshot_taker]") {
    TemporaryTree tree;
    tree.add_file("dados/a.bin", 2 * kMegabyte);
    tree.add_file("dados/sub/b.bin", 3 * kMegabyte);
    tree.add_file("solto.bin", 1 * kMegabyte);

    const SnapshotTaker taker{SnapshotOptions{.always_keep_depth = 10,
                                              .deep_folder_threshold = 0,
                                              .tracked_file_threshold = 2 * kMegabyte}};

    const auto snapshot = taker.take(tree.root(), "C:");

    CHECK(snapshot.complete);

    const auto* raiz = find(snapshot.folders, tree.root().filename().string());
    REQUIRE(raiz != nullptr);
    CHECK(raiz->allocated_bytes >= 6 * kMegabyte);

    const auto* dados = find(snapshot.folders, "dados");
    REQUIRE(dados != nullptr);
    CHECK(dados->allocated_bytes >= 5 * kMegabyte);

    // Arquivos acima do limite ficam guardados individualmente: um so deles
    // pode explicar o crescimento de uma pasta inteira.
    CHECK(snapshot.files.size() == 2);
}

// O banco de retratos crescendo apareceria como consumo misterioso — o monitor
// acusando a si mesmo.
TEST_CASE("caminho excluido nao entra no retrato", "[snapshot_taker]") {
    TemporaryTree tree;
    tree.add_file("normal/a.bin", 1 * kMegabyte);
    tree.add_file("cleaner-dados/banco.sqlite", 4 * kMegabyte);

    const SnapshotTaker taker{
        SnapshotOptions{.always_keep_depth = 10,
                        .deep_folder_threshold = 0,
                        .excluded_paths = {(tree.root() / "cleaner-dados").string()}}};

    const auto snapshot = taker.take(tree.root(), "C:");

    CHECK(find(snapshot.folders, "cleaner-dados") == nullptr);

    const auto* raiz = find(snapshot.folders, tree.root().filename().string());
    REQUIRE(raiz != nullptr);
    CHECK(raiz->allocated_bytes < 3 * kMegabyte);
}

// Retrato interrompido nao pode passar por completo: comparado depois, mostraria
// como espaco liberado tudo que nao chegou a ser visitado.
TEST_CASE("retrato cancelado se declara incompleto", "[snapshot_taker]") {
    TemporaryTree tree;
    tree.add_file("a.bin", 1 * kMegabyte);

    std::stop_source source;
    source.request_stop();

    const SnapshotTaker taker;
    const auto snapshot = taker.take(tree.root(), "C:", source.get_token());

    CHECK_FALSE(snapshot.complete);
}

TEST_CASE("pastas fundas e pequenas ficam de fora", "[snapshot_taker]") {
    TemporaryTree tree;
    tree.add_file("n1/n2/n3/n4/n5/minusculo.bin", 1024);

    const SnapshotTaker taker{SnapshotOptions{.always_keep_depth = 2,
                                              .deep_folder_threshold = 10 * kMegabyte}};

    const auto snapshot = taker.take(tree.root(), "C:");

    CHECK(find(snapshot.folders, "n5") == nullptr);
    CHECK(find(snapshot.folders, "n1") != nullptr);
}
