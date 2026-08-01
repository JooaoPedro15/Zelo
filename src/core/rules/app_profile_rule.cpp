#include "core/rules/app_profile_rule.hpp"

#include "core/rules/format.hpp"

#include <algorithm>

namespace cleaner::core {

namespace {

std::string regeneration_text(RegenerationCost cost) {
    switch (cost) {
    case RegenerationCost::Free:
        return "Volta sozinho, sem custo perceptivel.";
    case RegenerationCost::NeedsDownload:
        return "Volta sozinho, mas sera baixado de novo quando for preciso.";
    case RegenerationCost::NeedsRework:
        return "Volta sozinho, mas o programa refaz o trabalho no proximo uso.";
    case RegenerationCost::Permanent:
        return "Nao volta.";
    }
    return {};
}

/// O que dizer sobre agir, conforme o risco. Vermelho e desconhecido nao
/// recebem acao, e o texto explica por que — em vez de simplesmente nao ter
/// botao e deixar o usuario sem entender.
std::string action_text(const ProfileItem& item) {
    switch (item.risk) {
    case RiskLevel::Green:
        return "Pode ser removido. " + regeneration_text(item.regeneration);

    case RiskLevel::Yellow:
        return "Pode ser removido, mas revise antes. " + regeneration_text(item.regeneration);

    case RiskLevel::Red:
        return "O Cleaner nao oferece remover isto. E conteudo seu, nao sobra de funcionamento. Se "
               "precisar liberar este espaco, faca pelo proprio programa, que sabe o que pode "
               "descartar.";

    case RiskLevel::Unknown:
        return "O Cleaner nao oferece remover isto porque nao sabe o que ha aqui. Nao saber e motivo "
               "para nao mexer, nao para arriscar.";
    }
    return {};
}

}

std::string AppProfileRule::id() const {
    return "apps.profile-item";
}

int AppProfileRule::version() const {
    return 1;
}

std::vector<Recommendation> AppProfileRule::evaluate(const SystemSnapshot& snapshot) const {
    if (!snapshot.profiles_available) {
        return {};
    }

    std::vector<ProfileFinding> relevant;
    for (const auto& finding : snapshot.profile_findings) {
        if (finding.size_bytes >= kMinimumBytes) {
            relevant.push_back(finding);
        }
    }

    std::sort(relevant.begin(), relevant.end(),
              [](const ProfileFinding& left, const ProfileFinding& right) {
                  return left.size_bytes > right.size_bytes;
              });

    std::vector<Recommendation> recommendations;
    recommendations.reserve(relevant.size());

    for (const auto& finding : relevant) {
        const std::string size_text = format_bytes(finding.size_bytes);

        recommendations.push_back(Recommendation{
            .id = id() + ":" + finding.path,
            .rule_id = id(),
            .rule_version = version(),
            .title = finding.application + " — " + finding.item.display_name + " (" + size_text + ")",
            .description = finding.item.what_it_is,
            .category = finding.item.risk == RiskLevel::Green ? ActionCategory::KnownCache
                                                              : ActionCategory::ApplicationDatabase,
            .health_category = HealthCategory::Storage,
            .severity = Severity::Info,
            // Espaco que da para liberar e oportunidade, nao defeito: o disco
            // cheio de verdade ja e pontuado por conta propria.
            .counts_against_health = false,
            .risk = finding.item.risk,
            .confidence = Confidence::from_signals(
                finding.item.risk == RiskLevel::Unknown
                    ? std::vector<ConfidenceSignal>{{"tamanho medido no disco", 0.9},
                                                    {"conteudo nao identificado", -0.6}}
                    : std::vector<ConfidenceSignal>{{"item conhecido do programa", 0.85},
                                                    {"tamanho medido no disco", 0.1}}),
            .evidence = {Evidence{.source = finding.application,
                                  .description = "espaco ocupado por " + finding.item.display_name,
                                  .value = size_text + " em " + finding.path}},
            .reclaimable_bytes =
                finding.item.risk == RiskLevel::Green ? finding.size_bytes : std::uint64_t{0},
            .affected_paths = {finding.path},
            .recommended_action = action_text(finding.item),
            .alternative_action = "Manter como esta.",
            .expected_result = finding.item.risk == RiskLevel::Green
                                   ? size_text + " livres no disco."
                                   : "Nenhuma acao e feita por aqui.",
            .limitations = "O que voce perde: " + finding.item.what_you_lose,
        });
    }

    return recommendations;
}

}
