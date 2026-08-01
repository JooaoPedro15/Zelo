#include <catch2/catch_test_macros.hpp>
#include <monitor/growth_alerts.hpp>

#include <algorithm>

using cleaner::monitor::Alert;
using cleaner::monitor::AlertKind;
using cleaner::monitor::AlertThresholds;
using cleaner::monitor::GrowthItem;
using cleaner::monitor::GrowthReport;
using cleaner::monitor::Snapshot;
using cleaner::monitor::evaluate_alerts;

namespace {

constexpr std::uint64_t kGigabyte = 1024ULL * 1024 * 1024;

Snapshot snapshot_with(std::uint64_t free_bytes, bool complete = true) {
    Snapshot snapshot;
    snapshot.volume = "C:";
    snapshot.taken_at = "2026-08-01T10:00:00";
    snapshot.total_bytes = 500 * kGigabyte;
    snapshot.free_bytes = free_bytes;
    snapshot.complete = complete;
    return snapshot;
}

bool has(const std::vector<Alert>& alerts, AlertKind kind) {
    return std::any_of(alerts.begin(), alerts.end(),
                       [kind](const Alert& alert) { return alert.kind == kind; });
}

}

TEST_CASE("disco quase cheio gera alerta", "[growth_alerts]") {
    const auto alerts = evaluate_alerts(snapshot_with(2 * kGigabyte), GrowthReport{});

    REQUIRE(has(alerts, AlertKind::LowFreeSpace));

    // Todo alerta mostra o numero que o sustenta: avisar sem poder comprovar e
    // alarmismo.
    CHECK_FALSE(alerts.front().evidence.empty());
    CHECK_FALSE(alerts.front().suggested_action.empty());
}

TEST_CASE("disco folgado nao gera alerta", "[growth_alerts]") {
    CHECK(evaluate_alerts(snapshot_with(300 * kGigabyte), GrowthReport{}).empty());
}

// Um retrato interrompido nao viu o disco inteiro. Alertar com base nele
// mandaria o usuario procurar espaco que nunca sumiu.
TEST_CASE("retrato incompleto nao sustenta alerta", "[growth_alerts]") {
    CHECK(evaluate_alerts(snapshot_with(1 * kGigabyte, false), GrowthReport{}).empty());
}

TEST_CASE("consumo rapido gera alerta com as pastas responsaveis", "[growth_alerts]") {
    GrowthReport growth;
    growth.from_taken_at = "2026-07-31T10:00:00";
    growth.to_taken_at = "2026-08-01T10:00:00";
    growth.free_space_delta = -7 * static_cast<std::int64_t>(kGigabyte);
    growth.items = {
        GrowthItem{.path = "C:\\A", .exclusive_bytes = 4 * static_cast<std::int64_t>(kGigabyte)},
        GrowthItem{.path = "C:\\B", .exclusive_bytes = 3 * static_cast<std::int64_t>(kGigabyte)},
    };

    const auto alerts = evaluate_alerts(snapshot_with(300 * kGigabyte), growth);

    REQUIRE(has(alerts, AlertKind::FastGrowth));

    const auto rapido = std::find_if(alerts.begin(), alerts.end(), [](const Alert& alert) {
        return alert.kind == AlertKind::FastGrowth;
    });
    CHECK(rapido->folders.size() == 2);
    CHECK_FALSE(rapido->since.empty());
}

TEST_CASE("pasta que deu um salto vira alerta proprio", "[growth_alerts]") {
    GrowthReport growth;
    growth.from_taken_at = "2026-07-31T10:00:00";
    growth.items = {
        GrowthItem{.path = "C:\\enorme", .exclusive_bytes = 3 * static_cast<std::int64_t>(kGigabyte)},
        GrowthItem{.path = "C:\\pequena", .exclusive_bytes = 100 * 1024 * 1024},
    };

    const auto alerts = evaluate_alerts(snapshot_with(300 * kGigabyte), growth);

    const auto saltos = std::count_if(alerts.begin(), alerts.end(), [](const Alert& alert) {
        return alert.kind == AlertKind::FolderJump;
    });

    CHECK(saltos == 1);
}

// Disco grande com 10 GB livres ainda pode estar apertado em proporcao.
TEST_CASE("a proporcao livre tambem conta", "[growth_alerts]") {
    Snapshot enorme;
    enorme.volume = "C:";
    enorme.total_bytes = 4000 * kGigabyte;
    enorme.free_bytes = 100 * kGigabyte;
    enorme.complete = true;

    CHECK(has(evaluate_alerts(enorme, GrowthReport{}), AlertKind::LowFreeSpace));
}

TEST_CASE("os limites sao configuraveis", "[growth_alerts]") {
    const AlertThresholds relaxado{.minimum_free_bytes = 1 * kGigabyte,
                                   .minimum_free_ratio = 0.001};

    CHECK(evaluate_alerts(snapshot_with(2 * kGigabyte), GrowthReport{}, relaxado).empty());
}
