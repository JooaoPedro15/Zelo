#pragma once

#include "core/rules/analysis_rule.hpp"

namespace cleaner::core {

/// Avisa que o Windows esta esperando um reinicio para concluir algo.
///
/// Ate reiniciar, o sistema fica num estado intermediario que costuma explicar
/// lentidao e falhas que somem sozinhas depois. E a acao mais barata e mais
/// segura que existe — por isso vale avisar mesmo sendo simples.
class PendingRebootRule final : public AnalysisRule {
public:
    [[nodiscard]] std::string id() const override;
    [[nodiscard]] int version() const override;

    [[nodiscard]] std::vector<Recommendation> evaluate(const SystemSnapshot& snapshot) const override;
};

}
