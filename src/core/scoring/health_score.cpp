#include "core/scoring/health_score.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace cleaner::core {

HealthScore HealthScore::from_deductions(std::vector<HealthDeduction> deductions) {
    HealthScore score;
    for (auto& deduction : deductions) {
        score.deductions_[deduction.category].push_back(std::move(deduction));
    }
    return score;
}

namespace {

// Pesos da secao 13 do planejamento. Somam 1.0 — a soma e conferida por teste.
struct CategoryWeight {
    HealthCategory category;
    double weight;
};

constexpr std::array kWeights{
    CategoryWeight{HealthCategory::Storage, 0.15},
    CategoryWeight{HealthCategory::WindowsIntegrity, 0.15},
    CategoryWeight{HealthCategory::Disks, 0.15},
    CategoryWeight{HealthCategory::Stability, 0.15},
    CategoryWeight{HealthCategory::Performance, 0.15},
    CategoryWeight{HealthCategory::Updates, 0.10},
    CategoryWeight{HealthCategory::Security, 0.10},
    CategoryWeight{HealthCategory::Startup, 0.05},
};

}

int HealthScore::overall() const {
    double weighted = 0.0;
    int worst = kMaximumScore;

    for (const auto& entry : kWeights) {
        const int score = of(entry.category);
        weighted += entry.weight * score;
        worst = std::min(worst, score);
    }

    // Metade media ponderada, metade pior categoria.
    //
    // So a media diluia qualquer problema isolado: com oito categorias e peso
    // maximo de 15 por cento, um disco praticamente cheio movia o geral em
    // menos de dois pontos, e o numero dizia "esta tudo bem" enquanto havia um
    // problema real. Trazer a pior categoria para a conta faz a pontuacao
    // acompanhar o pior achado sem ignorar o resto.
    return static_cast<int>(std::lround(weighted * 0.5 + worst * 0.5));
}

int HealthScore::of(HealthCategory category) const {
    int score = kMaximumScore;
    for (const auto& deduction : deductions_for(category)) {
        score -= deduction.points;
    }
    return std::max(score, 0);
}

const std::vector<HealthDeduction>& HealthScore::deductions_for(HealthCategory category) const {
    static const std::vector<HealthDeduction> none;

    const auto entry = deductions_.find(category);
    return entry == deductions_.end() ? none : entry->second;
}

}
