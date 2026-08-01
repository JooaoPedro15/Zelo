#pragma once

#include <core/models/system_snapshot.hpp>

namespace cleaner::collectors {

/// Verifica se o Windows esta esperando um reinicio para concluir algo.
///
/// Somente leitura do registro. Nao consulta o Windows Update pela internet: a
/// busca online e lenta e depende de rede, e o MVP precisa responder rapido.
/// Atualizacoes pendentes ficam para depois, e a interface diz que nao olhou.
class UpdateCollector {
public:
    bool collect_into(core::SystemSnapshot& snapshot) const;

    [[nodiscard]] core::UpdatesInfo collect() const;
};

}
