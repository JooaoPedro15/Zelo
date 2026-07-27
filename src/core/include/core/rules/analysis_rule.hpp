#pragma once

#include "core/models/recommendation.hpp"
#include "core/models/system_snapshot.hpp"

#include <string>
#include <vector>

namespace zelo::core {

/// Uma regra de analise: le o que os coletores observaram e emite zero ou mais
/// recomendacoes. Regras sao puras — nao tocam no disco, nao chamam o Windows
/// e nao conhecem a interface, entao rodam inteiras em teste.
///
/// Cada regra carrega id e versao: quando um criterio muda, achados antigos
/// continuam rastreaveis ate a versao que os produziu.
class AnalysisRule {
public:
    AnalysisRule() = default;
    AnalysisRule(const AnalysisRule&) = default;
    AnalysisRule(AnalysisRule&&) = default;
    AnalysisRule& operator=(const AnalysisRule&) = default;
    AnalysisRule& operator=(AnalysisRule&&) = default;
    virtual ~AnalysisRule() = default;

    [[nodiscard]] virtual std::string id() const = 0;
    [[nodiscard]] virtual int version() const = 0;

    [[nodiscard]] virtual std::vector<Recommendation> evaluate(const SystemSnapshot& snapshot) const = 0;
};

}
