#include <catch2/catch_test_macros.hpp>
#include <core/risk/risk_classifier.hpp>

#include <vector>

using zelo::core::ActionCategory;
using zelo::core::ProtectedPaths;
using zelo::core::RiskClassifier;
using zelo::core::RiskLevel;

TEST_CASE("analise somente leitura e verde", "[risk_classifier]") {
    const RiskClassifier classifier{ProtectedPaths{{"C:\\Windows"}}};

    CHECK(classifier.classify(ActionCategory::ReadOnlyAnalysis) == RiskLevel::Green);
}

TEST_CASE("limpar a pasta Downloads exige revisao do usuario", "[risk_classifier]") {
    const RiskClassifier classifier{ProtectedPaths{{"C:\\Windows"}}};

    CHECK(classifier.classify(ActionCategory::DownloadsFolder) == RiskLevel::Yellow);
}

// Invariante do projeto: nao existe acao segura dentro de uma raiz protegida.
// Nem um temporario conhecido escapa — dentro do System32 ele vira vermelho.
TEST_CASE("caminho protegido rebaixa qualquer categoria para vermelho", "[risk_classifier]") {
    const RiskClassifier classifier{ProtectedPaths{{"C:\\Windows"}}};

    CHECK(classifier.classify(ActionCategory::KnownTemporaryFile, "D:\\Temp\\cache.tmp") ==
          RiskLevel::Green);

    CHECK(classifier.classify(ActionCategory::KnownTemporaryFile, "C:\\Windows\\Temp\\cache.tmp") ==
          RiskLevel::Red);
    CHECK(classifier.classify(ActionCategory::DownloadsFolder, "C:\\Windows\\System32") ==
          RiskLevel::Red);
}

namespace {

struct CatalogCase {
    ActionCategory category;
    RiskLevel expected;
    const char* name;
};

}

// O catalogo e a tabela revisada da secao 12 do planejamento. Cada categoria
// aparece aqui com o nivel que foi acordado, para que mudar um nivel exija
// mudar o teste — e portanto uma decisao consciente.
TEST_CASE("catalogo de risco classifica cada categoria no nivel acordado", "[risk_classifier]") {
    const RiskClassifier classifier{ProtectedPaths{{"C:\\Windows"}}};

    const std::vector<CatalogCase> cases{
        {ActionCategory::ReadOnlyAnalysis, RiskLevel::Green, "ReadOnlyAnalysis"},
        {ActionCategory::KnownTemporaryFile, RiskLevel::Green, "KnownTemporaryFile"},
        {ActionCategory::KnownCache, RiskLevel::Green, "KnownCache"},
        {ActionCategory::Thumbnail, RiskLevel::Green, "Thumbnail"},
        {ActionCategory::OldErrorReport, RiskLevel::Green, "OldErrorReport"},
        {ActionCategory::NonEssentialLog, RiskLevel::Green, "NonEssentialLog"},

        {ActionCategory::DownloadsFolder, RiskLevel::Yellow, "DownloadsFolder"},
        {ActionCategory::LargeFile, RiskLevel::Yellow, "LargeFile"},
        {ActionCategory::OldFile, RiskLevel::Yellow, "OldFile"},
        {ActionCategory::DuplicateFile, RiskLevel::Yellow, "DuplicateFile"},
        {ActionCategory::ProfessionalAppCache, RiskLevel::Yellow, "ProfessionalAppCache"},
        {ActionCategory::UserProject, RiskLevel::Yellow, "UserProject"},
        {ActionCategory::Recording, RiskLevel::Yellow, "Recording"},
        {ActionCategory::GameLibrary, RiskLevel::Yellow, "GameLibrary"},
        {ActionCategory::PossiblyUnusedProgram, RiskLevel::Yellow, "PossiblyUnusedProgram"},
        {ActionCategory::DisableStartupItem, RiskLevel::Yellow, "DisableStartupItem"},
        {ActionCategory::WindowsComponentCleanup, RiskLevel::Yellow, "WindowsComponentCleanup"},
        {ActionCategory::MoveUserFile, RiskLevel::Yellow, "MoveUserFile"},
        {ActionCategory::RepairRequiringReboot, RiskLevel::Yellow, "RepairRequiringReboot"},
        {ActionCategory::DeepDiskCheck, RiskLevel::Yellow, "DeepDiskCheck"},
        {ActionCategory::DriverUpdate, RiskLevel::Yellow, "DriverUpdate"},

        {ActionCategory::CriticalWindowsComponent, RiskLevel::Red, "CriticalWindowsComponent"},
        {ActionCategory::DriverModification, RiskLevel::Red, "DriverModification"},
        {ActionCategory::Firmware, RiskLevel::Red, "Firmware"},
        {ActionCategory::UnidentifiedSystemFile, RiskLevel::Red, "UnidentifiedSystemFile"},
        {ActionCategory::Registry, RiskLevel::Red, "Registry"},
        {ActionCategory::EssentialService, RiskLevel::Red, "EssentialService"},
        {ActionCategory::Partition, RiskLevel::Red, "Partition"},
        {ActionCategory::ProgramFolder, RiskLevel::Red, "ProgramFolder"},
        {ActionCategory::PersonalData, RiskLevel::Red, "PersonalData"},
        {ActionCategory::ApplicationDatabase, RiskLevel::Red, "ApplicationDatabase"},
        {ActionCategory::ProfessionalProject, RiskLevel::Red, "ProfessionalProject"},
        {ActionCategory::DataLossAction, RiskLevel::Red, "DataLossAction"},
        {ActionCategory::FormatCommand, RiskLevel::Red, "FormatCommand"},
        {ActionCategory::BootChange, RiskLevel::Red, "BootChange"},
        {ActionCategory::IrreversibleRepair, RiskLevel::Red, "IrreversibleRepair"},
    };

    for (const auto& item : cases) {
        INFO("categoria: " << item.name);
        CHECK(classifier.classify(item.category) == item.expected);
    }
}

// O switch exaustivo ja impede esquecer uma categoria em tempo de compilacao.
// Esta guarda cobre o que o compilador nao ve: um valor fora do enum vindo de
// cast ou de dado persistido de uma versao diferente.
TEST_CASE("categoria desconhecida cai em vermelho", "[risk_classifier]") {
    const RiskClassifier classifier{ProtectedPaths{{"C:\\Windows"}}};

    CHECK(classifier.classify(static_cast<ActionCategory>(9999)) == RiskLevel::Red);
}
