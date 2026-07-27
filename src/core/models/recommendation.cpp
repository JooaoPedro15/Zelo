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
    return recommendation.risk != RiskLevel::Red;
}

}
