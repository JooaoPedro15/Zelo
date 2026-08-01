#include <catch2/catch_test_macros.hpp>
#include <commands/command_catalog.hpp>
#include <commands/command_runner.hpp>

#include <algorithm>
#include <set>

using cleaner::commands::CommandId;
using cleaner::commands::CommandOutcome;
using cleaner::commands::CommandRequest;
using cleaner::commands::command_by_id;
using cleaner::commands::command_catalog;
using cleaner::commands::resolve_system_executable;
using cleaner::commands::run_official_command;
using cleaner::commands::running_elevated;

TEST_CASE("todo comando do catalogo se descreve", "[commands]") {
    for (const auto& command : command_catalog()) {
        CHECK_FALSE(command.name.empty());
        CHECK_FALSE(command.purpose.empty());
        CHECK_FALSE(command.executable.empty());

        // Quem altera o computador precisa dizer o que o usuario perde. Sem isso
        // nao existe consentimento informado, so um botao.
        if (command.modifies_system) {
            CHECK_FALSE(command.irreversible_effect.empty());
        }
    }
}

TEST_CASE("nao ha comando repetido nem identificador orfao", "[commands]") {
    std::set<CommandId> seen;
    for (const auto& command : command_catalog()) {
        CHECK(seen.insert(command.id).second);
        CHECK(command_by_id(command.id).name == command.name);
    }

    CHECK(seen.size() == command_catalog().size());
}

// O executavel vem do System32 por caminho absoluto. Trocar um programa de mesmo
// nome no PATH nao muda o que roda.
TEST_CASE("os executaveis vem do diretorio do sistema", "[commands]") {
    for (const auto& command : command_catalog()) {
        const auto path = resolve_system_executable(command.executable);
        INFO(command.executable);
        REQUIRE_FALSE(path.empty());
        CHECK(path.find(L":\\") != std::wstring::npos);
    }
}

TEST_CASE("nome fora do sistema nao vira caminho", "[commands]") {
    CHECK(resolve_system_executable("programa_que_nao_existe_cleaner.exe").empty());

    // Subir de diretorio precisa falhar mesmo se alguem passar isso adiante.
    CHECK(resolve_system_executable("..\\..\\Windows\\notepad.exe.nao").empty());
}

TEST_CASE("comando que altera o sistema nao roda sem confirmacao", "[commands]") {
    const auto result =
        run_official_command(CommandRequest{.id = CommandId::CleanupComponentStore});

    REQUIRE(result.outcome == CommandOutcome::Refused);
    CHECK(result.output.empty());
    CHECK_FALSE(result.explanation.empty());
}

// Sem privilegio, o DISM falha com mensagem obscura. Dizer isso antes de tentar
// e melhor do que traduzir um erro depois.
TEST_CASE("falta de privilegio e avisada antes de executar", "[commands]") {
    const auto result = run_official_command(CommandRequest{.id = CommandId::ListShadowStorage});

    if (running_elevated()) {
        CHECK(result.outcome != CommandOutcome::NeedsElevation);
    } else {
        REQUIRE(result.outcome == CommandOutcome::NeedsElevation);
        CHECK(result.output.empty());
    }
}

// O `compact` responde 102 para "nao esta comprimido". E resposta, nao erro:
// tratar como falha faria o Cleaner inventar um problema que o Windows nao relatou.
TEST_CASE("consulta sem privilegio executa de verdade", "[commands]") {
    const auto result = run_official_command(CommandRequest{.id = CommandId::QueryCompactOs});

    REQUIRE(result.outcome == CommandOutcome::Success);
    CHECK((result.exit_code == 0 || result.exit_code == 102));
    CHECK_FALSE(result.output.empty());
    CHECK_FALSE(result.explanation.empty());
}

TEST_CASE("codigo de saida conhecido troca a explicacao", "[commands]") {
    const auto& comando = command_by_id(CommandId::QueryCompactOs);
    const auto resposta = run_official_command(CommandRequest{.id = CommandId::QueryCompactOs});

    const auto esperado =
        std::find_if(comando.exit_meanings.begin(), comando.exit_meanings.end(),
                     [&](const auto& entry) { return entry.code == resposta.exit_code; });

    REQUIRE(esperado != comando.exit_meanings.end());
    CHECK(resposta.explanation == esperado->explanation);
}
