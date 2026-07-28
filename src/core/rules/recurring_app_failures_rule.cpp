#include "core/rules/recurring_app_failures_rule.hpp"

#include <algorithm>

namespace zelo::core {

std::string RecurringAppFailuresRule::id() const {
    return "stability.recurring-app-failures";
}

int RecurringAppFailuresRule::version() const {
    return 1;
}

std::vector<Recommendation> RecurringAppFailuresRule::evaluate(const SystemSnapshot& snapshot) const {
    if (!snapshot.stability.available) {
        return {};
    }

    std::vector<AppFailureInfo> recurring;
    for (const auto& failure : snapshot.stability.app_failures) {
        if (failure.count >= kMinimumFailures) {
            recurring.push_back(failure);
        }
    }

    if (recurring.empty()) {
        return {};
    }

    const std::string window = std::to_string(snapshot.stability.window_days);

    std::vector<Evidence> evidence;
    std::vector<std::string> affected;
    const auto reported = std::min(recurring.size(), kMaximumReported);

    for (std::size_t index = 0; index < reported; ++index) {
        const auto& failure = recurring.at(index);

        std::string detail = std::to_string(failure.count) + " falhas";
        if (!failure.faulting_module.empty()) {
            detail += ", componente apontado: " + failure.faulting_module;
        }
        if (!failure.last_seen.empty()) {
            detail += ", a mais recente em " + failure.last_seen;
        }

        evidence.push_back(Evidence{.source = "Visualizador de Eventos, canal Application",
                                    .description = "falhas registradas de " + failure.application,
                                    .value = detail});
        affected.push_back(failure.application);
    }

    const std::string title =
        recurring.size() == 1
            ? "O programa " + recurring.front().application + " falhou varias vezes"
            : std::to_string(recurring.size()) + " programas falharam varias vezes";

    return {Recommendation{
        .id = id(),
        .rule_id = id(),
        .rule_version = version(),
        .title = title,
        .description = "O Windows registrou falhas repetidas nos ultimos " + window +
                       " dias. Repeticao costuma indicar instalacao danificada, incompatibilidade "
                       "com uma atualizacao recente ou problema em um componente compartilhado.",
        .category = ActionCategory::ReadOnlyAnalysis,
        .severity = Severity::Attention,
        .risk = RiskLevel::Green,
        .confidence = Confidence::from_signals(
            {{"falhas registradas pelo proprio Windows", 0.6},
             {"o mesmo programa falhou mais de uma vez", 0.25}}),
        .evidence = std::move(evidence),
        .affected_paths = std::move(affected),
        .recommended_action =
            "Verifique se ha atualizacao para estes programas. Se a falha continuar, reinstalar o "
            "programa costuma resolver quando o problema e a instalacao.",
        .alternative_action = "Abrir o Monitor de Confiabilidade do Windows para ver o historico "
                              "completo antes de decidir.",
        .tool = "Visualizador de Eventos",
        .expected_result = "Menos travamentos do programa afetado, quando a causa estiver nele.",
        .limitations =
            "O registro mostra que houve falha, nao por que ela aconteceu. A causa pode estar no "
            "programa, em um driver, em uma atualizacao ou no hardware, e o Zelo nao consegue "
            "distinguir sozinho. Falhas que derrubam o computador inteiro podem nem aparecer aqui.",
    }};
}

}
