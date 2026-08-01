#pragma once

#include "core/rules/analysis_rule.hpp"

#include <cstdint>

namespace cleaner::core {

/// Transforma o que os perfis encontraram em recomendacoes.
///
/// Cada item vira uma linha propria, com o risco que o perfil determinou. E o
/// que separa "limpar o cache do Codex" de "apagar as suas sessoes": as duas
/// coisas moram na mesma pasta e nunca deveriam ter sido oferecidas juntas.
///
/// Itens vermelhos e desconhecidos tambem aparecem, sem acao. Some-los da lista
/// esconderia do usuario onde o espaco esta; mostra-los com o motivo de nao
/// haver acao e a diferenca entre um limpador e uma explicacao.
class AppProfileRule final : public AnalysisRule {
public:
    /// Abaixo disso o item nao merece uma linha na lista.
    static constexpr std::uint64_t kMinimumBytes = 20ULL * 1024 * 1024;

    [[nodiscard]] std::string id() const override;
    [[nodiscard]] int version() const override;

    [[nodiscard]] std::vector<Recommendation> evaluate(const SystemSnapshot& snapshot) const override;
};

}
