#include <catch2/catch_test_macros.hpp>
#include <core/models/recommendation.hpp>

#include <string>
#include <vector>

using cleaner::core::ActionCategory;
using cleaner::core::Confidence;
using cleaner::core::Evidence;
using cleaner::core::Recommendation;
using cleaner::core::RiskLevel;
using cleaner::core::Severity;

namespace {

Recommendation a_valid_recommendation() {
    return Recommendation{
        .id = "storage.low-free-space",
        .rule_id = "storage.low-free-space",
        .rule_version = 1,
        .title = "Disco C: com pouco espaco livre",
        .description = "O disco C: esta com 4% de espaco livre.",
        .category = ActionCategory::ReadOnlyAnalysis,
        .severity = Severity::Attention,
        .risk = RiskLevel::Green,
        .confidence = Confidence::from_signals({{"espaco livre medido em 4%", 0.9}}),
        .evidence = {Evidence{.source = "GetDiskFreeSpaceEx",
                              .description = "espaco livre no volume C:",
                              .value = "18,2 GB de 465 GB"}},
    };
}

}

TEST_CASE("recomendacao completa passa na validacao", "[recommendation]") {
    CHECK(validate(a_valid_recommendation()).empty());
}

// Invariante da etapa 4: nenhuma recomendacao sem evidencia. Sem isso o app
// estaria afirmando algo ao usuario sem poder mostrar como chegou la.
TEST_CASE("recomendacao sem evidencia e rejeitada", "[recommendation]") {
    Recommendation recommendation = a_valid_recommendation();
    recommendation.evidence.clear();

    CHECK_FALSE(validate(recommendation).empty());
}

TEST_CASE("recomendacao sem identificacao e rejeitada", "[recommendation]") {
    SECTION("sem id") {
        Recommendation recommendation = a_valid_recommendation();
        recommendation.id.clear();

        CHECK_FALSE(validate(recommendation).empty());
    }

    SECTION("sem regra de origem") {
        Recommendation recommendation = a_valid_recommendation();
        recommendation.rule_id.clear();

        CHECK_FALSE(validate(recommendation).empty());
    }

    SECTION("sem titulo") {
        Recommendation recommendation = a_valid_recommendation();
        recommendation.title.clear();

        CHECK_FALSE(validate(recommendation).empty());
    }
}

// Principio 6 do projeto: item vermelho e apenas explicado. Nem uma
// autorizacao generica do usuario transforma isso em acao automatica.
TEST_CASE("o aplicativo nunca executa recomendacao vermelha", "[recommendation]") {
    Recommendation recommendation = a_valid_recommendation();

    recommendation.risk = RiskLevel::Green;
    CHECK(app_may_execute(recommendation));

    recommendation.risk = RiskLevel::Yellow;
    CHECK(app_may_execute(recommendation));

    recommendation.risk = RiskLevel::Red;
    CHECK_FALSE(app_may_execute(recommendation));

    // Nao saber o que ha num lugar nao autoriza mexer nele. E o oposto: e o
    // motivo mais forte para nao mexer.
    recommendation.risk = RiskLevel::Unknown;
    CHECK_FALSE(app_may_execute(recommendation));
}

// Um campo esquecido nao pode passar silenciosamente: o padrao do modelo e o
// lado seguro, entao uma recomendacao criada vazia ja nasce vermelha.
TEST_CASE("recomendacao nasce vermelha por padrao", "[recommendation]") {
    const Recommendation recommendation;

    CHECK(recommendation.risk == RiskLevel::Red);
    CHECK_FALSE(app_may_execute(recommendation));
}
