#pragma once

#include "core/rules/analysis_rule.hpp"

namespace cleaner::core {

/// Aponta indicios de problema fisico nos discos.
///
/// Esta e a regra mais delicada do projeto. Disco com sinal de falha nao e caso
/// de limpeza nem de comando de reparo: e caso de fazer backup antes que seja
/// tarde. Nenhuma acao do Cleaner conserta hardware, e a recomendacao diz isso.
///
/// Na direcao oposta, a regra tambem nao declara disco saudavel. Um unico
/// indicador respondendo "Healthy" nao sustenta essa afirmacao — sustenta
/// apenas "nada foi detectado", que e diferente.
class DiskHealthRule final : public AnalysisRule {
public:
    /// Erros acumulados a partir daqui deixam de ser ruido de leitura isolada.
    static constexpr int kConcerningErrors = 1;

    [[nodiscard]] std::string id() const override;
    [[nodiscard]] int version() const override;

    [[nodiscard]] std::vector<Recommendation> evaluate(const SystemSnapshot& snapshot) const override;
};

}
