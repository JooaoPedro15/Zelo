#include "core/risk/risk_classifier.hpp"

#include <utility>

namespace cleaner::core {

RiskClassifier::RiskClassifier(ProtectedPaths protected_paths)
    : protected_paths_(std::move(protected_paths)) {}

RiskLevel RiskClassifier::classify(ActionCategory category) const {
    switch (category) {
    case ActionCategory::ReadOnlyAnalysis:
    case ActionCategory::KnownTemporaryFile:
    case ActionCategory::KnownCache:
    case ActionCategory::Thumbnail:
    case ActionCategory::OldErrorReport:
    case ActionCategory::NonEssentialLog:
        return RiskLevel::Green;

    case ActionCategory::DownloadsFolder:
    case ActionCategory::LargeFile:
    case ActionCategory::OldFile:
    case ActionCategory::DuplicateFile:
    case ActionCategory::ProfessionalAppCache:
    case ActionCategory::UserProject:
    case ActionCategory::Recording:
    case ActionCategory::GameLibrary:
    case ActionCategory::PossiblyUnusedProgram:
    case ActionCategory::DisableStartupItem:
    case ActionCategory::WindowsComponentCleanup:
    case ActionCategory::MoveUserFile:
    case ActionCategory::RepairRequiringReboot:
    case ActionCategory::DeepDiskCheck:
    case ActionCategory::DriverUpdate:
        return RiskLevel::Yellow;

    case ActionCategory::CriticalWindowsComponent:
    case ActionCategory::DriverModification:
    case ActionCategory::Firmware:
    case ActionCategory::UnidentifiedSystemFile:
    case ActionCategory::Registry:
    case ActionCategory::EssentialService:
    case ActionCategory::Partition:
    case ActionCategory::ProgramFolder:
    case ActionCategory::PersonalData:
    case ActionCategory::ApplicationDatabase:
    case ActionCategory::ProfessionalProject:
    case ActionCategory::DataLossAction:
    case ActionCategory::FormatCommand:
    case ActionCategory::BootChange:
    case ActionCategory::IrreversibleRepair:
        return RiskLevel::Red;
    }

    // Sem `default:` de proposito: o switch acima e exaustivo, entao adicionar
    // uma categoria sem classifica-la quebra a compilacao (-Werror=switch).
    // Este retorno so cobre um valor fora do enum vindo de cast.
    return RiskLevel::Red;
}

RiskLevel RiskClassifier::classify(ActionCategory category, std::string_view path) const {
    if (protected_paths_.is_protected(path)) {
        return RiskLevel::Red;
    }
    return classify(category);
}

}
