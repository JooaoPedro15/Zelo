#include <catch2/catch_test_macros.hpp>
#include <core/rules/excessive_temporary_files_rule.hpp>
#include <core/rules/low_free_space_rule.hpp>
#include <core/rules/too_many_startup_items_rule.hpp>

#include <string>

using zelo::core::ExcessiveTemporaryFilesRule;
using zelo::core::LowFreeSpaceRule;
using zelo::core::RiskLevel;
using zelo::core::StartupItemInfo;
using zelo::core::SystemSnapshot;
using zelo::core::TooManyStartupItemsRule;
using zelo::core::VolumeInfo;

namespace {

constexpr std::uint64_t kMegabyte = 1024ULL * 1024ULL;
constexpr std::uint64_t kGigabyte = 1024ULL * kMegabyte;

SystemSnapshot snapshot_with_system_volume(std::uint64_t total_gb, std::uint64_t free_gb) {
    SystemSnapshot snapshot;
    snapshot.volumes_available = true;
    snapshot.volumes.push_back(VolumeInfo{.letter = "C:",
                                          .total_bytes = total_gb * kGigabyte,
                                          .free_bytes = free_gb * kGigabyte,
                                          .is_system = true});
    return snapshot;
}

}

TEST_CASE("disco de sistema com pouco espaco livre gera recomendacao", "[rules]") {
    const LowFreeSpaceRule rule;

    const auto recommendations = rule.evaluate(snapshot_with_system_volume(500, 20));

    REQUIRE(recommendations.size() == 1);

    const auto& recommendation = recommendations.front();
    CHECK(recommendation.rule_id == rule.id());
    CHECK(recommendation.risk == RiskLevel::Green);
    CHECK_FALSE(recommendation.evidence.empty());
    CHECK_FALSE(recommendation.limitations.empty());
    CHECK(validate(recommendation).empty());
}

TEST_CASE("disco com espaco folgado nao gera recomendacao", "[rules]") {
    const LowFreeSpaceRule rule;

    CHECK(rule.evaluate(snapshot_with_system_volume(500, 300)).empty());
}

// Ausencia de dado nao pode virar conclusao: sem conseguir ler os volumes, a
// regra nao afirma nada em vez de supor que esta tudo bem.
TEST_CASE("sem dados de volume a regra nao conclui nada", "[rules]") {
    const LowFreeSpaceRule rule;
    SystemSnapshot snapshot;
    snapshot.volumes_available = false;

    CHECK(rule.evaluate(snapshot).empty());
}

TEST_CASE("acumulo de arquivos temporarios gera recomendacao", "[rules]") {
    const ExcessiveTemporaryFilesRule rule;

    SystemSnapshot snapshot;
    snapshot.temporary_files = {.available = true, .total_bytes = 12 * kGigabyte, .file_count = 8421};

    const auto recommendations = rule.evaluate(snapshot);

    REQUIRE(recommendations.size() == 1);

    const auto& recommendation = recommendations.front();
    CHECK(recommendation.risk == RiskLevel::Green);
    CHECK(recommendation.reclaimable_bytes == 12 * kGigabyte);
    CHECK(validate(recommendation).empty());
}

TEST_CASE("pouco temporario acumulado nao vira recomendacao", "[rules]") {
    const ExcessiveTemporaryFilesRule rule;

    SystemSnapshot snapshot;
    snapshot.temporary_files = {.available = true, .total_bytes = 200 * kMegabyte, .file_count = 40};

    CHECK(rule.evaluate(snapshot).empty());
}

// Sem ter conseguido varrer os temporarios, a regra nao pode dizer que esta
// tudo limpo — nao olhar e diferente de nao encontrar.
TEST_CASE("sem varredura de temporarios a regra nao conclui nada", "[rules]") {
    const ExcessiveTemporaryFilesRule rule;

    SystemSnapshot snapshot;
    snapshot.temporary_files = {.available = false, .total_bytes = 0, .file_count = 0};

    CHECK(rule.evaluate(snapshot).empty());
}

namespace {

SystemSnapshot snapshot_with_startup(int non_essential_count, int essential_count) {
    SystemSnapshot snapshot;
    snapshot.startup_available = true;

    for (int index = 0; index < non_essential_count; ++index) {
        snapshot.startup_items.push_back(
            StartupItemInfo{.name = "Programa " + std::to_string(index),
                            .publisher = "Fabricante",
                            .path = "C:\\Programas\\app" + std::to_string(index) + ".exe",
                            .essential = false});
    }
    for (int index = 0; index < essential_count; ++index) {
        snapshot.startup_items.push_back(
            StartupItemInfo{.name = "Antivirus " + std::to_string(index),
                            .publisher = "Fabricante de seguranca",
                            .path = "C:\\Program Files\\av" + std::to_string(index) + ".exe",
                            .essential = true});
    }
    return snapshot;
}

}

TEST_CASE("excesso de programas na inicializacao gera recomendacao", "[rules]") {
    const TooManyStartupItemsRule rule;

    const auto recommendations = rule.evaluate(snapshot_with_startup(8, 0));

    REQUIRE(recommendations.size() == 1);

    const auto& recommendation = recommendations.front();
    CHECK(recommendation.risk == RiskLevel::Yellow);
    CHECK(recommendation.affected_paths.size() == 8);
    CHECK(validate(recommendation).empty());
}

// Antivirus, drivers e componentes de audio e video nunca entram na conta nem
// na sugestao. Desativar um deles quebraria o sistema ou a protecao.
TEST_CASE("itens essenciais ficam fora da contagem e da sugestao", "[rules]") {
    const TooManyStartupItemsRule rule;

    SECTION("muitos essenciais nao disparam a regra") {
        CHECK(rule.evaluate(snapshot_with_startup(2, 12)).empty());
    }

    SECTION("nenhum caminho essencial aparece entre os afetados") {
        const auto recommendations = rule.evaluate(snapshot_with_startup(8, 5));

        REQUIRE(recommendations.size() == 1);

        const auto& affected = recommendations.front().affected_paths;
        CHECK(affected.size() == 8);
        for (const auto& path : affected) {
            CHECK(path.find("av") == std::string::npos);
        }
    }
}

TEST_CASE("sem dados de inicializacao a regra nao conclui nada", "[rules]") {
    const TooManyStartupItemsRule rule;

    SystemSnapshot snapshot;
    snapshot.startup_available = false;

    CHECK(rule.evaluate(snapshot).empty());
}
