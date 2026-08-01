#include "commands/command_catalog.hpp"

#include <algorithm>
#include <stdexcept>

namespace cleaner::commands {

const std::vector<OfficialCommand>& command_catalog() {
    static const std::vector<OfficialCommand> catalog = {
        OfficialCommand{
            .id = CommandId::AnalyzeComponentStore,
            .name = "Medir o deposito de componentes",
            .purpose = "Pergunta ao Windows quanto o WinSxS ocupa e se a limpeza oficial e "
                       "recomendada. Nao remove nada.",
            .executable = "Dism.exe",
            .arguments = {"/Online", "/Cleanup-Image", "/AnalyzeComponentStore"},
            .modifies_system = false,
            .requires_elevation = true,
            .exit_meanings = {ExitMeaning{
                .code = 3010,
                .success = true,
                .explanation = "A medicao terminou. O Windows pede reinicio para concluir.",
            }},
        },
        OfficialCommand{
            .id = CommandId::CleanupComponentStore,
            .name = "Limpar o deposito de componentes",
            .purpose = "Remove versoes antigas de componentes do Windows pelo metodo oficial "
                       "(DISM). E a mesma limpeza que a Limpeza de Disco faz.",
            .executable = "Dism.exe",
            .arguments = {"/Online", "/Cleanup-Image", "/StartComponentCleanup"},
            .modifies_system = true,
            .requires_elevation = true,
            .irreversible_effect = "Depois disso, atualizacoes ja instaladas deixam de poder ser "
                                   "desinstaladas. O Windows continua funcionando.",
            .exit_meanings = {ExitMeaning{
                .code = 3010,
                .success = true,
                .explanation = "A limpeza terminou. O Windows pede reinicio para concluir.",
            }},
        },
        OfficialCommand{
            .id = CommandId::QueryCompactOs,
            .name = "Verificar a compressao do sistema",
            .purpose = "Diz se os arquivos do Windows estao comprimidos. So consulta.",
            .executable = "Compact.exe",
            .arguments = {"/CompactOS:query"},
            .modifies_system = false,
            .requires_elevation = false,
            .exit_meanings =
                {
                    ExitMeaning{
                        .code = 0,
                        .success = true,
                        .explanation = "O Windows esta com a compressao do sistema ligada.",
                    },
                    ExitMeaning{
                        .code = 102,
                        .success = true,
                        .explanation = "O Windows nao esta comprimido. Ele mesmo avaliou que "
                                       "comprimir nao traria ganho neste computador.",
                    },
                },
        },
        OfficialCommand{
            .id = CommandId::ListShadowStorage,
            .name = "Medir os pontos de restauracao",
            .purpose = "Mostra quanto espaco as copias de sombra e os pontos de restauracao "
                       "reservam. So consulta.",
            .executable = "vssadmin.exe",
            .arguments = {"list", "shadowstorage"},
            .modifies_system = false,
            .requires_elevation = true,
        },
    };

    return catalog;
}

const OfficialCommand& command_by_id(CommandId id) {
    const auto& catalog = command_catalog();
    const auto found = std::find_if(catalog.begin(), catalog.end(),
                                    [id](const OfficialCommand& entry) { return entry.id == id; });

    if (found == catalog.end()) {
        // Um identificador fora da lista significa que alguem acrescentou um
        // valor ao enum sem descrever o comando. Falhar alto e melhor do que
        // executar qualquer outra coisa no lugar.
        throw std::logic_error("comando oficial sem entrada no catalogo");
    }

    return *found;
}

}
