#pragma once

#include "core/rules/analysis_rule.hpp"

namespace cleaner::core {

/// Aponta volumes de sistema com pouco espaco livre. Pouco espaco no disco de
/// sistema e uma das causas mais comuns de lentidao e de falha em atualizacao,
/// entao o achado vira recomendacao mesmo sem nenhum outro sintoma.
class LowFreeSpaceRule final : public AnalysisRule {
public:
    /// Limite da secao 13 do planejamento: abaixo de 10% de espaco livre o
    /// Windows comeca a ter dificuldade com arquivos temporarios e atualizacoes.
    static constexpr double kLowFreeRatio = 0.10;

    /// Abaixo disto o problema deixa de ser folga apertada e passa a atrapalhar
    /// o funcionamento: atualizacao falha, programa nao salva, o sistema trava.
    static constexpr double kCriticalFreeRatio = 0.05;

    [[nodiscard]] std::string id() const override;
    [[nodiscard]] int version() const override;

    [[nodiscard]] std::vector<Recommendation> evaluate(const SystemSnapshot& snapshot) const override;
};

}
