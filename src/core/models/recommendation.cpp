#include "core/models/recommendation.hpp"

namespace zelo::core {

std::vector<std::string> validate(const Recommendation& recommendation) {
    std::vector<std::string> problems;

    if (recommendation.id.empty()) {
        problems.emplace_back("recomendacao sem identificador");
    }
    if (recommendation.rule_id.empty()) {
        problems.emplace_back("recomendacao sem regra de origem");
    }
    if (recommendation.title.empty()) {
        problems.emplace_back("recomendacao sem titulo");
    }
    if (recommendation.evidence.empty()) {
        problems.emplace_back("recomendacao sem evidencia");
    }

    return problems;
}

bool app_may_execute(const Recommendation& recommendation) {
    switch (recommendation.risk) {
    case RiskLevel::Green:
    case RiskLevel::Yellow:
        return true;

    case RiskLevel::Red:
    case RiskLevel::Unknown:
        // Vermelho porque o aplicativo nao deve agir; desconhecido porque ele
        // nao sabe o que ha ali, e nao saber e o motivo mais forte para nao
        // mexer. Sem `default:`, um nivel novo quebra a compilacao em vez de
        // ser liberado por descuido.
        return false;
    }
    return false;
}

}
