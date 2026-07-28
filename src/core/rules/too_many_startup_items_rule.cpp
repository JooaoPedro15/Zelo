#include "core/rules/too_many_startup_items_rule.hpp"

namespace zelo::core {

std::string TooManyStartupItemsRule::id() const {
    return "startup.too-many-items";
}

int TooManyStartupItemsRule::version() const {
    return 1;
}

std::vector<Recommendation> TooManyStartupItemsRule::evaluate(const SystemSnapshot& snapshot) const {
    if (!snapshot.startup_available) {
        return {};
    }

    std::vector<std::string> candidate_paths;
    std::vector<std::string> candidate_names;
    for (const auto& item : snapshot.startup_items) {
        if (item.essential) {
            continue;
        }
        candidate_paths.push_back(item.path);
        candidate_names.push_back(item.name);
    }

    if (candidate_paths.size() <= kAcceptableCount) {
        return {};
    }

    const std::string count_text = std::to_string(candidate_paths.size());

    std::string names_text;
    for (std::size_t index = 0; index < candidate_names.size(); ++index) {
        if (index > 0) {
            names_text += index + 1 == candidate_names.size() ? " e " : ", ";
        }
        names_text += candidate_names.at(index);
    }

    return {Recommendation{
        .id = id(),
        .rule_id = id(),
        .rule_version = version(),
        .title = count_text + " programas nao essenciais iniciam com o Windows",
        .description =
            "Estes programas sobem junto com o Windows e disputam disco e memoria durante a "
            "inicializacao: " + names_text +
            ". Desativar um item da inicializacao nao desinstala nem impede de abrir depois.",
        .category = ActionCategory::DisableStartupItem,
        .severity = candidate_paths.size() >= kSevereCount ? Severity::Serious : Severity::Attention,
        .risk = RiskLevel::Yellow,
        .confidence = Confidence::from_signals(
            {{"itens lidos das origens de inicializacao do Windows", 0.9},
             {"itens reconhecidos como essenciais foram descartados da contagem", 0.05}}),
        .evidence = {Evidence{.source = "chaves Run e pastas de inicializacao",
                              .description = "programas nao essenciais configurados para iniciar "
                                             "junto com o Windows",
                              .value = count_text + " itens"}},
        .affected_paths = candidate_paths,
        .recommended_action = "Revisar a lista e desativar os programas que voce nao precisa ter "
                              "abertos assim que liga o computador.",
        .alternative_action = "Manter como esta e abrir os programas manualmente quando quiser.",
        .undoable = true,
        .expected_result = "Inicializacao mais rapida e menos disputa por disco e memoria logo "
                           "apos ligar o computador.",
        .limitations = "O ganho depende de quanto cada programa realmente consome ao iniciar, e "
                       "isso varia entre computadores. Alguns programas voltam sozinhos para a "
                       "inicializacao quando sao atualizados.",
    }};
}

}
