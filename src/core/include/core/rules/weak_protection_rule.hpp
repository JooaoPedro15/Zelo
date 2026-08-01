#pragma once

#include "core/rules/analysis_rule.hpp"

namespace cleaner::core {

/// Avisa quando a protecao do Windows esta desligada ou com definicoes velhas.
///
/// O Cleaner nao e antivirus e nao promete detectar ameaca nenhuma. Ele apenas
/// mostra o que a protecao ja instalada esta reportando sobre si mesma, e
/// aponta para as ferramentas oficiais.
class WeakProtectionRule final : public AnalysisRule {
public:
    /// Definicao mais velha que isto deixa de cobrir o que apareceu depois.
    static constexpr int kStaleSignatureDays = 7;

    [[nodiscard]] std::string id() const override;
    [[nodiscard]] int version() const override;

    [[nodiscard]] std::vector<Recommendation> evaluate(const SystemSnapshot& snapshot) const override;
};

}
