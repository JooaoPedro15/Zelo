#pragma once

#include "commands/command_catalog.hpp"

#include <chrono>
#include <string>

namespace cleaner::commands {

enum class CommandOutcome {
    /// Rodou e o Windows relatou sucesso.
    Success,

    /// Rodou e falhou. O codigo e a saida ficam no resultado.
    Failed,

    /// O Cleaner se recusou a executar. Sempre por regra propria, nunca por erro
    /// do Windows.
    Refused,

    /// Precisa de administrador e o Cleaner nao esta com esse privilegio.
    NeedsElevation,

    /// Passou do tempo. O processo continua rodando: ver `explanation`.
    TimedOut,

    /// Nem chegou a comecar.
    NotLaunched,
};

struct CommandResult {
    CommandOutcome outcome = CommandOutcome::NotLaunched;
    int exit_code = -1;

    /// A saida do comando, ja convertida do codigo de pagina do console.
    std::string output;

    /// O que aconteceu, em uma frase para o usuario ler.
    std::string explanation;
};

struct CommandRequest {
    CommandId id = CommandId::AnalyzeComponentStore;

    /// Comando que altera o computador so roda com confirmacao explicita. O
    /// padrao falso faz com que esquecer de perguntar tenha o mesmo efeito de
    /// receber um nao.
    bool confirmed = false;

    std::chrono::seconds timeout{900};
};

/// Executa uma das limpezas oficiais.
///
/// O executavel sai do System32 por caminho absoluto e os argumentos vem do
/// catalogo: nada do que o usuario digita entra na linha de comando, e nenhum
/// interpretador de comandos participa.
[[nodiscard]] CommandResult run_official_command(const CommandRequest& request);

/// Caminho absoluto do executavel dentro do diretorio do sistema. Vazio quando
/// o arquivo nao existe ali.
[[nodiscard]] std::wstring resolve_system_executable(const std::string& executable);

/// Se o processo atual tem privilegio de administrador.
[[nodiscard]] bool running_elevated();

}
