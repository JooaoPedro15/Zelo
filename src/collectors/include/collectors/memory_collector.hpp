#pragma once

#include <core/models/system_snapshot.hpp>

namespace zelo::collectors {

/// Le a situacao da memoria no momento da analise.
///
/// E uma foto, nao um acompanhamento: serve para detectar aperto agora, nao
/// para afirmar que o computador vive sem memoria. As regras que usam este dado
/// precisam dizer isso ao usuario.
class MemoryCollector {
public:
    bool collect_into(core::SystemSnapshot& snapshot) const;

    [[nodiscard]] core::MemoryInfo collect() const;
};

}
