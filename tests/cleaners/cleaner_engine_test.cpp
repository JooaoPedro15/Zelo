#include "../scanner/temporary_tree.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cleaners/cleaner_engine.hpp>

#include <algorithm>
#include <filesystem>

using cleaner::cleaners::CleanerEngine;
using cleaner::cleaners::available_cleaners;
using cleaner::core::CleanerSpec;
using cleaner::core::ContentClass;
using cleaner::core::ProtectedPaths;
using cleaner::core::ProtectedPathsSpec;
using cleaner::testing::TemporaryTree;

namespace {

CleanerSpec spec_for(const std::filesystem::path& root, std::vector<std::string> preserved = {}) {
    return CleanerSpec{
        .id = "teste",
        .display_name = "Limpador de teste",
        .description = "Existe so para o teste.",
        .allowed_roots = {root.string()},
        .preserved_names = std::move(preserved),
        .content_class = ContentClass::SafeToClean,
        .confidence = 0.9,
        .consequence = "Nada.",
    };
}

ProtectedPaths nothing_protected() {
    return ProtectedPaths{ProtectedPathsSpec{}};
}

}

TEST_CASE("a previa mede sem apagar", "[cleaner_engine]") {
    TemporaryTree tree;
    tree.add_file("a.bin", 5000);
    tree.add_file("sub/b.bin", 3000);

    const CleanerEngine engine{nothing_protected()};
    const auto preview = engine.preview(spec_for(tree.root()));

    CHECK(preview.file_count == 2);
    CHECK(preview.bytes == 8000);
    CHECK(preview.complete);

    // Nada saiu do disco.
    CHECK(std::filesystem::exists(tree.root() / "a.bin"));
    CHECK(std::filesystem::exists(tree.root() / "sub" / "b.bin"));
}

TEST_CASE("o que a declaracao manda preservar nao entra na previa", "[cleaner_engine]") {
    TemporaryTree tree;
    tree.add_file("cache.bin", 4000);
    tree.add_file("config.json", 500);

    const CleanerEngine engine{nothing_protected()};
    const auto preview = engine.preview(spec_for(tree.root(), {"config.json"}));

    CHECK(preview.file_count == 1);
    CHECK(preview.bytes == 4000);
}

TEST_CASE("a limpeza remove e relata o que removeu", "[cleaner_engine]") {
    TemporaryTree tree;
    tree.add_file("a.bin", 5000);
    tree.add_file("sub/b.bin", 3000);
    tree.add_file("guardar.txt", 100);

    const CleanerEngine engine{nothing_protected()};

    const auto preview = engine.preview(spec_for(tree.root(), {"guardar.txt"}));
    INFO("previa: " << preview.file_count << " arquivos, " << preview.bytes << " bytes");

    const auto outcome = engine.execute(spec_for(tree.root(), {"guardar.txt"}));
    INFO("removidos: " << outcome.removed_count << " ignorados: " << outcome.skipped_count
                       << " falhas: " << outcome.failed_count);
    for (const auto& reason : outcome.reasons) {
        INFO(reason);
    }

    CHECK(outcome.removed_count == 2);
    CHECK(outcome.bytes == 8000);
    CHECK(outcome.failed_count == 0);

    CHECK_FALSE(std::filesystem::exists(tree.root() / "a.bin"));
    CHECK(std::filesystem::exists(tree.root() / "guardar.txt"));
}

// A deny-list decide por cima da declaracao. Um limpador com a raiz certa e a
// deny-list contra ela nao apaga nada.
TEST_CASE("area protegida vence a declaracao do limpador", "[cleaner_engine]") {
    TemporaryTree tree;
    tree.add_file("a.bin", 5000);

    const CleanerEngine engine{
        ProtectedPaths{ProtectedPathsSpec{.subtree_roots = {tree.root().string()}}}};

    const auto preview = engine.preview(spec_for(tree.root()));
    CHECK(preview.file_count == 0);
    CHECK_FALSE(preview.rejected.empty());

    const auto outcome = engine.execute(spec_for(tree.root()));
    CHECK(outcome.removed_count == 0);
    CHECK(std::filesystem::exists(tree.root() / "a.bin"));
}

// O caso que a checagem final existe para cobrir: um caminho dentro da area
// permitida que na verdade aponta para fora dela.
TEST_CASE("link que aponta para fora da area nao e seguido", "[cleaner_engine]") {
    TemporaryTree inside;
    TemporaryTree outside;

    outside.add_file("importante.txt", 2000);
    inside.add_file("normal.bin", 1000);

    if (!inside.add_junction("atalho", outside.root())) {
        SUCCEED("o sistema nao permitiu criar a junction");
        return;
    }

    const CleanerEngine engine{nothing_protected()};
    const auto outcome = engine.execute(spec_for(inside.root()));

    // O arquivo de fora continua onde estava: a varredura nao atravessa link.
    CHECK(std::filesystem::exists(outside.root() / "importante.txt"));
    CHECK(outcome.removed_count == 1);
}

TEST_CASE("cancelar interrompe sem apagar o resto", "[cleaner_engine]") {
    TemporaryTree tree;
    tree.add_file("a.bin", 1000);

    std::stop_source source;
    source.request_stop();

    const CleanerEngine engine{nothing_protected()};
    const auto preview = engine.preview(spec_for(tree.root()), source.get_token());

    CHECK_FALSE(preview.complete);
    CHECK(std::filesystem::exists(tree.root() / "a.bin"));
}

TEST_CASE("raiz que nao existe nao vira erro", "[cleaner_engine]") {
    const CleanerEngine engine{nothing_protected()};

    auto spec = spec_for("C:\\pasta_que_nao_existe_do_cleaner");
    CHECK(engine.preview(spec).file_count == 0);
    CHECK(engine.execute(spec).removed_count == 0);
}

// Cada limpador que acompanha o programa precisa estar completo. Oferecer
// limpeza sem dizer o que sai e o que fica nao e consentimento informado.
TEST_CASE("todo limpador declarado se descreve por inteiro", "[cleaner_engine][integration]") {
    for (const auto& spec : available_cleaners()) {
        INFO(spec.id);

        CHECK_FALSE(spec.id.empty());
        CHECK_FALSE(spec.display_name.empty());
        CHECK_FALSE(spec.description.empty());
        CHECK_FALSE(spec.consequence.empty());
        CHECK_FALSE(spec.preserved_description.empty());
        CHECK_FALSE(spec.allowed_roots.empty());
        CHECK(spec.confidence > 0.0);
        CHECK(spec.rule_version > 0);

        // Nenhum limpador que acompanha o programa pode ser vermelho ou
        // desconhecido: acao automatica so existe para o que foi identificado.
        CHECK(spec.content_class != ContentClass::Protected);
        CHECK(spec.content_class != ContentClass::NeedsReview);

        for (const auto& root : spec.allowed_roots) {
            INFO(root);
            CHECK(std::filesystem::path(root).is_absolute());
        }
    }
}

// O perfil do navegador nao pode virar raiz permitida: senha, favorito e
// extensao moram nele.
TEST_CASE("o limpador de navegador so alcanca pastas de cache",
          "[cleaner_engine][integration]") {
    for (const auto& spec : available_cleaners()) {
        if (spec.id != "chrome.cache" && spec.id != "edge.cache") {
            continue;
        }

        for (const auto& root : spec.allowed_roots) {
            INFO(root);
            const auto leaf = std::filesystem::path(root).filename().string();
            CHECK((leaf == "Cache" || leaf == "Code Cache" || leaf == "GPUCache" ||
                   leaf == "ShaderCache"));
        }
    }
}
