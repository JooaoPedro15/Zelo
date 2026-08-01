#include <catch2/catch_test_macros.hpp>
#include <collectors/running_apps.hpp>

#include <algorithm>

using zelo::collectors::is_running;
using zelo::collectors::running_apps;

TEST_CASE("os programas abertos sao listados", "[running_apps][integration]") {
    const auto apps = running_apps();

    REQUIRE_FALSE(apps.empty());

    for (const auto& app : apps) {
        CHECK_FALSE(app.executable.empty());
        CHECK(app.instances >= 1);
    }
}

// O proprio teste esta rodando, entao ele precisa se enxergar. Sem isso o
// aviso de "programa aberto" nunca dispararia e o usuario estranharia o espaco
// liberado a menos.
TEST_CASE("o proprio processo aparece como em execucao", "[running_apps][integration]") {
    CHECK(is_running("zelo_tests.exe"));
}

TEST_CASE("programa inexistente nao aparece", "[running_apps]") {
    CHECK_FALSE(is_running("programa-que-nao-existe-mesmo.exe"));
}

// Comparar so o comeco do nome faria "code.exe" casar com "codehelper.exe".
TEST_CASE("a comparacao exige o nome inteiro", "[running_apps]") {
    CHECK_FALSE(is_running("zelo_tes"));
    CHECK_FALSE(is_running("zelo_tests.exe.extra"));
}
