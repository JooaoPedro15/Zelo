#include "storage/json_codec.hpp"

#include <array>
#include <stdexcept>
#include <utility>

namespace cleaner::storage {

namespace {

using core::ActionCategory;
using core::HealthCategory;
using core::RiskLevel;
using core::Severity;

template <typename Enumeration, std::size_t Count>
std::string name_of(Enumeration value,
                    const std::array<std::pair<Enumeration, const char*>, Count>& names) {
    for (const auto& [candidate, name] : names) {
        if (candidate == value) {
            return name;
        }
    }
    return "unknown";
}

template <typename Enumeration, std::size_t Count>
Enumeration value_of(const std::string& name,
                     const std::array<std::pair<Enumeration, const char*>, Count>& names,
                     Enumeration fallback) {
    for (const auto& [candidate, candidate_name] : names) {
        if (name == candidate_name) {
            return candidate;
        }
    }
    return fallback;
}

constexpr std::array<std::pair<RiskLevel, const char*>, 3> kRiskNames{{
    {RiskLevel::Green, "green"},
    {RiskLevel::Yellow, "yellow"},
    {RiskLevel::Red, "red"},
}};

constexpr std::array<std::pair<Severity, const char*>, 3> kSeverityNames{{
    {Severity::Info, "info"},
    {Severity::Attention, "attention"},
    {Severity::Serious, "serious"},
}};

constexpr std::array<std::pair<HealthCategory, const char*>, 8> kHealthCategoryNames{{
    {HealthCategory::Storage, "storage"},
    {HealthCategory::WindowsIntegrity, "windows_integrity"},
    {HealthCategory::Startup, "startup"},
    {HealthCategory::Disks, "disks"},
    {HealthCategory::Updates, "updates"},
    {HealthCategory::Security, "security"},
    {HealthCategory::Performance, "performance"},
    {HealthCategory::Stability, "stability"},
}};

constexpr std::array<std::pair<ActionCategory, const char*>, 36> kActionCategoryNames{{
    {ActionCategory::ReadOnlyAnalysis, "read_only_analysis"},
    {ActionCategory::KnownTemporaryFile, "known_temporary_file"},
    {ActionCategory::KnownCache, "known_cache"},
    {ActionCategory::Thumbnail, "thumbnail"},
    {ActionCategory::OldErrorReport, "old_error_report"},
    {ActionCategory::NonEssentialLog, "non_essential_log"},
    {ActionCategory::DownloadsFolder, "downloads_folder"},
    {ActionCategory::LargeFile, "large_file"},
    {ActionCategory::OldFile, "old_file"},
    {ActionCategory::DuplicateFile, "duplicate_file"},
    {ActionCategory::ProfessionalAppCache, "professional_app_cache"},
    {ActionCategory::UserProject, "user_project"},
    {ActionCategory::Recording, "recording"},
    {ActionCategory::GameLibrary, "game_library"},
    {ActionCategory::PossiblyUnusedProgram, "possibly_unused_program"},
    {ActionCategory::DisableStartupItem, "disable_startup_item"},
    {ActionCategory::WindowsComponentCleanup, "windows_component_cleanup"},
    {ActionCategory::MoveUserFile, "move_user_file"},
    {ActionCategory::RepairRequiringReboot, "repair_requiring_reboot"},
    {ActionCategory::DeepDiskCheck, "deep_disk_check"},
    {ActionCategory::DriverUpdate, "driver_update"},
    {ActionCategory::CriticalWindowsComponent, "critical_windows_component"},
    {ActionCategory::DriverModification, "driver_modification"},
    {ActionCategory::Firmware, "firmware"},
    {ActionCategory::UnidentifiedSystemFile, "unidentified_system_file"},
    {ActionCategory::Registry, "registry"},
    {ActionCategory::EssentialService, "essential_service"},
    {ActionCategory::Partition, "partition"},
    {ActionCategory::ProgramFolder, "program_folder"},
    {ActionCategory::PersonalData, "personal_data"},
    {ActionCategory::ApplicationDatabase, "application_database"},
    {ActionCategory::ProfessionalProject, "professional_project"},
    {ActionCategory::DataLossAction, "data_loss_action"},
    {ActionCategory::FormatCommand, "format_command"},
    {ActionCategory::BootChange, "boot_change"},
    {ActionCategory::IrreversibleRepair, "irreversible_repair"},
}};

nlohmann::json evidence_to_json(const core::Evidence& evidence) {
    return {
        {"source", evidence.source},
        {"description", evidence.description},
        {"value", evidence.value},
    };
}

nlohmann::json recommendation_to_json(const core::Recommendation& recommendation) {
    return {
        {"id", recommendation.id},
        {"rule_id", recommendation.rule_id},
        {"rule_version", recommendation.rule_version},
        {"title", recommendation.title},
        {"description", recommendation.description},
        {"category", to_string(recommendation.category)},
        {"severity", to_string(recommendation.severity)},
        {"risk", to_string(recommendation.risk)},
        {"confidence",
         {{"value", recommendation.confidence.value()},
          {"reasons", recommendation.confidence.reasons()}}},
        {"evidence",
         [&recommendation] {
             nlohmann::json items = nlohmann::json::array();
             for (const auto& evidence : recommendation.evidence) {
                 items.push_back(evidence_to_json(evidence));
             }
             return items;
         }()},
        {"size_bytes", recommendation.size_bytes},
        {"reclaimable_bytes", recommendation.reclaimable_bytes},
        {"affected_paths", recommendation.affected_paths},
        {"recommended_action", recommendation.recommended_action},
        {"alternative_action", recommendation.alternative_action},
        {"tool", recommendation.tool},
        {"command_id", recommendation.command_id.value_or("")},
        {"requires_admin", recommendation.requires_admin},
        {"requires_internet", recommendation.requires_internet},
        {"requires_reboot", recommendation.requires_reboot},
        {"cancellable", recommendation.cancellable},
        {"undoable", recommendation.undoable},
        {"expected_result", recommendation.expected_result},
        {"limitations", recommendation.limitations},
    };
}

core::Evidence evidence_from_json(const nlohmann::json& document) {
    return core::Evidence{
        .source = document.value("source", std::string{}),
        .description = document.value("description", std::string{}),
        .value = document.value("value", std::string{}),
    };
}

core::Recommendation recommendation_from_json(const nlohmann::json& document) {
    core::Recommendation recommendation;

    recommendation.id = document.value("id", std::string{});
    recommendation.rule_id = document.value("rule_id", std::string{});
    recommendation.rule_version = document.value("rule_version", 0);
    recommendation.title = document.value("title", std::string{});
    recommendation.description = document.value("description", std::string{});

    // Um valor desconhecido cai no lado seguro: categoria vira analise somente
    // leitura e risco vira vermelho, entao historico de uma versao mais nova
    // nunca vira permissao para agir.
    recommendation.category = value_of(document.value("category", std::string{}),
                                       kActionCategoryNames, ActionCategory::ReadOnlyAnalysis);
    recommendation.severity =
        value_of(document.value("severity", std::string{}), kSeverityNames, Severity::Info);
    recommendation.risk =
        value_of(document.value("risk", std::string{}), kRiskNames, RiskLevel::Red);

    const auto confidence = document.value("confidence", nlohmann::json::object());
    recommendation.confidence =
        core::Confidence::restored(confidence.value("value", 0.0),
                                   confidence.value("reasons", std::vector<std::string>{}));

    for (const auto& evidence : document.value("evidence", nlohmann::json::array())) {
        recommendation.evidence.push_back(evidence_from_json(evidence));
    }

    recommendation.size_bytes = document.value("size_bytes", std::uint64_t{0});
    recommendation.reclaimable_bytes = document.value("reclaimable_bytes", std::uint64_t{0});
    recommendation.affected_paths = document.value("affected_paths", std::vector<std::string>{});
    recommendation.recommended_action = document.value("recommended_action", std::string{});
    recommendation.alternative_action = document.value("alternative_action", std::string{});
    recommendation.tool = document.value("tool", std::string{});

    const auto command_id = document.value("command_id", std::string{});
    if (!command_id.empty()) {
        recommendation.command_id = command_id;
    }

    recommendation.requires_admin = document.value("requires_admin", false);
    recommendation.requires_internet = document.value("requires_internet", false);
    recommendation.requires_reboot = document.value("requires_reboot", false);
    recommendation.cancellable = document.value("cancellable", true);
    recommendation.undoable = document.value("undoable", false);
    recommendation.expected_result = document.value("expected_result", std::string{});
    recommendation.limitations = document.value("limitations", std::string{});

    return recommendation;
}

}

std::string to_string(RiskLevel risk) {
    return name_of(risk, kRiskNames);
}

std::string to_string(Severity severity) {
    return name_of(severity, kSeverityNames);
}

std::string to_string(ActionCategory category) {
    return name_of(category, kActionCategoryNames);
}

std::string to_string(HealthCategory category) {
    return name_of(category, kHealthCategoryNames);
}

nlohmann::json to_json(const StoredSession& session) {
    nlohmann::json recommendations = nlohmann::json::array();
    for (const auto& recommendation : session.result.recommendations) {
        recommendations.push_back(recommendation_to_json(recommendation));
    }

    nlohmann::json deductions = nlohmann::json::array();
    for (const auto& [category, name] : kHealthCategoryNames) {
        for (const auto& deduction : session.result.health.deductions_for(category)) {
            deductions.push_back({
                {"category", name},
                {"points", deduction.points},
                {"cause", deduction.cause},
            });
        }
    }

    return {
        {"schema", kSchemaVersion},
        {"id", session.id},
        {"started_at", session.started_at},
        {"app_version", session.app_version},
        {"recommendations", std::move(recommendations)},
        {"health_deductions", std::move(deductions)},
        {"unavailable", session.result.unavailable},
    };
}

StoredSession session_from_json(const nlohmann::json& document) {
    if (!document.is_object() || !document.contains("schema")) {
        throw std::runtime_error("documento nao parece uma sessao do Cleaner");
    }
    if (document.at("schema").get<int>() > kSchemaVersion) {
        throw std::runtime_error("sessao gravada por uma versao mais nova do aplicativo");
    }

    StoredSession session;
    session.id = document.value("id", std::string{});
    session.started_at = document.value("started_at", std::string{});
    session.app_version = document.value("app_version", std::string{});

    for (const auto& recommendation : document.value("recommendations", nlohmann::json::array())) {
        session.result.recommendations.push_back(recommendation_from_json(recommendation));
    }

    std::vector<core::HealthDeduction> deductions;
    for (const auto& deduction : document.value("health_deductions", nlohmann::json::array())) {
        deductions.push_back(core::HealthDeduction{
            value_of(deduction.value("category", std::string{}), kHealthCategoryNames,
                     HealthCategory::Storage),
            deduction.value("points", 0),
            deduction.value("cause", std::string{}),
        });
    }
    session.result.health = core::HealthScore::from_deductions(std::move(deductions));
    session.result.unavailable = document.value("unavailable", std::vector<std::string>{});

    return session;
}

}
