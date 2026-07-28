#include "core/usecases/quick_analysis.hpp"

#include "core/rules/excessive_temporary_files_rule.hpp"
#include "core/rules/low_free_space_rule.hpp"
#include "core/rules/low_memory_rule.hpp"
#include "core/rules/pending_reboot_rule.hpp"
#include "core/rules/recurring_app_failures_rule.hpp"
#include "core/rules/too_many_startup_items_rule.hpp"

#include <utility>

namespace zelo::core {

namespace {

/// Quanto cada achado tira da pontuacao. Os valores saem dos exemplos da secao
/// 13 do planejamento e ficam fixados em teste, para que mudar o peso de um
/// problema seja uma decisao consciente.
int deduction_for(Severity severity) {
    switch (severity) {
    case Severity::Info:
        return 3;
    case Severity::Attention:
        return 10;
    case Severity::Serious:
        return 20;
    }
    return 3;
}

}

QuickAnalysis::QuickAnalysis(std::vector<std::shared_ptr<const AnalysisRule>> rules)
    : rules_(std::move(rules)) {}

QuickAnalysis QuickAnalysis::with_default_rules() {
    return QuickAnalysis{{
        std::make_shared<const LowFreeSpaceRule>(),
        std::make_shared<const ExcessiveTemporaryFilesRule>(),
        std::make_shared<const TooManyStartupItemsRule>(),
        std::make_shared<const RecurringAppFailuresRule>(),
        std::make_shared<const PendingRebootRule>(),
        std::make_shared<const LowMemoryRule>(),
    }};
}

AnalysisResult QuickAnalysis::run(const SystemSnapshot& snapshot) const {
    AnalysisResult result;

    std::vector<HealthDeduction> deductions;

    for (const auto& rule : rules_) {
        for (auto& recommendation : rule->evaluate(snapshot)) {
            // Achado sem evidencia nao chega ao usuario. Se uma regra produzir
            // algo incompleto, o problema fica no achado, nao na analise toda.
            if (!validate(recommendation).empty()) {
                continue;
            }

            deductions.push_back(HealthDeduction{recommendation.health_category,
                                                 deduction_for(recommendation.severity),
                                                 recommendation.title});
            result.recommendations.push_back(std::move(recommendation));
        }
    }

    if (!snapshot.volumes_available) {
        result.unavailable.emplace_back("espaco em disco");
    }
    if (!snapshot.temporary_files.available) {
        result.unavailable.emplace_back("arquivos temporarios");
    }
    if (!snapshot.startup_available) {
        result.unavailable.emplace_back("programas de inicializacao");
    }
    if (!snapshot.stability.available) {
        result.unavailable.emplace_back("falhas registradas pelo Windows");
    }
    if (!snapshot.updates.available) {
        result.unavailable.emplace_back("atualizacoes do Windows");
    }
    if (!snapshot.memory.available) {
        result.unavailable.emplace_back("memoria do sistema");
    }

    result.health = HealthScore::from_deductions(std::move(deductions));
    return result;
}

}
