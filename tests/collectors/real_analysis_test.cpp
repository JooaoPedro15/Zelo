#include <catch2/catch_test_macros.hpp>
#include <collectors/snapshot_collector.hpp>
#include <core/usecases/quick_analysis.hpp>

#include <string>

using zelo::collectors::collect_snapshot;
using zelo::core::HealthCategory;
using zelo::core::QuickAnalysis;
using zelo::core::RiskLevel;
using zelo::core::SystemSnapshot;

namespace {

std::string risk_name(RiskLevel risk) {
    switch (risk) {
    case RiskLevel::Green:
        return "verde";
    case RiskLevel::Yellow:
        return "amarelo";
    case RiskLevel::Red:
        return "vermelho";
    case RiskLevel::Unknown:
        return "desconhecido";
    }
    return "desconhecido";
}

}

// A analise ponta a ponta nesta maquina: coleta de verdade, regras de verdade,
// pontuacao de verdade. Nada e alterado — tudo aqui e somente leitura.
TEST_CASE("a analise completa roda na maquina real", "[real_analysis][integration]") {
    const SystemSnapshot snapshot = collect_snapshot();
    const auto analysis = QuickAnalysis::with_default_rules();
    const auto result = analysis.run(snapshot);

    INFO("saude geral: " << result.health.overall());
    INFO("achados: " << result.recommendations.size());

    for (const auto& area : result.unavailable) {
        WARN("area nao observada: " << area);
    }

    CHECK(result.health.overall() >= 0);
    CHECK(result.health.overall() <= 100);

    for (const auto& recommendation : result.recommendations) {
        INFO("achado: " << recommendation.title << " [" << risk_name(recommendation.risk) << "]");

        // As promessas do produto, verificadas contra dados reais em vez de
        // fixture: nenhum achado sem evidencia, sem limitacao declarada, e
        // nenhum vermelho executavel pelo aplicativo.
        CHECK(validate(recommendation).empty());
        CHECK_FALSE(recommendation.evidence.empty());
        CHECK_FALSE(recommendation.limitations.empty());
        CHECK_FALSE(recommendation.recommended_action.empty());
        CHECK(recommendation.confidence.value() <= 0.95);

        if (recommendation.risk == RiskLevel::Red) {
            CHECK_FALSE(app_may_execute(recommendation));
        }
    }

    // Cada ponto perdido precisa ter uma causa visivel atras dele.
    for (const auto category : {HealthCategory::Storage, HealthCategory::Startup,
                                HealthCategory::WindowsIntegrity, HealthCategory::Disks}) {
        if (result.health.of(category) < 100) {
            CHECK_FALSE(result.health.deductions_for(category).empty());
        }
    }
}
