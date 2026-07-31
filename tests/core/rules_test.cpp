#include <catch2/catch_test_macros.hpp>
#include <core/rules/excessive_temporary_files_rule.hpp>
#include <core/rules/low_free_space_rule.hpp>
#include <core/rules/low_memory_rule.hpp>
#include <core/rules/pending_reboot_rule.hpp>
#include <core/rules/recurring_app_failures_rule.hpp>
#include <core/rules/too_many_startup_items_rule.hpp>

#include <string>

using zelo::core::ExcessiveTemporaryFilesRule;
using zelo::core::LowFreeSpaceRule;
using zelo::core::LowMemoryRule;
using zelo::core::PendingRebootRule;
using zelo::core::RecurringAppFailuresRule;
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

// A gravidade acompanha o quanto o problema e grande. Tratar 9% e 2% livres
// como a mesma coisa faria a pontuacao dizer o mesmo em situacoes muito
// diferentes.
TEST_CASE("a gravidade cresce conforme o disco enche", "[rules]") {
    const LowFreeSpaceRule rule;

    const auto tight = rule.evaluate(snapshot_with_system_volume(500, 45));
    REQUIRE(tight.size() == 1);
    CHECK(tight.front().severity == zelo::core::Severity::Attention);

    const auto critical = rule.evaluate(snapshot_with_system_volume(500, 10));
    REQUIRE(critical.size() == 1);
    CHECK(critical.front().severity == zelo::core::Severity::Serious);
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
// O que e irrelevante num disco folgado passa a importar num disco apertado.
// Meio giga de temporario nao vale a atencao de quem tem 400 GB livres, mas e
// mais da metade do que resta para quem esta com menos de 1 GB.
TEST_CASE("o limite de temporarios cai quando o disco esta apertado", "[rules]") {
    const ExcessiveTemporaryFilesRule rule;

    SystemSnapshot snapshot;
    snapshot.temporary_files = {.available = true, .total_bytes = 550 * kMegabyte, .file_count = 900};
    snapshot.volumes_available = true;

    SECTION("com disco folgado, meio giga nao vira recomendacao") {
        snapshot.volumes = {{.letter = "C:",
                             .total_bytes = 500 * kGigabyte,
                             .free_bytes = 300 * kGigabyte,
                             .is_system = true}};

        CHECK(rule.evaluate(snapshot).empty());
    }

    SECTION("com disco quase cheio, o mesmo volume e recomendado") {
        snapshot.volumes = {{.letter = "C:",
                             .total_bytes = 222 * kGigabyte,
                             .free_bytes = 1 * kGigabyte,
                             .is_system = true}};

        const auto recommendations = rule.evaluate(snapshot);

        REQUIRE(recommendations.size() == 1);
        CHECK(recommendations.front().reclaimable_bytes == 550 * kMegabyte);
    }
}

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

TEST_CASE("a gravidade da inicializacao cresce com o excesso", "[rules]") {
    const TooManyStartupItemsRule rule;

    const auto some = rule.evaluate(snapshot_with_startup(7, 0));
    REQUIRE(some.size() == 1);
    CHECK(some.front().severity == zelo::core::Severity::Attention);

    const auto many = rule.evaluate(snapshot_with_startup(30, 0));
    REQUIRE(many.size() == 1);
    CHECK(many.front().severity == zelo::core::Severity::Serious);
}

// Cada regra declara em que area da saude o achado pesa. Antes isso era
// deduzido do tipo da acao, e falhas de aplicativo — que sao leitura, como
// quase tudo no MVP — acabavam descontando de Armazenamento.
TEST_CASE("cada regra desconta da area certa da saude", "[rules]") {
    using zelo::core::HealthCategory;

    SystemSnapshot snapshot;
    snapshot.volumes_available = true;
    snapshot.volumes = {{.letter = "C:",
                         .total_bytes = 500 * kGigabyte,
                         .free_bytes = 10 * kGigabyte,
                         .is_system = true}};
    snapshot.temporary_files = {.available = true, .total_bytes = 12 * kGigabyte, .file_count = 10};
    snapshot.startup_available = true;
    snapshot.startup_items = snapshot_with_startup(8, 0).startup_items;
    snapshot.stability = {.available = true,
                          .app_failures = {{.application = "Editor.exe", .count = 5}},
                          .unexpected_shutdowns = 0,
                          .window_days = 30};
    snapshot.updates = {.available = true, .reboot_pending = true, .reboot_reasons = {"teste"}};
    snapshot.memory = {.available = true,
                       .total_bytes = 16 * kGigabyte,
                       .available_bytes = 1 * kGigabyte,
                       .load_percent = 94};

    const auto check = [&snapshot](const zelo::core::AnalysisRule& rule, HealthCategory expected) {
        const auto found = rule.evaluate(snapshot);
        REQUIRE(found.size() == 1);
        CHECK(found.front().health_category == expected);
    };

    check(LowFreeSpaceRule{}, HealthCategory::Storage);
    check(ExcessiveTemporaryFilesRule{}, HealthCategory::Storage);
    check(TooManyStartupItemsRule{}, HealthCategory::Startup);
    check(RecurringAppFailuresRule{}, HealthCategory::Stability);
    check(PendingRebootRule{}, HealthCategory::Updates);
    check(LowMemoryRule{}, HealthCategory::Performance);
}

TEST_CASE("sem dados de inicializacao a regra nao conclui nada", "[rules]") {
    const TooManyStartupItemsRule rule;

    SystemSnapshot snapshot;
    snapshot.startup_available = false;

    CHECK(rule.evaluate(snapshot).empty());
}
