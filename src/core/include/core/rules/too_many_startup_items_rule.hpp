#pragma once

#include "core/rules/analysis_rule.hpp"

#include <cstddef>

namespace cleaner::core {

/// Aponta excesso de programas nao essenciais iniciando com o Windows, uma das
/// causas mais comuns de inicializacao lenta.
///
/// Itens essenciais — antivirus, drivers, componentes de audio e video,
/// ferramentas de hardware — ficam fora da contagem e da sugestao. Desativar
/// um deles quebraria protecao ou funcionamento do sistema.
class TooManyStartupItemsRule final : public AnalysisRule {
public:
    /// Ate esta quantidade a inicializacao continua razoavel. Acima disso o
    /// tempo de boot costuma ficar perceptivelmente pior.
    static constexpr std::size_t kAcceptableCount = 5;

    /// A partir daqui a inicializacao deixa de ser so mais lenta e passa a
    /// disputar disco e memoria a ponto de o computador demorar para responder
    /// depois de ligar.
    static constexpr std::size_t kSevereCount = 15;

    [[nodiscard]] std::string id() const override;
    [[nodiscard]] int version() const override;

    [[nodiscard]] std::vector<Recommendation> evaluate(const SystemSnapshot& snapshot) const override;
};

}
