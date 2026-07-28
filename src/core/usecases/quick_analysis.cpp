#include "core/usecases/quick_analysis.hpp"

#include "core/rules/excessive_temporary_files_rule.hpp"
#include "core/rules/low_free_space_rule.hpp"
#include "core/rules/recurring_app_failures_rule.hpp"
#include "core/rules/too_many_startup_items_rule.hpp"

#include <utility>

namespace zelo::core {

namespace {

/// Quanto cada achado tira da pontuacao. Os valores saem dos exemplos da secao
/// 13 do planejamento e ficam fixados em teste, para que mudar o peso de um
/// problema seja uma decisao consciente.
int deduction_for(Severity severity) {
    switch (severity) {
    case Severity::Info:
        return 3;
    case Severity::Attention:
        return 10;
    case Severity::Serious:
        return 20;
    }
    return 3;
}

HealthCategory category_for(const Recommendation& recommendation) {
    switch (recommendation.category) {
    case ActionCategory::DisableStartupItem:
        return HealthCategory::Startup;

    case ActionCategory::KnownTemporaryFile:
    case ActionCategory::KnownCache:
    case ActionCategory::Thumbnail:
    case ActionCategory::DownloadsFolder:
    case ActionCategory::LargeFile:
    case ActionCategory::OldFile:
    case ActionCategory::DuplicateFile:
    case ActionCategory::ProfessionalAppCache:
    case ActionCategory::UserProject:
    case ActionCategory::Recording:
    case ActionCategory::GameLibrary:
    case ActionCategory::MoveUserFile:
        return HealthCategory::Storage;

    case ActionCategory::DeepDiskCheck:
        return HealthCategory::Disks;

    case ActionCategory::OldErrorReport:
    case ActionCategory::NonEssentialLog:
    case ActionCategory::RepairRequiringReboot:
    case ActionCategory::WindowsComponentCleanup:
    case ActionCategory::CriticalWindowsComponent:
    case ActionCategory::UnidentifiedSystemFile:
    case ActionCategory::Registry:
    case ActionCategory::EssentialService:
    case ActionCategory::IrreversibleRepair:
        return HealthCategory::WindowsIntegrity;

    case ActionCategory::DriverUpdate:
    case ActionCategory::DriverModification:
    case ActionCategory::Firmware:
        return HealthCategory::Performance;

    case ActionCategory::PossiblyUnusedProgram:
    case ActionCategory::ProgramFolder:
    case ActionCategory::ApplicationDatabase:
    case ActionCategory::PersonalData:
    case ActionCategory::ProfessionalProject:
    case ActionCategory::Partition:
    case ActionCategory::DataLossAction:
    case ActionCategory::FormatCommand:
    case ActionCategory::BootChange:
        return HealthCategory::Storage;

    case ActionCategory::ReadOnlyAnalysis:
        break;
    }
    return HealthCategory::Storage;
}

}

QuickAnalysis::QuickAnalysis(std::vector<std::shared_ptr<const AnalysisRule>> rules)
    : rules_(std::move(rules)) {}

QuickAnalysis QuickAnalysis::with_default_rules() {
    return QuickAnalysis{{
        std::make_shared<const LowFreeSpaceRule>(),
        std::make_shared<const ExcessiveTemporaryFilesRule>(),
        std::make_shared<const TooManyStartupItemsRule>(),
        std::make_shared<const RecurringAppFailuresRule>(),
    }};
}

AnalysisResult QuickAnalysis::run(const SystemSnapshot& snapshot) const {
    AnalysisResult result;

    std::vector<HealthDeduction> deductions;

    for (const auto& rule : rules_) {
        for (auto& recommendation : rule->evaluate(snapshot)) {
            // Achado sem evidencia nao chega ao usuario. Se uma regra produzir
            // algo incompleto, o problema fica no achado, nao na analise toda.
            if (!validate(recommendation).empty()) {
                continue;
            }

            deductions.push_back(HealthDeduction{category_for(recommendation),
                                                 deduction_for(recommendation.severity),
                                                 recommendation.title});
            result.recommendations.push_back(std::move(recommendation));
        }
    }

    if (!snapshot.volumes_available) {
        result.unavailable.emplace_back("espaco em disco");
    }
    if (!snapshot.temporary_files.available) {
        result.unavailable.emplace_back("arquivos temporarios");
    }
    if (!snapshot.startup_available) {
        result.unavailable.emplace_back("programas de inicializacao");
    }
    if (!snapshot.stability.available) {
        result.unavailable.emplace_back("falhas registradas pelo Windows");
    }

    result.health = HealthScore::from_deductions(std::move(deductions));
    return result;
}

}
