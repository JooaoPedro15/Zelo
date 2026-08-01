#include <catch2/catch_test_macros.hpp>
#include <monitor/folder_watcher.hpp>

#include <chrono>
#include <fstream>
#include <string>
#include <thread>

using cleaner::monitor::FolderActivity;
using cleaner::monitor::FolderWatcher;

namespace {

class Sandbox {
public:
    Sandbox() {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
        std::filesystem::create_directories(root_, error);
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

    void write(const std::string& relative, const std::string& content) const {
        const auto path = root_ / relative;

        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << content;
    }

private:
    static int counter() {
        static int value = 0;
        return value++;
    }

    std::filesystem::path root_ =
        std::filesystem::temp_directory_path() / ("cleaner-watch-" + std::to_string(counter()));
};

/// Aviso do sistema de arquivos e assincrono: esperar um instante fixo daria um
/// teste que falha em maquina ocupada. Espera ate a condicao valer, com teto.
bool wait_until(const std::function<bool()>& condition,
                std::chrono::milliseconds limit = std::chrono::seconds{10}) {
    const auto deadline = std::chrono::steady_clock::now() + limit;

    while (std::chrono::steady_clock::now() < deadline) {
        if (condition()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }
    return condition();
}

const FolderActivity* find(const std::vector<FolderActivity>& activity,
                           const std::string& suffix) {
    const auto found =
        std::find_if(activity.begin(), activity.end(), [&suffix](const auto& entry) {
            return entry.folder.size() >= suffix.size() &&
                   entry.folder.compare(entry.folder.size() - suffix.size(), suffix.size(),
                                        suffix) == 0;
        });
    return found == activity.end() ? nullptr : &*found;
}

}

// A lacuna que o retrato nao cobre: comparar dois retratos revela que uma pasta
// cresceu, nao quando isso aconteceu.
TEST_CASE("escrever num diretorio observado gera atividade com hora", "[folder_watcher]") {
    const Sandbox sandbox;

    FolderWatcher watcher;
    REQUIRE(watcher.watch(sandbox.root()));

    sandbox.write("arquivo.txt", "conteudo");

    REQUIRE(wait_until([&watcher] { return !watcher.collect().empty(); }));

    const auto activity = watcher.collect();
    REQUIRE_FALSE(activity.empty());

    CHECK(activity.front().event_count > 0);
    CHECK_FALSE(activity.front().first_seen.empty());
    CHECK_FALSE(activity.front().last_seen.empty());
}

TEST_CASE("a observacao alcanca as subpastas", "[folder_watcher]") {
    const Sandbox sandbox;

    FolderWatcher watcher;
    REQUIRE(watcher.watch(sandbox.root()));

    sandbox.write("fundo/mais/fundo/arquivo.txt", "conteudo");

    REQUIRE(wait_until([&watcher] {
        return find(watcher.collect(), "fundo") != nullptr;
    }));
}

// Um programa gravando o mesmo log mil vezes e diferente de um que criou mil
// arquivos, e o relatorio precisa distinguir os dois.
TEST_CASE("arquivos distintos sao contados a parte dos eventos", "[folder_watcher]") {
    const Sandbox sandbox;

    FolderWatcher watcher;
    REQUIRE(watcher.watch(sandbox.root()));

    for (int index = 0; index < 5; ++index) {
        sandbox.write("mesmo.log", "escrita " + std::to_string(index));
    }

    REQUIRE(wait_until([&watcher] {
        const auto activity = watcher.collect();
        return !activity.empty() && activity.front().event_count >= 5;
    }));

    const auto activity = watcher.collect();
    REQUIRE_FALSE(activity.empty());

    CHECK(activity.front().distinct_files == 1);
    CHECK(activity.front().event_count >= 5);
}

// A memoria nao pode crescer enquanto o aplicativo fica aberto o dia todo.
TEST_CASE("esvaziar limpa o acumulado", "[folder_watcher]") {
    const Sandbox sandbox;

    FolderWatcher watcher;
    REQUIRE(watcher.watch(sandbox.root()));

    sandbox.write("a.txt", "conteudo");
    REQUIRE(wait_until([&watcher] { return !watcher.collect().empty(); }));

    CHECK_FALSE(watcher.drain().empty());
    CHECK(watcher.collect().empty());
}

TEST_CASE("pasta inexistente e recusada sem quebrar", "[folder_watcher]") {
    FolderWatcher watcher;
    CHECK_FALSE(watcher.watch("Z:\\nao-existe-mesmo"));
}

// Fechar o aplicativo nao pode ficar preso esperando a proxima escrita em disco.
TEST_CASE("parar retorna mesmo sem atividade nenhuma", "[folder_watcher]") {
    const Sandbox sandbox;

    FolderWatcher watcher;
    REQUIRE(watcher.watch(sandbox.root()));

    const auto started = std::chrono::steady_clock::now();
    watcher.stop();
    const auto elapsed = std::chrono::steady_clock::now() - started;

    CHECK(elapsed < std::chrono::seconds{3});
}
