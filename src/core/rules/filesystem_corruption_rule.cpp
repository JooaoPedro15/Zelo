#include "core/rules/filesystem_corruption_rule.hpp"

namespace zelo::core {

namespace {

std::string join(const std::vector<std::string>& items) {
    std::string joined;
    for (const auto& item : items) {
        if (!joined.empty()) {
            joined += ", ";
        }
        joined += item;
    }
    return joined;
}

}

std::string FilesystemCorruptionRule::id() const {
    return "disks.filesystem-corruption";
}

int FilesystemCorruptionRule::version() const {
    return 1;
}

std::vector<Recommendation> FilesystemCorruptionRule::evaluate(const SystemSnapshot& snapshot) const {
    if (!snapshot.disks.filesystem_events_available) {
        return {};
    }

    const auto& corruption = snapshot.disks.filesystem_corruption;
    if (corruption.event_count < kMinimumEvents) {
        return {};
    }

    const std::string volumes = corruption.affected_volumes.empty()
                                    ? std::string("um ou mais volumes")
                                    : join(corruption.affected_volumes);
    const std::string count = std::to_string(corruption.event_count);
    const bool severe = corruption.event_count >= kSevereEvents;

    std::vector<Evidence> evidence{
        Evidence{.source = "canal System, driver NTFS",
                 .description = "eventos de corrupcao na estrutura do sistema de arquivos",
                 .value = count + " ocorrencias em " + volumes}};

    if (!corruption.last_seen.empty()) {
        evidence.push_back(Evidence{.source = "Visualizador de Eventos",
                                    .description = "ocorrencia mais recente",
                                    .value = corruption.last_seen});
    }

    return {Recommendation{
        .id = id(),
        .rule_id = id(),
        .rule_version = version(),
        .title = "Corrupcao no sistema de arquivos de " + volumes,
        .description =
            "O Windows registrou " + count +
            " avisos de corrupcao na estrutura do sistema de arquivos. Isso costuma vir de "
            "desligamento brusco, remocao do disco sem ejetar ou falha de gravacao. O Windows "
            "repara parte disso sozinho, mas o registro repetido indica que vale verificar.",
        .category = ActionCategory::ReadOnlyAnalysis,
        .health_category = HealthCategory::Disks,
        .severity = severe ? Severity::Serious : Severity::Attention,
        .risk = RiskLevel::Green,
        .confidence = Confidence::from_signals(
            {{"corrupcao registrada pelo proprio driver do sistema de arquivos", 0.7},
             {"mais de uma ocorrencia no periodo", 0.2}}),
        .evidence = std::move(evidence),
        .affected_paths = corruption.affected_volumes,
        .recommended_action =
            "Antes de qualquer reparo, faca backup do que importa neste volume. Depois, a "
            "verificacao somente leitura do Windows (chkdsk " + volumes +
            " /scan) analisa o volume sem altera-lo e sem impedir voce de usar o computador.",
        .alternative_action =
            "Se os arquivos deste volume sao importantes e ainda nao tem copia, copie primeiro. "
            "Verificacao nao substitui backup.",
        .tool = "Verificacao de disco do Windows",
        .requires_admin = true,
        .expected_result = "A verificacao dira se a estrutura do volume esta consistente e o que "
                           "encontrou de errado.",
        .limitations =
            "Esta versao do Zelo nao executa a verificacao — ela reune a evidencia e explica a "
            "ferramenta. O reparo, que e outro comando, altera o disco e pode exigir reiniciar. "
            "E importante saber: corrupcao repetida as vezes e sintoma de disco falhando "
            "fisicamente, e nesse caso reparar o sistema de arquivos nao resolve a causa. Se os "
            "avisos continuarem depois da verificacao, procure avaliacao tecnica do disco.",
    }};
}

}
