#pragma once

#include "core/rules/analysis_rule.hpp"

namespace cleaner::core {

/// Aponta falta de memoria disponivel no momento da analise.
///
/// Uso alto de memoria nao e defeito: o Windows aproveita memoria livre como
/// cache de proposito, e um numero alto ali costuma significar que o sistema
/// esta usando bem o que tem. O que indica aperto e sobrar pouco — por isso a
/// regra olha memoria disponivel, nao memoria usada.
class LowMemoryRule final : public AnalysisRule {
public:
    /// Abaixo disto o sistema comeca a empurrar conteudo para o disco para dar
    /// conta, e o computador fica perceptivelmente mais lento.
    static constexpr double kLowAvailableRatio = 0.10;

    static constexpr double kCriticalAvailableRatio = 0.05;

    [[nodiscard]] std::string id() const override;
    [[nodiscard]] int version() const override;

    [[nodiscard]] std::vector<Recommendation> evaluate(const SystemSnapshot& snapshot) const override;
};

}
