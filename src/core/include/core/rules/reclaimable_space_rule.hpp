#pragma once

#include "core/rules/analysis_rule.hpp"

#include <cstdint>

namespace cleaner::core {

/// Mostra os locais conhecidos que estao ocupando espaco, um por recomendacao.
///
/// Uma recomendacao por local, e nao uma lista unica, porque cada um tem seu
/// proprio risco e sua propria consequencia. Juntar tudo num "limpar caches"
/// obrigaria o usuario a aceitar ou recusar em bloco coisas que nao se parecem:
/// miniaturas do Explorador e modelos de IA de varios gigabytes nao merecem a
/// mesma decisao.
class ReclaimableSpaceRule final : public AnalysisRule {
public:
    /// Abaixo disso nao vale ocupar a atencao do usuario com uma linha na lista.
    static constexpr std::uint64_t kMinimumBytes = 500ULL * 1024ULL * 1024ULL;

    /// Com o disco apertado, ate um local menor passa a importar.
    static constexpr std::uint64_t kMinimumBytesUnderPressure = 100ULL * 1024ULL * 1024ULL;

    [[nodiscard]] std::string id() const override;
    [[nodiscard]] int version() const override;

    [[nodiscard]] std::vector<Recommendation> evaluate(const SystemSnapshot& snapshot) const override;
};

}
