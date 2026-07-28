#include "core/rules/excessive_temporary_files_rule.hpp"

#include "core/rules/format.hpp"

namespace zelo::core {

std::string ExcessiveTemporaryFilesRule::id() const {
    return "storage.excessive-temporary-files";
}

int ExcessiveTemporaryFilesRule::version() const {
    return 1;
}

std::vector<Recommendation> ExcessiveTemporaryFilesRule::evaluate(const SystemSnapshot& snapshot) const {
    const auto& temporary_files = snapshot.temporary_files;
    if (!temporary_files.available || temporary_files.total_bytes < kMinimumBytes) {
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
