#include <catch2/catch_test_macros.hpp>
#include <collectors/system_paths.hpp>

using zelo::collectors::SystemPaths;
using zelo::collectors::build_protected_paths;
using zelo::collectors::collect_system_paths;

namespace {

SystemPaths windows_like_paths() {
    return SystemPaths{
        .windows_directory = "C:\\Windows",
        .program_files = "C:\\Program Files",
        .program_files_x86 = "C:\\Program Files (x86)",
        .program_data = "C:\\ProgramData",
        .user_profile = "C:\\Users\\Joao",
        .drive_roots = {"C:\\", "D:\\"},
        .cloud_roots = {"C:\\Users\\Joao\\OneDrive"},
    };
}

}

TEST_CASE("diretorios criticos do Windows ficam protegidos", "[system_paths]") {
    const auto protected_paths = build_protected_paths(windows_like_paths());

    CHECK(protected_paths.is_protected("C:\\Windows"));
    CHECK(protected_paths.is_protected("C:\\Windows\\System32\\drivers\\etc\\hosts"));
    CHECK(protected_paths.is_protected("C:\\Program Files\\App"));
    CHECK(protected_paths.is_protected("C:\\Program Files (x86)\\App"));
    CHECK(protected_paths.is_protected("C:\\ProgramData\\App"));
    CHECK(protected_paths.is_protected("C:\\Users\\Joao"));
    CHECK(protected_paths.is_protected("C:\\"));
    CHECK(protected_paths.is_protected("D:\\"));
}

// O carve-out aprovado: o proprio Disk Cleanup limpa esta pasta, entao ela
// volta a ser tratada como normal mesmo estando dentro de uma raiz protegida.
TEST_CASE("a pasta de temporarios do Windows fica liberada", "[system_paths]") {
    const auto protected_paths = build_protected_paths(windows_like_paths());

    CHECK_FALSE(protected_paths.is_protected("C:\\Windows\\Temp"));
    CHECK_FALSE(protected_paths.is_protected("C:\\Windows\\Temp\\instalador.tmp"));
}

// O perfil do usuario e protegido na raiz, mas o conteudo dentro dele precisa
// continuar analisavel — senao o app nao poderia falar de Downloads nem de
// arquivos grandes, que sao o motivo principal do produto existir.
TEST_CASE("conteudo dentro do perfil do usuario continua analisavel", "[system_paths]") {
    const auto protected_paths = build_protected_paths(windows_like_paths());

    CHECK_FALSE(protected_paths.is_protected("C:\\Users\\Joao\\Downloads\\instalador.exe"));
    CHECK_FALSE(protected_paths.is_protected("C:\\Users\\Joao\\Videos\\gravacao.mkv"));
}

// Dentro de pasta de nuvem, apagar nao e uma operacao local: a exclusao viaja
// para a nuvem e para os outros dispositivos. O caminho para recuperar espaco
// ali e "liberar espaco local", que esvazia o conteudo e mantem o arquivo.
TEST_CASE("pasta de nuvem inteira fica protegida", "[system_paths]") {
    const auto protected_paths = build_protected_paths(windows_like_paths());

    CHECK(protected_paths.is_protected("C:\\Users\\Joao\\OneDrive"));
    CHECK(protected_paths.is_protected("C:\\Users\\Joao\\OneDrive\\Documentos\\contrato.pdf"));

    // Pasta de nome parecido, fora da sincronizacao, continua analisavel.
    CHECK_FALSE(protected_paths.is_protected("C:\\Users\\Joao\\OneDriveAntigo\\copia.pdf"));
}

TEST_CASE("os caminhos do sistema sao lidos do Windows", "[system_paths][integration]") {
    const SystemPaths paths = collect_system_paths();

    CHECK_FALSE(paths.windows_directory.empty());
    CHECK_FALSE(paths.program_files.empty());
    CHECK_FALSE(paths.user_profile.empty());
    CHECK_FALSE(paths.drive_roots.empty());

    // Montar a deny-list a partir do que foi lido nao pode lancar: uma excecao
    // invalida derrubaria o aplicativo na inicializacao.
    REQUIRE_NOTHROW(build_protected_paths(paths));

    const auto protected_paths = build_protected_paths(paths);

    INFO("windows: " << paths.windows_directory);
    INFO("perfil: " << paths.user_profile);

    CHECK(protected_paths.is_protected(paths.windows_directory + "\\System32"));
    CHECK(protected_paths.is_protected(paths.user_profile));
    CHECK_FALSE(protected_paths.is_protected(paths.windows_directory + "\\Temp\\algo.tmp"));
    CHECK_FALSE(protected_paths.is_protected(paths.user_profile + "\\Downloads\\algo.exe"));
}
