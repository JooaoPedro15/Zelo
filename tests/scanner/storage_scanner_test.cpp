#include "temporary_tree.hpp"

#include <catch2/catch_test_macros.hpp>
#include <scanner/storage_scanner.hpp>

#include <stop_token>
#include <string>

using zelo::scanner::ScanResult;
using zelo::scanner::StorageScanner;
using zelo::testing::TemporaryTree;

namespace {

constexpr std::uint64_t kKilobyte = 1024;

}

TEST_CASE("varredura soma exatamente o tamanho de uma arvore conhecida", "[scanner]") {
    TemporaryTree tree;
    tree.add_file("a.bin", 10 * kKilobyte);
    tree.add_file("sub/b.bin", 20 * kKilobyte);
    tree.add_file("sub/mais/c.bin", 30 * kKilobyte);

    const StorageScanner scanner;
    const ScanResult result = scanner.scan(tree.root());

    CHECK(result.total_bytes == 60 * kKilobyte);
    CHECK(result.file_count == 3);
    CHECK(result.completed);
    CHECK(result.skipped_count == 0);
}

TEST_CASE("varredura de arvore vazia devolve zero sem falhar", "[scanner]") {
    TemporaryTree tree;

    const StorageScanner scanner;
    const ScanResult result = scanner.scan(tree.root());

    CHECK(result.total_bytes == 0);
    CHECK(result.file_count == 0);
    CHECK(result.completed);
}

// Duas varreduras da mesma arvore parada tem que dar o mesmo numero. Sem isso
// nao da para confiar em comparacao antes e depois de uma limpeza.
TEST_CASE("duas varreduras seguidas dao o mesmo resultado", "[scanner]") {
    TemporaryTree tree;
    tree.add_file("a.bin", 7 * kKilobyte);
    tree.add_file("sub/b.bin", 13 * kKilobyte);

    const StorageScanner scanner;

    CHECK(scanner.scan(tree.root()).total_bytes == scanner.scan(tree.root()).total_bytes);
}

// Junction cria uma segunda rota para o mesmo conteudo. Seguir uma delas
// contaria os bytes duas vezes e daria ao usuario um numero inflado.
TEST_CASE("junction nao e atravessada nem contada duas vezes", "[scanner]") {
    TemporaryTree tree;
    tree.add_file("real/dados.bin", 40 * kKilobyte);

    if (!tree.add_junction("atalho", tree.root() / "real")) {
        SUCCEED("sistema nao permitiu criar junction; teste ignorado");
        return;
    }

    const StorageScanner scanner;
    const ScanResult result = scanner.scan(tree.root());

    CHECK(result.total_bytes == 40 * kKilobyte);
    CHECK(result.file_count == 1);
}

// Junction apontando para um ancestral faz a arvore virar um ciclo. Sem a
// protecao a varredura giraria para sempre.
TEST_CASE("junction circular nao trava a varredura", "[scanner]") {
    TemporaryTree tree;
    tree.add_file("dados.bin", 5 * kKilobyte);
    tree.add_directory("sub");

    if (!tree.add_junction("sub/volta", tree.root())) {
        SUCCEED("sistema nao permitiu criar junction; teste ignorado");
        return;
    }

    const StorageScanner scanner;
    const ScanResult result = scanner.scan(tree.root());

    CHECK(result.total_bytes == 5 * kKilobyte);
    CHECK(result.completed);
}

// O limite historico de 260 caracteres do Windows nao pode virar um teto
// silencioso: um arquivo grande enterrado fundo tem que aparecer.
TEST_CASE("caminho acima de 260 caracteres e varrido", "[scanner]") {
    TemporaryTree tree;

    std::string deep;
    for (int level = 0; level < 20; ++level) {
        deep += "diretorio-com-nome-bem-comprido-" + std::to_string(level) + "/";
    }
    const std::string relative = deep + "arquivo.bin";
    REQUIRE(relative.size() > 260);

    tree.add_file(relative, 3 * kKilobyte);

    const StorageScanner scanner;
    const ScanResult result = scanner.scan(tree.root());

    CHECK(result.total_bytes == 3 * kKilobyte);
    CHECK(result.file_count == 1);
}

// Uma pasta ilegivel — permissao negada, disco removido no meio — nao pode
// derrubar a analise inteira. Ela e contada como pulada e a varredura segue.
TEST_CASE("diretorio ilegivel e contabilizado sem abortar", "[scanner]") {
    TemporaryTree tree;

    const StorageScanner scanner;
    const ScanResult result = scanner.scan(tree.root() / "nao-existe");

    CHECK(result.skipped_count == 1);
    CHECK(result.completed);
    CHECK(result.total_bytes == 0);
}

TEST_CASE("cancelamento interrompe e marca resultado como parcial", "[scanner]") {
    TemporaryTree tree;
    for (int index = 0; index < 40; ++index) {
        tree.add_file("sub" + std::to_string(index) + "/dados.bin", kKilobyte);
    }

    std::stop_source source;
    source.request_stop();

    const StorageScanner scanner;
    const ScanResult result = scanner.scan(tree.root(), source.get_token());

    CHECK_FALSE(result.completed);
}

TEST_CASE("maiores arquivos vem ordenados e limitados", "[scanner]") {
    TemporaryTree tree;
    tree.add_file("pequeno.bin", kKilobyte);
    tree.add_file("medio.bin", 10 * kKilobyte);
    tree.add_file("grande.bin", 50 * kKilobyte);

    const StorageScanner scanner{{.largest_files_kept = 2}};
    const ScanResult result = scanner.scan(tree.root());

    REQUIRE(result.largest_files.size() == 2);
    CHECK(result.largest_files.at(0).bytes == 50 * kKilobyte);
    CHECK(result.largest_files.at(1).bytes == 10 * kKilobyte);
}
