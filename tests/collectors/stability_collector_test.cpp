#include <catch2/catch_test_macros.hpp>
#include <collectors/stability_collector.hpp>
#include <core/rules/recurring_app_failures_rule.hpp>

using zelo::collectors::StabilityCollector;
using zelo::core::AppFailureInfo;
using zelo::core::RecurringAppFailuresRule;
using zelo::core::RiskLevel;
using zelo::core::SystemSnapshot;

namespace {

SystemSnapshot snapshot_with_failures(std::size_t count) {
    SystemSnapshot snapshot;
    snapshot.stability.available = true;
    snapshot.stability.window_days = 30;
    snapshot.stability.app_failures.push_back(AppFailureInfo{.application = "Editor.exe",
                                                             .faulting_module = "ntdll.dll",
                                                             .count = count,
                                                             .first_seen = "2026-07-01T10:00:00Z",
                                                             .last_seen = "2026-07-20T10:00:00Z"});
    return snapshot;
}

}

// Uma falha isolada acontece e nao diz nada. So a repeticao vira recomendacao.
TEST_CASE("falha isolada nao vira recomendacao", "[stability]") {
    const RecurringAppFailuresRule rule;

    CHECK(rule.evaluate(snapshot_with_failures(1)).empty());
    CHECK(rule.evaluate(snapshot_with_failures(2)).empty());
}

TEST_CASE("falhas repetidas viram recomendacao explicavel", "[stability]") {
    const RecurringAppFailuresRule rule;

    const auto recommendations = rule.evaluate(snapshot_with_failures(7));

    REQUIRE(recommendations.size() == 1);
    const auto& recommendation = recommendations.front();

    CHECK(recommendation.risk == RiskLevel::Green);
    CHECK(validate(recommendation).empty());
    REQUIRE_FALSE(recommendation.evidence.empty());

    // A evidencia precisa dizer quantas vezes e qual componente, senao o
    // usuario nao tem como julgar.
    CHECK(recommendation.evidence.front().value.find("7 falhas") != std::string::npos);
    CHECK(recommendation.evidence.front().value.find("ntdll.dll") != std::string::npos);

    // O produto nao pode sugerir que sabe a causa: o log mostra que falhou,
    // nao por que falhou.
    CHECK(recommendation.limitations.find("nao por que") != std::string::npos);
}

TEST_CASE("sem leitura de eventos a regra nao conclui nada", "[stability]") {
    const RecurringAppFailuresRule rule;

    SystemSnapshot snapshot = snapshot_with_failures(10);
    snapshot.stability.available = false;

    CHECK(rule.evaluate(snapshot).empty());
}

TEST_CASE("o registro de eventos da maquina e lido", "[stability][integration]") {
    const StabilityCollector collector;
    const auto stability = collector.collect();

    INFO("programas com falha: " << stability.app_failures.size());
    INFO("desligamentos inesperados: " << stability.unexpected_shutdowns);

    CHECK(stability.available);
    CHECK(stability.window_days == 30);

    for (const auto& failure : stability.app_failures) {
        INFO("falha: " << failure.application << " x" << failure.count);
        CHECK_FALSE(failure.application.empty());
        CHECK(failure.count > 0);
    }
}
