#include <catch2/catch_test_macros.hpp>
#include <core/models/cleanup_plan.hpp>
#include <core/risk/protected_paths.hpp>
#include <storage/cleanup_service.hpp>
#include <storage/quarantine_store.hpp>

#include <fstream>
#include <string>

using cleaner::core::CleanupItem;
using cleaner::core::CleanupPlan;
using cleaner::core::ProtectedPaths;
using cleaner::storage::CleanupService;
using cleaner::storage::QuarantineStore;
using cleaner::storage::RemovalMode;

namespace {

class Sandbox {
public:
    Sandbox() {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
        std::filesystem::create_directories(root_ / "temp", error);
        std::filesystem::create_directories(root_ / "sistema", error);
    }

    ~Sandbox() {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    Sandbox(const Sandbox&) = delete;
    Sandbox& operator=(const Sandbox&) = delete;
    Sandbox(Sandbox&&) = delete;
    Sandbox& operator=(Sandbox&&) = delete;

    [[nodiscard]] const std::filesystem::path& root() const { return root_; }

    std::filesystem::path make_file(const std::string& relative, std::size_t bytes) const {
        const auto path = root_ / relative;

        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << std::string(bytes, 'z');
        return path;
    }

    [[nodiscard]] ProtectedPaths paths() const {
        cleaner::core::ProtectedPathsSpec spec;
        spec.subtree_roots = {(root_ / "sistema").string()};
        return ProtectedPaths{spec};
    }

    [[nodiscard]] QuarantineStore quarantine() const {
        return QuarantineStore{root_ / "quarentena", paths()};
    }

private:
    std::filesystem::path root_ =
        std::filesystem::temp_directory_path() / ("cleaner-limpeza-" + std::to_string(counter()));

    static int counter() {
        static int value = 0;
        return value++;
    }
};

}

TEST_CASE("o plano lista o que sera removido antes de mexer em nada", "[cleanup]") {
    const Sandbox sandbox;
    const CleanupService service{sandbox.quarantine(), sandbox.paths()};

    const auto primeiro = sandbox.make_file("temp/a.tmp", 100);
    const auto segundo = sandbox.make_file("temp/b.tmp", 250);

    const CleanupPlan plan = service.plan({primeiro.string(), segundo.string()}, "temporarios");

    CHECK(plan.items.size() == 2);
    CHECK(plan.total_bytes() == 350);

    // Planejar nao altera nada.
    CHECK(std::filesystem::exists(primeiro));
    CHECK(std::filesystem::exists(segundo));
}

// Defesa em profundidade: mesmo que uma regra erre, o plano recusa caminho
// protegido — e diz que recusou, em vez de omitir.
TEST_CASE("o plano recusa caminho protegido e informa", "[cleanup]") {
    const Sandbox sandbox;
    const CleanupService service{sandbox.quarantine(), sandbox.paths()};

    const auto comum = sandbox.make_file("temp/a.tmp", 10);
    const auto protegido = sandbox.make_file("sistema/importante.dll", 10);

    const CleanupPlan plan = service.plan({comum.string(), protegido.string()}, "temporarios");

    REQUIRE(plan.items.size() == 1);
    CHECK(plan.items.front().path == comum.string());
    CHECK(plan.rejected.size() == 1);
}

TEST_CASE("executar remove os arquivos e permite desfazer", "[cleanup]") {
    const Sandbox sandbox;
    const auto quarantine = sandbox.quarantine();
    const CleanupService service{quarantine, sandbox.paths()};

    const auto primeiro = sandbox.make_file("temp/a.tmp", 100);
    const auto segundo = sandbox.make_file("temp/b.tmp", 250);

    const CleanupPlan plan = service.plan({primeiro.string(), segundo.string()}, "temporarios");
    const auto outcome = service.execute(plan, RemovalMode::Quarantine);

    CHECK(outcome.removed_count == 2);
    CHECK(outcome.freed_bytes == 350);
    CHECK_FALSE(std::filesystem::exists(primeiro));
    CHECK_FALSE(std::filesystem::exists(segundo));

    // Tudo que saiu esta na quarentena e volta.
    REQUIRE(outcome.quarantine_ids.size() == 2);
    for (const auto& id : outcome.quarantine_ids) {
        CHECK(quarantine.restore(id));
    }

    CHECK(std::filesystem::exists(primeiro));
    CHECK(std::filesystem::exists(segundo));
}

// O plano e montado antes e executado depois. Entre um e outro o disco muda, e
// a verificacao no momento de agir e o que impede a limpeza de remover algo que
// entrou num caminho protegido nesse intervalo.
TEST_CASE("executar revalida a protecao, nao confia no plano", "[cleanup]") {
    const Sandbox sandbox;
    const CleanupService service{sandbox.quarantine(), sandbox.paths()};

    const auto arquivo = sandbox.make_file("temp/a.tmp", 100);

    CleanupPlan plan;
    plan.items.push_back(CleanupItem{.path = (sandbox.root() / "sistema" / "forjado.dll").string(),
                                     .size_bytes = 10,
                                     .recommendation_id = "forjado"});

    const auto protegido = sandbox.make_file("sistema/forjado.dll", 10);
    const auto outcome = service.execute(plan, RemovalMode::Delete);

    CHECK(outcome.removed_count == 0);
    CHECK(outcome.skipped.size() == 1);
    CHECK(std::filesystem::exists(protegido));
    CHECK(std::filesystem::exists(arquivo));
}

TEST_CASE("plano de pasta inclui tudo que esta dentro dela", "[cleanup]") {
    const Sandbox sandbox;
    const CleanupService service{sandbox.quarantine(), sandbox.paths()};

    sandbox.make_file("cache/a.bin", 100);
    sandbox.make_file("cache/sub/b.bin", 200);
    sandbox.make_file("cache/sub/mais/c.bin", 300);
    sandbox.make_file("fora/d.bin", 999);

    const CleanupPlan plan =
        service.plan_folder((sandbox.root() / "cache").string(), "cache-de-teste");

    CHECK(plan.items.size() == 3);
    CHECK(plan.total_bytes() == 600);

    for (const auto& item : plan.items) {
        CHECK(item.path.find("fora") == std::string::npos);
    }
}

// A pasta continua existindo depois da limpeza. Removida, o programa dono
// poderia falhar ao procura-la em vez de simplesmente recria-la.
TEST_CASE("limpar uma pasta esvazia o conteudo mas mantem a pasta", "[cleanup]") {
    const Sandbox sandbox;
    const CleanupService service{sandbox.quarantine(), sandbox.paths()};

    const auto arquivo = sandbox.make_file("cache/a.bin", 100);
    const auto pasta = sandbox.root() / "cache";

    const auto outcome =
        service.execute(service.plan_folder(pasta.string(), "cache-de-teste"), RemovalMode::Delete);

    CHECK(outcome.removed_count == 1);
    CHECK_FALSE(std::filesystem::exists(arquivo));
    CHECK(std::filesystem::is_directory(pasta));
}

TEST_CASE("plano de pasta protegida sai vazio", "[cleanup]") {
    const Sandbox sandbox;
    const CleanupService service{sandbox.quarantine(), sandbox.paths()};

    sandbox.make_file("sistema/importante.dll", 100);

    const CleanupPlan plan =
        service.plan_folder((sandbox.root() / "sistema").string(), "nao-deveria-acontecer");

    CHECK(plan.items.empty());
    CHECK_FALSE(plan.rejected.empty());
    CHECK(std::filesystem::exists(sandbox.root() / "sistema" / "importante.dll"));
}

TEST_CASE("plano de pasta inexistente sai vazio sem quebrar", "[cleanup]") {
    const Sandbox sandbox;
    const CleanupService service{sandbox.quarantine(), sandbox.paths()};

    const CleanupPlan plan =
        service.plan_folder((sandbox.root() / "nao-existe").string(), "teste");

    CHECK(plan.items.empty());
}

TEST_CASE("plano vazio nao faz nada", "[cleanup]") {
    const Sandbox sandbox;
    const CleanupService service{sandbox.quarantine(), sandbox.paths()};

    const auto outcome = service.execute(CleanupPlan{}, RemovalMode::Delete);

    CHECK(outcome.removed_count == 0);
    CHECK(outcome.freed_bytes == 0);
    CHECK(outcome.quarantine_ids.empty());
}

TEST_CASE("arquivo que sumiu entre o plano e a execucao nao quebra a limpeza", "[cleanup]") {
    const Sandbox sandbox;
    const CleanupService service{sandbox.quarantine(), sandbox.paths()};

    const auto some = sandbox.make_file("temp/some.tmp", 50);
    const auto fica = sandbox.make_file("temp/fica.tmp", 70);

    const CleanupPlan plan = service.plan({some.string(), fica.string()}, "temporarios");

    std::error_code error;
    std::filesystem::remove(some, error);

    const auto outcome = service.execute(plan, RemovalMode::Delete);

    CHECK(outcome.removed_count == 1);
    CHECK(outcome.freed_bytes == 70);
    CHECK(outcome.skipped.size() == 1);
}

// O botao promete liberar espaco. Guardar copia numa pasta do mesmo disco
// ocuparia exatamente o que se queria liberar, e a promessa seria falsa.
TEST_CASE("apagar libera espaco de verdade, sem copia na quarentena", "[cleanup]") {
    const Sandbox sandbox;
    const auto quarantine = sandbox.quarantine();
    const CleanupService service{quarantine, sandbox.paths()};

    const auto arquivo = sandbox.make_file("cache/grande.bin", 4096);

    const auto outcome =
        service.execute(service.plan({arquivo.string()}, "cache"), RemovalMode::Delete);

    CHECK(outcome.removed_count == 1);
    CHECK(outcome.freed_bytes == 4096);
    CHECK_FALSE(std::filesystem::exists(arquivo));

    // Nada foi guardado: nem entrada no registro, nem copia no disco.
    CHECK(quarantine.entries().empty());
    CHECK(outcome.quarantine_ids.empty());

    std::error_code error;
    const auto guardados = std::filesystem::exists(sandbox.root() / "quarentena" / "arquivos", error)
                               ? std::distance(std::filesystem::directory_iterator(
                                                   sandbox.root() / "quarentena" / "arquivos"),
                                               std::filesystem::directory_iterator{})
                               : 0;
    CHECK(guardados == 0);
}
