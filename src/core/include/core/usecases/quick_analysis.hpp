#pragma once

#include "core/models/recommendation.hpp"
#include "core/models/system_snapshot.hpp"
#include "core/rules/analysis_rule.hpp"
#include "core/scoring/health_score.hpp"

#include <memory>
#include <vector>

namespace zelo::core {

/// O resultado de uma analise: o que foi achado, o quanto isso pesa na
/// pontuacao e o que a analise nao conseguiu observar.
struct AnalysisResult {
    std::vector<Recommendation> recommendations;
    HealthScore health;

    /// Categorias que ficaram sem dado. A interface precisa dizer "nao consegui
    /// olhar isto" em vez de deixar o usuario achar que esta tudo bem.
    std::vector<std::string> unavailable;
};

/// Roda as regras sobre o que os coletores observaram e transforma os achados
/// em pontuacao.
///
/// Nao coleta nada por conta propria: recebe o snapshot pronto. E por isso que
/// a analise inteira roda em teste sem tocar no sistema.
class QuickAnalysis {
public:
    explicit QuickAnalysis(std::vector<std::shared_ptr<const AnalysisRule>> rules);

    /// Monta o conjunto padrao de regras do MVP.
    [[nodiscard]] static QuickAnalysis with_default_rules();

    [[nodiscard]] AnalysisResult run(const SystemSnapshot& snapshot) const;

private:
    std::vector<std::shared_ptr<const AnalysisRule>> rules_;
};

}
