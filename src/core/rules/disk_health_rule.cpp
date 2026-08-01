#include "core/rules/disk_health_rule.hpp"

#include "core/rules/format.hpp"

#include <algorithm>

namespace cleaner::core {

std::string DiskHealthRule::id() const {
    return "disks.physical-warning";
}

int DiskHealthRule::version() const {
    return 1;
}

std::vector<Recommendation> DiskHealthRule::evaluate(const SystemSnapshot& snapshot) const {
    if (!snapshot.disks.available) {
        return {};
    }

    std::vector<Evidence> evidence;
    std::vector<std::string> affected;

    for (const auto& disk : snapshot.disks.disks) {
        const bool reported_unhealthy =
            disk.health_status == "Warning" || disk.health_status == "Unhealthy";
        const bool has_errors = disk.read_errors >= kConcerningErrors ||
                                disk.write_errors >= kConcerningErrors;

        if (!reported_unhealthy && !has_errors) {
            continue;
        }

        std::string detail;
        if (reported_unhealthy) {
            detail = "o Windows classificou o disco como " + disk.health_status;
        }
        if (has_errors) {
            if (!detail.empty()) {
                detail += "; ";
            }
            detail += "erros de leitura: " + std::to_string(std::max(disk.read_errors, 0)) +
                      ", de gravacao: " + std::to_string(std::max(disk.write_errors, 0));
        }

        evidence.push_back(Evidence{.source = "informacoes de armazenamento do Windows",
                                    .description = "estado do disco " + disk.model,
                                    .value = detail});
        affected.push_back(disk.model);
    }

    if (evidence.empty()) {
        return {};
    }

    return {Recommendation{
        .id = id(),
        .rule_id = id(),
        .rule_version = version(),
        .title = affected.size() == 1 ? "O disco " + affected.front() + " apresenta sinais de problema"
                                      : "Ha discos apresentando sinais de problema",
        .description =
            "O Windows registrou indicios de problema fisico. Isso nao e questao de limpeza nem "
            "de reparo por comando: nenhum programa conserta um disco que esta falhando. O que "
            "importa agora e proteger os arquivos.",
        .category = ActionCategory::ReadOnlyAnalysis,
        .health_category = HealthCategory::Disks,
        .severity = Severity::Serious,
        .risk = RiskLevel::Green,
        .confidence = Confidence::from_signals(
            {{"indicadores informados pelo proprio sistema de armazenamento", 0.7}}),
        .evidence = std::move(evidence),
        .affected_paths = std::move(affected),
        .recommended_action =
            "Faca backup dos arquivos importantes agora, em outro disco ou na nuvem. Depois "
            "verifique o disco com a ferramenta do fabricante e considere avaliacao tecnica.",
        .alternative_action = "Reduza o uso do disco ate conseguir fazer o backup. Continuar "
                              "gravando aumenta o risco de perder o que ainda esta la.",
        .expected_result = "Seus arquivos ficam protegidos independentemente do que acontecer com "
                           "o disco.",
        .limitations =
            "Os indicadores mostram que algo foi detectado, nao quanto tempo o disco ainda tem. "
            "Um disco nesse estado pode durar meses ou falhar amanha. O Cleaner nao consegue "
            "estimar isso, e nenhuma acao dele corrige um defeito fisico.",
    }};
}

}
