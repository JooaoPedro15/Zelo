#include <catch2/catch_test_macros.hpp>
#include <core/risk/protected_paths.hpp>
#include <storage/quarantine_store.hpp>

#include <fstream>
#include <string>

using cleaner::core::ProtectedPaths;
using cleaner::storage::QuarantineStore;

namespace {

/// Diretorio descartavel para cada teste. Codigo que move e apaga arquivo so
/// pode ser exercitado contra arquivos criados para isso.
class Sandbox {
public:
    Sandbox() {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
        std::filesystem::create_directories(root_ / "origem", error);
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
    [[nodiscard]] std::filesystem::path quarantine() const { return root_ / "quarentena"; }

    std::filesystem::path make_file(const std::string& name, const std::string& content) const {
        const auto path = root_ / "origem" / name;
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << content;
        return path;
    }

    [[nodiscard]] static std::string read(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

private:
    std::filesystem::path root_ =
        std::filesystem::temp_directory_path() / ("cleaner-quarentena-" + std::to_string(counter()));

    static int counter() {
        static int value = 0;
        return value++;
    }
};

ProtectedPaths sandbox_paths(const Sandbox& sandbox) {
    cleaner::core::ProtectedPathsSpec spec;
    spec.subtree_roots = {(sandbox.root() / "sistema").string()};
    return ProtectedPaths{spec};
}

}

TEST_CASE("arquivo em quarentena sai da origem e pode voltar", "[quarantine]") {
    const Sandbox sandbox;
    const QuarantineStore store{sandbox.quarantine(), sandbox_paths(sandbox)};

    const auto file = sandbox.make_file("cache.tmp", "conteudo do arquivo");

    const auto entry = store.take(file, "cache conhecido");

    REQUIRE(entry.has_value());
    CHECK_FALSE(std::filesystem::exists(file));
    CHECK(entry->original_path == file.string());
    CHECK(entry->size_bytes == 19);

    REQUIRE(store.restore(entry->id));

    CHECK(std::filesystem::exists(file));
    CHECK(Sandbox::read(file) == "conteudo do arquivo");
}

// A quarentena e a rede de seguranca da limpeza. Se ela perder o conteudo, o
// desfazer vira promessa vazia.
TEST_CASE("o conteudo do arquivo sobrevive a ida e volta", "[quarantine]") {
    const Sandbox sandbox;
    const QuarantineStore store{sandbox.quarantine(), sandbox_paths(sandbox)};

    const std::string content(50000, 'x');
    const auto file = sandbox.make_file("grande.bin", content);

    const auto entry = store.take(file, "teste");
    REQUIRE(entry.has_value());
    REQUIRE(store.restore(entry->id));

    CHECK(Sandbox::read(file) == content);
}

// Defesa em profundidade: mesmo que uma regra erre e aponte para um caminho
// protegido, a quarentena recusa. A verificacao acontece no momento de agir,
// nao apenas quando a recomendacao foi criada.
TEST_CASE("quarentena recusa caminho protegido", "[quarantine]") {
    const Sandbox sandbox;
    const QuarantineStore store{sandbox.quarantine(), sandbox_paths(sandbox)};

    std::error_code error;
    std::filesystem::create_directories(sandbox.root() / "sistema", error);

    const auto protegido = sandbox.root() / "sistema" / "importante.dll";
    std::ofstream(protegido, std::ios::binary) << "nao pode sumir";

    CHECK_FALSE(store.take(protegido, "nao deveria acontecer").has_value());
    CHECK(std::filesystem::exists(protegido));
}

TEST_CASE("restaurar nao sobrescreve arquivo que voltou a existir", "[quarantine]") {
    const Sandbox sandbox;
    const QuarantineStore store{sandbox.quarantine(), sandbox_paths(sandbox)};

    const auto file = sandbox.make_file("recriado.tmp", "original");
    const auto entry = store.take(file, "teste");
    REQUIRE(entry.has_value());

    // O programa dono do arquivo o recriou enquanto ele estava em quarentena.
    std::ofstream(file, std::ios::binary) << "versao nova";

    CHECK_FALSE(store.restore(entry->id));
    CHECK(Sandbox::read(file) == "versao nova");
}

TEST_CASE("o registro da quarentena sobrevive a reabertura", "[quarantine]") {
    const Sandbox sandbox;
    const auto paths = sandbox_paths(sandbox);

    std::string id;
    {
        const QuarantineStore store{sandbox.quarantine(), paths};
        const auto entry = store.take(sandbox.make_file("a.tmp", "abc"), "teste");
        REQUIRE(entry.has_value());
        id = entry->id;
    }

    const QuarantineStore reaberto{sandbox.quarantine(), paths};

    const auto entries = reaberto.entries();
    REQUIRE(entries.size() == 1);
    CHECK(entries.front().id == id);
    CHECK(reaberto.restore(id));
}

// O perfil de quem usa o Cleaner costuma ter acento. Se o registro nao puder ser
// gravado nesse caminho, os arquivos saem do lugar sem que nada saiba de onde
// vieram — e o desfazer, que e a razao de a quarentena existir, se perde.
TEST_CASE("quarentena funciona em caminho com acento", "[quarantine]") {
    const auto root = std::filesystem::temp_directory_path() / "cleaner-quarentena-João-ação";

    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root / "origem", error);

    const auto file = root / "origem" / "cache.tmp";
    std::ofstream(file, std::ios::binary) << "conteudo";

    cleaner::core::ProtectedPathsSpec spec;
    spec.subtree_roots = {(root / "sistema").string()};
    const QuarantineStore store{root / "quarentena", ProtectedPaths{spec}};

    const auto entry = store.take(file, "teste com acento");

    REQUIRE(entry.has_value());
    CHECK_FALSE(std::filesystem::exists(file));

    // O registro precisa ter sobrevivido: sem ele nao ha como devolver.
    REQUIRE(store.entries().size() == 1);
    REQUIRE(store.restore(entry->id));
    CHECK(std::filesystem::exists(file));

    std::filesystem::remove_all(root, error);
}

// Sem registro nao ha desfazer, e sem desfazer a quarentena nao cumpre o que
// promete. Nesse caso e melhor deixar o arquivo onde esta.
TEST_CASE("arquivo nao sai do lugar se o registro nao puder ser gravado", "[quarantine]") {
    const Sandbox sandbox;
    const QuarantineStore store{sandbox.quarantine(), sandbox_paths(sandbox)};

    const auto file = sandbox.make_file("a.tmp", "conteudo");

    // Um arquivo no lugar do diretorio de registro impede a gravacao.
    std::error_code error;
    std::filesystem::remove_all(sandbox.quarantine(), error);
    std::filesystem::create_directories(sandbox.quarantine().parent_path(), error);
    std::ofstream(sandbox.quarantine(), std::ios::binary) << "bloqueio";

    CHECK_FALSE(store.take(file, "teste").has_value());
    CHECK(std::filesystem::exists(file));
}

TEST_CASE("arquivo inexistente nao entra em quarentena", "[quarantine]") {
    const Sandbox sandbox;
    const QuarantineStore store{sandbox.quarantine(), sandbox_paths(sandbox)};

    CHECK_FALSE(store.take(sandbox.root() / "origem" / "nao-existe.tmp", "teste").has_value());
}
