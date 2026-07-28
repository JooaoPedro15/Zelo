#include <catch2/catch_test_macros.hpp>
#include <core/usecases/quick_analysis.hpp>

#include <cstdint>

using zelo::core::AnalysisResult;
using zelo::core::HealthCategory;
using zelo::core::QuickAnalysis;
using zelo::core::StartupItemInfo;
using zelo::core::SystemSnapshot;
using zelo::core::VolumeInfo;

namespace {

constexpr std::uint64_t kGigabyte = 1024ULL * 1024ULL * 1024ULL;

SystemSnapshot healthy_snapshot() {
    SystemSnapshot snapshot;

    snapshot.volumes_available = true;
    snapshot.volumes.push_back(VolumeInfo{.letter = "C:",
                                          .total_bytes = 500 * kGigabyte,
                                          .free_bytes = 300 * kGigabyte,
                                          .is_system = true});

    snapshot.temporary_files = {.available = true, .total_bytes = 0, .file_count = 0};
    snapshot.startup_available = true;
    snapshot.stability.available = true;
    snapshot.stability.window_days = 30;

    snapshot.updates = {.available = true, .reboot_pending = false, .reboot_reasons = {}};
    snapshot.memory = {.available = true,
                       .total_bytes = 16 * kGigabyte,
                       .available_bytes = 8 * kGigabyte,
                       .load_percent = 50};
    snapshot.security = {.available = true,
                         .antivirus_enabled = true,
                         .realtime_protection_enabled = true,
                         .signature_age_days = 1,
                         .last_quick_scan_age_days = 2,
                         .provider = "Seguranca do Windows"};

    return snapshot;
}

}

TEST_CASE("maquina saudavel nao gera achados e mantem a pontuacao cheia", "[quick_analysis]") {
    const auto analysis = QuickAnalysis::with_default_rules();

    const AnalysisResult result = analysis.run(healthy_snapshot());

    CHECK(result.recommendations.empty());
    CHECK(result.health.overall() == 100);
    CHECK(result.unavailable.empty());
}

TEST_CASE("cada achado desconta na categoria certa", "[quick_analysis]") {
    SystemSnapshot snapshot = healthy_snapshot();
    snapshot.volumes.front().free_bytes = 10 * kGigabyte;
    snapshot.temporary_files = {.available = true, .total_bytes = 12 * kGigabyte, .file_count = 900};
    for (int index = 0; index < 8; ++index) {
        snapshot.startup_items.push_back(
            StartupItemInfo{.name = "App" + std::to_string(index), .path = "C:\\app.exe"});
    }

    const auto analysis = QuickAnalysis::with_default_rules();
    const AnalysisResult result = analysis.run(snapshot);

    REQUIRE(result.recommendations.size() == 3);

    CHECK(result.health.of(HealthCategory::Storage) < 100);
    CHECK(result.health.of(HealthCategory::Startup) < 100);
    CHECK(result.health.overall() < 100);

    // Nada foi observado sobre discos, entao a categoria nao pode ser punida
    // por um problema que ninguem mediu.
    CHECK(result.health.of(HealthCategory::Disks) == 100);
}

// Todo desconto precisa poder ser explicado. Uma pontuacao que cai sem causa
// visivel e exatamente o que o produto promete nao fazer.
TEST_CASE("todo desconto carrega a causa que o originou", "[quick_analysis]") {
    SystemSnapshot snapshot = healthy_snapshot();
    snapshot.volumes.front().free_bytes = 5 * kGigabyte;

    const auto analysis = QuickAnalysis::with_default_rules();
    const AnalysisResult result = analysis.run(snapshot);

    const auto& storage = result.health.deductions_for(HealthCategory::Storage);

    REQUIRE_FALSE(storage.empty());
    for (const auto& deduction : storage) {
        CHECK_FALSE(deduction.cause.empty());
        CHECK(deduction.points > 0);
    }
}

// O que nao foi observado precisa ser dito. Silencio faria o usuario concluir
// que esta tudo bem numa area que sequer foi analisada.
TEST_CASE("o que nao pode ser observado e declarado", "[quick_analysis]") {
    SystemSnapshot snapshot;

    const auto analysis = QuickAnalysis::with_default_rules();
    const AnalysisResult result = analysis.run(snapshot);

    CHECK(result.recommendations.empty());
    CHECK(result.health.overall() == 100);

    // Sete areas observaveis, todas declaradas como nao observadas.
    CHECK(result.unavailable.size() == 7);
}
