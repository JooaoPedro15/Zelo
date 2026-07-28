#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <storage/json_codec.hpp>

using Catch::Approx;
using zelo::core::ActionCategory;
using zelo::core::Confidence;
using zelo::core::Evidence;
using zelo::core::HealthCategory;
using zelo::core::HealthDeduction;
using zelo::core::HealthScore;
using zelo::core::Recommendation;
using zelo::core::RiskLevel;
using zelo::core::Severity;
using zelo::storage::StoredSession;
using zelo::storage::session_from_json;
using zelo::storage::to_json;

namespace {

StoredSession a_session() {
    Recommendation recommendation{
        .id = "startup.too-many-items",
        .rule_id = "startup.too-many-items",
        .rule_version = 1,
        .title = "8 programas nao essenciais iniciam com o Windows",
        .description = "Estes programas sobem junto com o Windows.",
        .category = ActionCategory::DisableStartupItem,
        .severity = Severity::Attention,
        .risk = RiskLevel::Yellow,
        .confidence = Confidence::from_signals({{"itens lidos do registro", 0.9}}),
        .evidence = {Evidence{.source = "chaves Run",
                              .description = "programas configurados para iniciar",
                              .value = "8 itens"}},
        .affected_paths = {"C:\\um.exe", "C:\\dois.exe"},
        .recommended_action = "Revisar a lista.",
        .alternative_action = "Manter como esta.",
        .undoable = true,
        .expected_result = "Inicializacao mais rapida.",
        .limitations = "O ganho varia entre computadores.",
    };

    StoredSession session;
    session.id = "2026-07-27_1430";
    session.started_at = "2026-07-27T14:30:00";
    session.app_version = "0.1.0";
    session.result.recommendations.push_back(std::move(recommendation));
    session.result.health = HealthScore::from_deductions(
        {HealthDeduction{HealthCategory::Startup, 10, "muitos programas na inicializacao"}});
    session.result.unavailable = {"arquivos temporarios"};

    return session;
}

}

TEST_CASE("a sessao sobrevive a ida e volta pelo JSON", "[json_codec]") {
    const StoredSession original = a_session();
    const StoredSession restored = session_from_json(to_json(original));

    CHECK(restored.id == original.id);
    CHECK(restored.started_at == original.started_at);
    CHECK(restored.app_version == original.app_version);
    CHECK(restored.result.unavailable == original.result.unavailable);

    REQUIRE(restored.result.recommendations.size() == 1);
    const auto& before = original.result.recommendations.front();
    const auto& after = restored.result.recommendations.front();

    CHECK(after.id == before.id);
    CHECK(after.rule_version == before.rule_version);
    CHECK(after.title == before.title);
    CHECK(after.category == before.category);
    CHECK(after.severity == before.severity);
    CHECK(after.risk == before.risk);
    CHECK(after.confidence.value() == Approx(before.confidence.value()));
    CHECK(after.confidence.reasons() == before.confidence.reasons());
    CHECK(after.affected_paths == before.affected_paths);
    CHECK(after.limitations == before.limitations);
    CHECK(after.undoable == before.undoable);
    CHECK(validate(after).empty());
}

TEST_CASE("a pontuacao e seus descontos sobrevivem a ida e volta", "[json_codec]") {
    const StoredSession restored = session_from_json(to_json(a_session()));

    CHECK(restored.result.health.of(HealthCategory::Startup) == 90);
    CHECK(restored.result.health.of(HealthCategory::Storage) == 100);

    const auto& deductions = restored.result.health.deductions_for(HealthCategory::Startup);
    REQUIRE(deductions.size() == 1);
    CHECK(deductions.front().cause == "muitos programas na inicializacao");
}

// Os enums viram nome, nao numero. Gravar o indice quebraria todo o historico
// no dia em que um valor fosse inserido no meio do enum.
TEST_CASE("os enums sao gravados como nome estavel", "[json_codec]") {
    const auto document = to_json(a_session());
    const auto& recommendation = document.at("recommendations").at(0);

    CHECK(recommendation.at("risk") == "yellow");
    CHECK(recommendation.at("severity") == "attention");
    CHECK(recommendation.at("category") == "disable_startup_item");
    CHECK(document.at("health_deductions").at(0).at("category") == "startup");
}

TEST_CASE("documento irreconhecivel e recusado", "[json_codec]") {
    CHECK_THROWS(session_from_json(nlohmann::json::object()));
    CHECK_THROWS(session_from_json(nlohmann::json::array()));
    CHECK_THROWS(session_from_json(nlohmann::json{{"schema", 9999}}));
}

// Historico gravado por uma versao futura pode conter categoria que esta versao
// nao conhece. O desconhecido cai no lado seguro em vez de virar permissao.
TEST_CASE("valor desconhecido cai no lado seguro", "[json_codec]") {
    auto document = to_json(a_session());
    document.at("recommendations").at(0).at("risk") = "cor-que-nao-existe";
    document.at("recommendations").at(0).at("category") = "categoria-que-nao-existe";

    const StoredSession restored = session_from_json(document);
    const auto& recommendation = restored.result.recommendations.front();

    CHECK(recommendation.risk == RiskLevel::Red);
    CHECK_FALSE(app_may_execute(recommendation));
    CHECK(recommendation.category == ActionCategory::ReadOnlyAnalysis);
}
