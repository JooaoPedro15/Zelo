#pragma once

#include "core/rules/analysis_rule.hpp"

#include <cstdint>

namespace zelo::core {

/// Aponta acumulo de arquivos temporarios conhecidos. Sao arquivos que o
/// proprio sistema recria quando precisa, entao a limpeza e de risco verde —
/// o que nao dispensa mostrar ao usuario o que seria apagado.
class ExcessiveTemporaryFilesRule final : public AnalysisRule {
public:
    /// Abaixo disso o acumulo nao justifica ocupar a atencao do usuario: a
    /// recomendacao existe para liberar espaco relevante, nao para caçar migalhas.
    static constexpr std::uint64_t kMinimumBytes = 1024ULL * 1024ULL * 1024ULL;

    /// A partir deste volume o acumulo deixa de ser detalhe e vira uma parcela
    /// relevante do disco de quem esta sem espaco.
    static constexpr std::uint64_t kLargeBytes = 10ULL * kMinimumBytes;

    [[nodiscard]] std::string id() const override;
    [[nodiscard]] int version() const override;

    [[nodiscard]] std::vector<Recommendation> evaluate(const SystemSnapshot& snapshot) const override;
};

}
