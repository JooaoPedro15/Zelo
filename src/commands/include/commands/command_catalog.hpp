#pragma once

#include <string>
#include <vector>

namespace zelo::commands {

/// As limpezas oficiais do Windows que o Zelo sabe acionar.
///
/// A lista e fechada em tempo de compilacao: nao existe caminho no programa
/// para montar um comando novo enquanto ele roda. O que nao esta aqui nao
/// executa — nem por configuracao, nem por texto vindo da interface.
enum class CommandId {
    /// Mede o deposito de componentes sem mexer nele.
    AnalyzeComponentStore,

    /// Remove versoes antigas de componentes do WinSxS pelo metodo oficial.
    /// Limpar WinSxS na mao quebra o Windows; o DISM e a unica forma aceita.
    CleanupComponentStore,

    /// Pergunta se o Windows esta com a compressao do sistema ligada.
    QueryCompactOs,

    /// Mostra quanto espaco as copias de sombra reservam por volume.
    ListShadowStorage,
};

/// O que um codigo de saida quer dizer.
///
/// Programa do Windows nem sempre usa zero para "deu certo": o `compact`
/// responde 102 para dizer que o sistema nao esta comprimido, o que e uma
/// resposta, nao um erro. Sem essa tabela o Zelo mostraria falha onde houve
/// resposta.
struct ExitMeaning {
    int code = 0;
    bool success = false;
    std::string explanation;
};

struct OfficialCommand {
    CommandId id = CommandId::AnalyzeComponentStore;

    /// Nome curto, para o usuario escolher.
    std::string name;

    /// O que o comando faz, em uma frase que se le antes de aprovar.
    std::string purpose;

    /// Nome do executavel. Resolvido dentro do System32, nunca pelo PATH.
    std::string executable;

    /// Argumentos fixos. Nada aqui vem de fora do programa.
    std::vector<std::string> arguments;

    /// Verdadeiro quando o comando altera o computador. Esses so rodam com
    /// confirmacao explicita do usuario.
    bool modifies_system = false;

    /// Verdadeiro quando o Windows exige privilegio de administrador. Sem ele o
    /// comando falha com mensagem obscura, entao o Zelo avisa antes de tentar.
    bool requires_elevation = false;

    /// O que o usuario perde se deixar rodar. Vazio quando nao ha perda.
    std::string irreversible_effect;

    /// Codigos de saida com significado proprio. Fora desta lista, zero e
    /// sucesso e o resto e falha.
    std::vector<ExitMeaning> exit_meanings;
};

/// Todos os comandos conhecidos, na ordem em que fazem sentido para o usuario:
/// medir antes de mexer.
[[nodiscard]] const std::vector<OfficialCommand>& command_catalog();

/// O comando de um identificador. Como a lista e fechada e completa, sempre
/// existe.
[[nodiscard]] const OfficialCommand& command_by_id(CommandId id);

}
