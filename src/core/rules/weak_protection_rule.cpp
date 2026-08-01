#include "core/rules/weak_protection_rule.hpp"

namespace cleaner::core {

std::string WeakProtectionRule::id() const {
    return "security.weak-protection";
}

int WeakProtectionRule::version() const {
    return 1;
}

std::vector<Recommendation> WeakProtectionRule::evaluate(const SystemSnapshot& snapshot) const {
    const auto& security = snapshot.security;
    if (!security.available) {
        return {};
    }

    // Antivirus de terceiros: ha protecao, apenas nao e a do Windows. O Cleaner
    // nao tem como avaliar a saude dele e nao vai fingir que tem.
    if (!security.provider.empty() && security.provider != "Seguranca do Windows") {
        return {};
    }

    std::vector<Evidence> evidence;
    bool protection_off = false;

    if (!security.antivirus_enabled || !security.realtime_protection_enabled) {
        protection_off = true;
        evidence.push_back(Evidence{
            .source = "Seguranca do Windows",
            .description = "estado da protecao",
            .value = security.antivirus_enabled ? "protecao em tempo real desligada"
                                                : "protecao contra virus desligada"});
    }

    const bool stale_signatures = security.signature_age_days > kStaleSignatureDays;
    if (stale_signatures) {
        evidence.push_back(Evidence{.source = "Seguranca do Windows",
                                    .description = "idade das definicoes de ameaca",
                                    .value = std::to_string(security.signature_age_days) +
                                             " dias desde a ultima atualizacao"});
    }

    if (evidence.empty()) {
        return {};
    }

    const std::string title =
        protection_off ? "A protecao do Windows esta desligada"
                       : "As definicoes de ameaca estao desatualizadas";

    return {Recommendation{
        .id = id(),
        .rule_id = id(),
        .rule_version = version(),
        .title = title,
        .description =
            protection_off
                ? "A protecao contra virus do Windows nao esta ativa. Sem ela o computador fica "
                  "sem a defesa que ja vem instalada."
                : "As definicoes de ameaca estao com mais de uma semana. Elas continuam "
                  "protegendo contra o que ja era conhecido, mas nao contra o que apareceu "
                  "depois.",
        .category = ActionCategory::ReadOnlyAnalysis,
        .health_category = HealthCategory::Security,
        .severity = protection_off ? Severity::Serious : Severity::Attention,
        .risk = RiskLevel::Green,
        .confidence = Confidence::from_signals(
            {{"estado informado pela propria Seguranca do Windows", 0.9}}),
        .evidence = std::move(evidence),
        .recommended_action = "Abra a Seguranca do Windows e reative a protecao ou atualize as "
                              "definicoes.",
        .alternative_action = "Se voce usa outro antivirus, verifique se ele esta ativo — nesse "
                              "caso a protecao do Windows fica desligada de proposito.",
        .tool = "Seguranca do Windows",
        .expected_result = "Protecao ativa e definicoes atualizadas.",
        .limitations =
            "O Cleaner apenas le o que a protecao informa sobre si mesma. Ele nao procura ameacas, "
            "nao substitui um antivirus e nao consegue dizer se o computador ja foi infectado.",
    }};
}

}
