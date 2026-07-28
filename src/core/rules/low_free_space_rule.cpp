#include "core/rules/low_free_space_rule.hpp"

#include "core/rules/format.hpp"

namespace zelo::core {

std::string LowFreeSpaceRule::id() const {
    return "storage.low-free-space";
}

int LowFreeSpaceRule::version() const {
    return 1;
}

std::vector<Recommendation> LowFreeSpaceRule::evaluate(const SystemSnapshot& snapshot) const {
    if (!snapshot.volumes_available) {
        return {};
    }

    std::vector<Recommendation> recommendations;
    for (const auto& volume : snapshot.volumes) {
        if (!volume.is_system || volume.total_bytes == 0) {
            continue;
        }

        const double free_ratio = volume.free_ratio();
        if (free_ratio >= kLowFreeRatio) {
            continue;
        }

        const std::string free_text = format_bytes(volume.free_bytes);
        const std::string total_text = format_bytes(volume.total_bytes);
        const std::string percentage = format_percentage(free_ratio);

        recommendations.push_back(Recommendation{
            .id = id() + ":" + volume.letter,
            .rule_id = id(),
            .rule_version = version(),
            .title = "Pouco espaco livre no disco " + volume.letter,
            .description = "O disco " + volume.letter + " esta com " + free_text + " livres de " +
                           total_text + ", ou seja " + percentage +
                           ". Com menos de 10% livres o Windows costuma ter dificuldade para "
                           "aplicar atualizacoes e para trabalhar com arquivos temporarios.",
            .category = ActionCategory::ReadOnlyAnalysis,
            .severity = free_ratio < kCriticalFreeRatio ? Severity::Serious : Severity::Attention,
            .risk = RiskLevel::Green,
            .confidence = Confidence::from_signals(
                {{"espaco livre medido diretamente no volume", 0.95}}),
            .evidence = {Evidence{.source = "volume " + volume.letter,
                                  .description = "espaco livre em relacao a capacidade total",
                                  .value = free_text + " livres de " + total_text + " (" +
                                           percentage + ")"}},
            .recommended_action =
                "Revisar o que ocupa espaco no disco e liberar ou mover conteudo para outro disco.",
            .alternative_action =
                "Manter como esta e acompanhar; o disco ainda funciona, mas com pouca folga.",
            .expected_result = "Mais espaco livre, reduzindo falhas de atualizacao e lentidao "
                               "causada por falta de espaco para arquivos temporarios.",
            .limitations = "A medida mostra quanto falta de espaco, mas nao diz sozinha que o "
                           "computador esta lento por causa disso. Outras causas precisam ser "
                           "verificadas separadamente.",
        });
    }

    return recommendations;
}

}
