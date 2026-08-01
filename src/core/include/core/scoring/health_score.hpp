#pragma once

#include "core/models/health_category.hpp"

#include <map>
#include <string>
#include <vector>

namespace cleaner::core {

/// Um desconto na pontuacao, sempre com a causa visivel. A pontuacao nunca
/// perde pontos sem que o usuario possa ver de onde veio o desconto.
struct HealthDeduction {
    HealthCategory category;
    int points;
    std::string cause;
};

/// Pontuacao de 0 a 100, geral e por categoria. E orientacao, nao diagnostico
/// definitivo: serve para guiar a atencao do usuario, nunca para assusta-lo.
class HealthScore {
public:
    static constexpr int kMaximumScore = 100;

    static HealthScore from_deductions(std::vector<HealthDeduction> deductions);

    [[nodiscard]] int overall() const;
    [[nodiscard]] int of(HealthCategory category) const;

    /// Os descontos aplicados a uma categoria, na ordem em que foram
    /// informados. A interface usa isto para mostrar de onde os pontos sairam.
    [[nodiscard]] const std::vector<HealthDeduction>& deductions_for(HealthCategory category) const;

private:
    std::map<HealthCategory, std::vector<HealthDeduction>> deductions_;
};

}
