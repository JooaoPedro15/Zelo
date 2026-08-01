#include <catch2/catch_test_macros.hpp>
#include <ui/presentation.hpp>

using cleaner::core::AnalysisResult;
using cleaner::core::Confidence;
using cleaner::core::Recommendation;
using cleaner::core::RiskLevel;

// O rotulo de risco fala do que o usuario deve fazer, nao da cor interna. Um
// item vermelho precisa deixar claro que o aplicativo nao vai agir sozinho.
TEST_CASE("o rotulo de risco orienta em vez de nomear a cor", "[presentation]") {
    CHECK(cleaner::ui::risk_label(RiskLevel::Green) == "Normalmente seguro");
    CHECK(cleaner::ui::risk_label(RiskLevel::Yellow) == "Revise antes");
    CHECK(cleaner::ui::risk_label(RiskLevel::Red) == "Apenas informativo");
}

TEST_CASE("cada nivel de risco tem cor propria", "[presentation]") {
    CHECK(cleaner::ui::risk_color(RiskLevel::Green) != cleaner::ui::risk_color(RiskLevel::Yellow));
    CHECK(cleaner::ui::risk_color(RiskLevel::Yellow) != cleaner::ui::risk_color(RiskLevel::Red));
}

TEST_CASE("a confianca aparece como porcentagem inteira", "[presentation]") {
    CHECK(cleaner::ui::confidence_label(Confidence::restored(0.76, {})) == "76%");
    CHECK(cleaner::ui::confidence_label(Confidence::restored(0.0, {})) == "0%");

    // O teto de 95 por cento tambem vale na tela: o usuario nunca ve 100.
    CHECK(cleaner::ui::confidence_label(Confidence::restored(1.0, {})) == "95%");
}

// Sem achados, a frase diz o que foi analisado — nunca que o computador esta
// perfeito, porque a analise cobre apenas parte do sistema.
TEST_CASE("o resumo nao promete mais do que a analise cobre", "[presentation]") {
    AnalysisResult empty;
    CHECK(cleaner::ui::health_summary(empty) == "Nenhum problema encontrado nas areas analisadas.");

    AnalysisResult one;
    one.recommendations.push_back(Recommendation{});
    CHECK(cleaner::ui::health_summary(one) == "1 ponto de atencao encontrado.");

    AnalysisResult three;
    three.recommendations.resize(3);
    CHECK(cleaner::ui::health_summary(three) == "3 pontos de atencao encontrados.");
}
