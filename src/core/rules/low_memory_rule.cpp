#include "core/rules/low_memory_rule.hpp"

#include "core/rules/format.hpp"

namespace cleaner::core {

std::string LowMemoryRule::id() const {
    return "performance.low-available-memory";
}

int LowMemoryRule::version() const {
    return 1;
}

std::vector<Recommendation> LowMemoryRule::evaluate(const SystemSnapshot& snapshot) const {
    const auto& memory = snapshot.memory;
    if (!memory.available || memory.total_bytes == 0) {
        return {};
    }

    const double available_ratio = memory.available_ratio();
    if (available_ratio >= kLowAvailableRatio) {
        return {};
    }

    const std::string available_text = format_bytes(memory.available_bytes);
    const std::string total_text = format_bytes(memory.total_bytes);

    return {Recommendation{
        .id = id(),
        .rule_id = id(),
        .rule_version = version(),
        .title = "Pouca memoria disponivel no momento",
        .description = "Restavam " + available_text + " livres de " + total_text +
                       " quando a analise rodou. Com pouca memoria sobrando o Windows passa a "
                       "usar o disco como apoio, e o computador responde mais devagar.",
        .category = ActionCategory::ReadOnlyAnalysis,
        .health_category = HealthCategory::Performance,
        .severity =
            available_ratio < kCriticalAvailableRatio ? Severity::Serious : Severity::Attention,
        .risk = RiskLevel::Green,
        // Confianca deliberadamente baixa: e uma unica medida, num unico
        // instante. Basta para levantar a questao, nao para afirmar um padrao.
        .confidence = Confidence::from_signals(
            {{"memoria disponivel medida no momento da analise", 0.5}}),
        .evidence = {Evidence{.source = "memoria do sistema",
                              .description = "memoria disponivel em relacao ao total",
                              .value = available_text + " livres de " + total_text + " (" +
                                       format_percentage(available_ratio) + ")"}},
        .recommended_action =
            "Feche programas que nao estiver usando e observe se a lentidao melhora.",
        .alternative_action =
            "Se isso acontecer com frequencia mesmo com poucos programas abertos, o computador "
            "pode estar pedindo mais memoria do que tem instalada.",
        .expected_result = "Mais memoria disponivel e menos apoio no disco, o que costuma deixar o "
                           "sistema mais responsivo.",
        .limitations =
            "Esta e uma unica medida, tirada no instante da analise. Um programa pesado aberto "
            "agora explica o numero sem que exista problema nenhum. So a repeticao em varias "
            "analises indicaria falta real de memoria, e o Cleaner ainda nao acompanha isso ao longo "
            "do tempo.",
    }};
}

}
