#include <catch2/catch_test_macros.hpp>
#include <collectors/startup_collector.hpp>

using cleaner::collectors::StartupCollector;
using cleaner::collectors::executable_from_command;
using cleaner::collectors::looks_essential;
using cleaner::core::SystemSnapshot;

// A heuristica erra de proposito para o lado de marcar demais: deixar de
// sugerir algo e bem menos grave do que sugerir desativar um antivirus.
TEST_CASE("componentes de seguranca e hardware sao reconhecidos", "[startup_collector]") {
    CHECK(looks_essential("SecurityHealth", "C:\\Windows\\System32\\SecurityHealthSystray.exe"));
    CHECK(looks_essential("Avast", "C:\\Program Files\\Avast\\ui.exe"));
    CHECK(looks_essential("RtkAudUService", "C:\\Windows\\RtkAudUService64.exe"));
    CHECK(looks_essential("NVIDIA", "C:\\Program Files\\NVIDIA Corporation\\nvcplui.exe"));
    CHECK(looks_essential("SynTPEnh", "C:\\Program Files\\Synaptics\\SynTP\\SynTPEnh.exe"));
}

TEST_CASE("o reconhecimento ignora a caixa das letras", "[startup_collector]") {
    CHECK(looks_essential("AVAST ANTIVIRUS", "C:\\qualquer.exe"));
    CHECK(looks_essential("qualquer", "C:\\PROGRAM FILES\\NVIDIA\\app.exe"));
}

TEST_CASE("programa comum nao e marcado como essencial", "[startup_collector]") {
    CHECK_FALSE(looks_essential("Spotify", "C:\\Users\\Joao\\AppData\\Roaming\\Spotify.exe"));
    CHECK_FALSE(looks_essential("Discord", "C:\\Users\\Joao\\AppData\\Local\\Discord\\app.exe"));
    CHECK_FALSE(looks_essential("Steam", "C:\\Program Files (x86)\\Steam\\steam.exe"));
}

// O registro guarda a linha de comando inteira. Sem separar o executavel, o
// caminho nao casaria com a deny-list nem serviria para abrir a pasta.
TEST_CASE("o executavel e separado dos argumentos", "[startup_collector]") {
    CHECK(executable_from_command(R"("C:\Program Files (x86)\Steam\steam.exe" -silent)") ==
          R"(C:\Program Files (x86)\Steam\steam.exe)");

    CHECK(executable_from_command(R"(C:\Program Files\Parsec\parsecd.exe app_silent=1)") ==
          R"(C:\Program Files\Parsec\parsecd.exe)");

    CHECK(executable_from_command(R"(C:\Windows\system32\SecurityHealthSystray.exe)") ==
          R"(C:\Windows\system32\SecurityHealthSystray.exe)");

    CHECK(executable_from_command("") .empty());
}

TEST_CASE("a inicializacao da maquina e lida sem erro", "[startup_collector][integration]") {
    const StartupCollector collector;
    const auto items = collector.collect();

    for (const auto& item : items) {
        INFO("item " << item.name << " -> " << item.path);
        CHECK_FALSE(item.name.empty());
    }

    SUCCEED("leitura concluida com " + std::to_string(items.size()) + " itens");
}

// Nenhum item na inicializacao e um resultado legitimo. O que a analise precisa
// saber e se deu para olhar, nao se achou algo.
TEST_CASE("a coleta marca disponibilidade mesmo sem itens", "[startup_collector][integration]") {
    const StartupCollector collector;

    SystemSnapshot snapshot;
    REQUIRE(collector.collect_into(snapshot));

    CHECK(snapshot.startup_available);
}
