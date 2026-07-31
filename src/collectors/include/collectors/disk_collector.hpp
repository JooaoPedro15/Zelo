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
    explicit DiskCollector(int window_days = 30);

    bool collect_into(core::SystemSnapshot& snapshot) const;

    [[nodiscard]] core::DisksInfo collect() const;

private:
    /// Le os eventos em que o NTFS reclama da estrutura do sistema de arquivos.
    /// E problema de disco, com chkdsk como ferramenta — diferente de
    /// componente do Windows danificado, que pede sfc.
    void collect_filesystem_events(core::DisksInfo& info) const;

    int window_days_;
};

}
