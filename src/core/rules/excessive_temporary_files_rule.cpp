#include "core/rules/excessive_temporary_files_rule.hpp"

#include "core/rules/format.hpp"
#include "core/rules/low_free_space_rule.hpp"

namespace zelo::core {

namespace {

/// O disco de sistema esta sem folga? Reaproveita o mesmo limite da regra de
/// espaco livre, para as duas nao discordarem sobre o que e "apertado".
bool system_volume_is_tight(const SystemSnapshot& snapshot) {
    if (!snapshot.volumes_available) {
        return false;
    }

    for (const auto& volume : snapshot.volumes) {
        if (volume.is_system && volume.total_bytes > 0) {
            return volume.free_ratio() < LowFreeSpaceRule::kLowFreeRatio;
        }
    }
    return false;
}

}

std::string ExcessiveTemporaryFilesRule::id() const {
    return "storage.excessive-temporary-files";
}

int ExcessiveTemporaryFilesRule::version() const {
    return 1;
}

std::vector<Recommendation> ExcessiveTemporaryFilesRule::evaluate(const SystemSnapshot& snapshot) const {
    const auto& temporary_files = snapshot.temporary_files;
    if (!temporary_files.available) {
        return {};
    }

    const bool disk_under_pressure = system_volume_is_tight(snapshot);
    const std::uint64_t minimum =
        disk_under_pressure ? kMinimumBytesUnderPressure : kMinimumBytes;

    if (temporary_files.total_bytes < minimum) {
        return {};
    }

    const std::string size_text = format_bytes(temporary_files.total_bytes);
    const std::string count_text = std::to_string(temporary_files.file_count);

    return {Recommendation{
        .id = id(),
        .rule_id = id(),
        .rule_version = version(),
        .title = "Arquivos temporarios ocupando " + size_text,
        .description = "Foram encontrados " + count_text + " arquivos temporarios somando " +
                       size_text +
                       ". Sao arquivos que o sistema e os programas recriam quando precisam.",
        .category = ActionCategory::KnownTemporaryFile,
        .health_category = HealthCategory::Storage,
        .severity = temporary_files.total_bytes >= kLargeBytes ? Severity::Attention : Severity::Info,
        .risk = RiskLevel::Green,
        .confidence = Confidence::from_signals(
            {{"arquivos encontrados em pastas de temporarios conhecidas", 0.9}}),
        .evidence = {Evidence{.source = "varredura de pastas de temporarios",
                              .description = "total ocupado por arquivos temporarios",
                              .value = size_text + " em " + count_text + " arquivos"}},
        .reclaimable_bytes = temporary_files.total_bytes,
        .recommended_action = "Revisar a lista e apagar os arquivos temporarios.",
        .alternative_action = "Manter como esta; os arquivos nao atrapalham o funcionamento, "
                              "apenas ocupam espaco.",
        .expected_result = "Cerca de " + size_text + " liberados no disco.",
        .limitations =
            "Um programa aberto pode estar usando parte destes arquivos, e nesse caso eles nao "
            "sao apagados agora. O espaco liberado pode ficar abaixo do estimado.",
    }};
}

}
