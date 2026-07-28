#pragma once

#include <core/models/system_snapshot.hpp>

namespace zelo::collectors {

/// Consulta o estado da protecao do Windows.
///
/// Somente leitura, e apenas do que o proprio sistema ja sabe. O Zelo nao
/// procura ameacas, nao substitui antivirus e nao opina sobre qual usar — ele
/// so mostra o que a protecao instalada esta reportando.
class SecurityCollector {
public:
    bool collect_into(core::SystemSnapshot& snapshot) const;

    [[nodiscard]] core::SecurityInfo collect() const;
};

}
