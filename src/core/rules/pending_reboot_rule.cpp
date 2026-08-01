#include "core/rules/pending_reboot_rule.hpp"

namespace cleaner::core {

std::string PendingRebootRule::id() const {
    return "updates.pending-reboot";
}

int PendingRebootRule::version() const {
    return 1;
}

std::vector<Recommendation> PendingRebootRule::evaluate(const SystemSnapshot& snapshot) const {
    if (!snapshot.updates.available || !snapshot.updates.reboot_pending) {
        return {};
    }

    std::vector<Evidence> evidence;
    evidence.reserve(snapshot.updates.reboot_reasons.size());
    for (const auto& reason : snapshot.updates.reboot_reasons) {
        evidence.push_back(Evidence{.source = "registro do Windows",
                                    .description = "marcador de reinicio pendente",
                                    .value = reason});
    }

    return {Recommendation{
        .id = id(),
        .rule_id = id(),
        .rule_version = version(),
        .title = "O Windows esta esperando um reinicio",
        .description =
            "Alguma instalacao ou atualizacao ficou pela metade e so termina depois de reiniciar. "
            "Ate la o sistema trabalha num estado intermediario, que costuma causar lentidao e "
            "erros que desaparecem sozinhos depois do reinicio.",
        .category = ActionCategory::ReadOnlyAnalysis,
        .health_category = HealthCategory::Updates,
        .severity = Severity::Attention,
        .risk = RiskLevel::Green,
        .confidence = Confidence::from_signals(
            {{"o proprio Windows registrou que falta reiniciar", 0.9}}),
        .evidence = std::move(evidence),
        .recommended_action = "Reinicie o computador quando puder, salvando o que estiver aberto.",
        .alternative_action = "Continuar usando; nada quebra por causa disso, mas o estado "
                              "intermediario permanece ate o proximo reinicio.",
        .requires_reboot = true,
        .expected_result = "As instalacoes pendentes se completam e o sistema volta a um estado "
                           "consistente.",
        .limitations = "Reiniciar conclui o que estava pendente. Nao corrige problemas de outra "
                       "natureza, e um reinicio demorado logo apos atualizacoes grandes e normal.",
    }};
}

}
