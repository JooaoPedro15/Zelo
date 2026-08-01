#pragma once

#include <core/models/system_snapshot.hpp>

#include <string>
#include <vector>

namespace cleaner::collectors {

/// Le capacidade e espaco livre dos volumes fixos da maquina.
///
/// Volume que nao responde — unidade removivel vazia, midia ejetada, disco de
/// rede fora do ar — e simplesmente omitido. A ausencia fica registrada no
/// snapshot como dado indisponivel, nunca como zero.
class VolumeCollector {
public:
    /// Preenche `volumes` e marca `volumes_available`. Devolve falso quando nem
    /// a lista de unidades pode ser lida, caso em que o snapshot fica sem
    /// afirmar nada sobre armazenamento.
    bool collect_into(core::SystemSnapshot& snapshot) const;

    [[nodiscard]] std::vector<core::VolumeInfo> collect() const;
};

}
