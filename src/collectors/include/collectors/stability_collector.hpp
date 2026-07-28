#pragma once

#include <core/models/system_snapshot.hpp>

namespace zelo::collectors {

/// Le o registro de eventos do Windows procurando falhas de aplicativo e
/// desligamentos inesperados.
///
/// Somente leitura. O canal Application e legivel por usuario comum; o canal
/// System costuma exigir elevacao, e quando nao da para ler o coletor devolve
/// o que conseguiu em vez de falhar inteiro.
class StabilityCollector {
public:
    explicit StabilityCollector(int window_days = 30);

    bool collect_into(core::SystemSnapshot& snapshot) const;

    [[nodiscard]] core::StabilityInfo collect() const;

private:
    int window_days_;
};

}
