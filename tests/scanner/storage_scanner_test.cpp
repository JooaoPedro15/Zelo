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

// Hard link e um segundo nome para o mesmo conteudo. Somar os dois faria o
// scanner relatar o dobro do espaco que existe de fato no disco.
TEST_CASE("hard link nao e contado duas vezes", "[scanner]") {
    TemporaryTree tree;

    // Acima do limite em que o scanner confere identidade — abaixo dele o
    // erro seria de kilobytes e nao compensaria abrir cada arquivo.
    const auto original = tree.add_file("dados.bin", 2 * 1024 * kKilobyte);

    if (!tree.add_hard_link("copia.bin", original)) {
        SUCCEED("o sistema recusou criar hard link; teste ignorado");
        return;
    }

    const StorageScanner scanner;
    const ScanResult result = scanner.scan(tree.root());

    // Dois nomes aparecem na varredura, mas o espaco ocupado e de um so.
    CHECK(result.file_count == 2);
    CHECK(result.hard_link_duplicates == 1);
    CHECK(result.allocated_bytes < 3 * 1024 * kKilobyte);
    CHECK(result.total_bytes == 4 * 1024 * kKilobyte);
}

// Arquivo esparso declara um tamanho grande e ocupa quase nada. Usar o tamanho
// declarado prometeria liberar espaco que nao existe.
TEST_CASE("arquivo esparso separa tamanho declarado de espaco ocupado", "[scanner]") {
    TemporaryTree tree;

    if (!tree.add_sparse_file("esparso.bin", 8 * 1024 * kKilobyte)) {
        SUCCEED("o sistema de arquivos nao suporta arquivo esparso; teste ignorado");
        return;
    }

    const StorageScanner scanner;
    const ScanResult result = scanner.scan(tree.root());

    CHECK(result.total_bytes >= 8 * 1024 * kKilobyte);
    CHECK(result.allocated_bytes < result.total_bytes / 2);
}

TEST_CASE("espaco ocupado considera o tamanho do cluster", "[scanner]") {
    TemporaryTree tree;

    // Dez arquivos minusculos: cada um ocupa um cluster inteiro, entao o espaco
    // ocupado e bem maior que a soma dos tamanhos.
    for (int index = 0; index < 10; ++index) {
        tree.add_file("mini/" + std::to_string(index) + ".txt", 10);
    }

    const StorageScanner scanner;
    const ScanResult result = scanner.scan(tree.root());

    CHECK(result.total_bytes == 100);
    CHECK(result.allocated_bytes > result.total_bytes);
}

TEST_CASE("as datas do arquivo sao coletadas", "[scanner]") {
    TemporaryTree tree;
    tree.add_file("grande.bin", 200 * kKilobyte);

    const StorageScanner scanner;
    const ScanResult result = scanner.scan(tree.root());

    REQUIRE(result.largest_files.size() == 1);

    const auto& file = result.largest_files.front();
    CHECK_FALSE(file.created_at.empty());
    CHECK_FALSE(file.modified_at.empty());

    // O horario do ultimo acesso so vale quando o Windows esta registrando isso;
    // exibi-lo sem a ressalva levaria o usuario a apagar por dado errado.
    if (file.last_access_reliable) {
        CHECK_FALSE(file.last_access_at.empty());
    }
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
