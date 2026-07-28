#pragma once

#include <core/models/system_snapshot.hpp>

namespace zelo::collectors {

/// Le o que o Windows sabe sobre a saude fisica dos discos.
///
/// Somente leitura. Os contadores de confiabilidade dependem do driver expor —
/// NVMe e disco externo frequentemente nao expoem — e nesse caso ficam
/// marcados como indisponiveis, nunca como zero. Zero significaria "nenhum
/// erro", que e afirmacao bem diferente de "nao consegui medir".
class DiskCollector {
public:
    bool collect_into(core::SystemSnapshot& snapshot) const;

    [[nodiscard]] core::DisksInfo collect() const;
};

}
