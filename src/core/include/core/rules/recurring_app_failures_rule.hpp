#pragma once

#include "core/rules/analysis_rule.hpp"

#include <cstddef>

namespace zelo::core {

/// Aponta programas que falharam varias vezes no periodo observado.
///
/// Uma falha isolada acontece e nao diz nada. Repeticao e que indica problema —
/// e e por isso que a regra so fala a partir de um numero minimo de ocorrencias.
class RecurringAppFailuresRule final : public AnalysisRule {
public:
    /// Abaixo disso a recomendacao seria ruido: programas travam de vez em
    /// quando sem que haja nada a fazer.
    static constexpr std::size_t kMinimumFailures = 3;

    /// Quantos programas problematicos citar. A lista existe para orientar,
    /// nao para despejar tudo que o log tem.
    static constexpr std::size_t kMaximumReported = 5;

    [[nodiscard]] std::string id() const override;
    [[nodiscard]] int version() const override;

    [[nodiscard]] std::vector<Recommendation> evaluate(const SystemSnapshot& snapshot) const override;
};

}
