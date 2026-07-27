#pragma once

#include "core/risk/protected_paths.hpp"
#include "core/risk/risk_level.hpp"

#include <string_view>

namespace zelo::core {

/// Categoria da acao recomendada. O risco vem de uma tabela estatica revisada,
/// nunca de calculo em tempo de execucao.
enum class ActionCategory {
    // Verde — normalmente seguro.
    ReadOnlyAnalysis,
    KnownTemporaryFile,
    KnownCache,
    Thumbnail,
    OldErrorReport,
    NonEssentialLog,

    // Amarelo — exige revisao do usuario.
    DownloadsFolder,
    LargeFile,
    OldFile,
    DuplicateFile,
    ProfessionalAppCache,
    UserProject,
    Recording,
    GameLibrary,
    PossiblyUnusedProgram,
    DisableStartupItem,
    WindowsComponentCleanup,
    MoveUserFile,
    RepairRequiringReboot,
    DeepDiskCheck,
    DriverUpdate,

    // Vermelho — apenas explicado, nunca executado.
    CriticalWindowsComponent,
    DriverModification,
    Firmware,
    UnidentifiedSystemFile,
    Registry,
    EssentialService,
    Partition,
    ProgramFolder,
    PersonalData,
    ApplicationDatabase,
    ProfessionalProject,
    DataLossAction,
    FormatCommand,
    BootChange,
    IrreversibleRepair,
};

class RiskClassifier {
public:
    explicit RiskClassifier(ProtectedPaths protected_paths);

    [[nodiscard]] RiskLevel classify(ActionCategory category) const;

    /// Mesma tabela, mas um caminho sob raiz protegida sempre vence e devolve
    /// vermelho — nao existe categoria que autorize agir ali.
    [[nodiscard]] RiskLevel classify(ActionCategory category, std::string_view path) const;

private:
    ProtectedPaths protected_paths_;
};

}
