#pragma once

#include "core/classify/content_class.hpp"

#include <string>
#include <vector>

namespace cleaner::core {

/// Uma regra de identificacao de conteudo.
///
/// O casamento e por segmentos de caminho, nunca por semelhanca de nome. Uma
/// pasta chamada "cache" pode guardar qualquer coisa, e apagar pelo nome e como
/// limpadores destroem dados.
struct ClassificationRule {
    /// Segmentos que precisam aparecer no caminho, nesta ordem, mas nao
    /// necessariamente colados. Comparados sem diferenciar maiusculas.
    ///
    /// {"Google", "Chrome", "User Data", "Cache"} casa
    /// "...\Google\Chrome\User Data\Default\Cache".
    std::vector<std::string> segments;

    /// Quando verdadeiro, a regra so vale se o ultimo segmento for o fim do
    /// caminho. Serve para distinguir a pasta em si de tudo que ha abaixo dela.
    bool exact_leaf = false;

    Classification result;
};

/// Decide o que ha em um caminho.
///
/// A ordem importa: a primeira regra que casar vence, e as regras mais
/// especificas vem antes. E assim que "Cache" dentro do perfil do Chrome pode
/// ser seguro enquanto o perfil inteiro continua protegido.
class ContentClassifier {
public:
    ContentClassifier();
    explicit ContentClassifier(std::vector<ClassificationRule> rules);

    /// Classifica um caminho. Sem regra que case, devolve `unclassified()` — o
    /// conteudo continua aparecendo, so que declarado como nao identificado.
    [[nodiscard]] Classification classify(const std::string& path) const;

    [[nodiscard]] const std::vector<ClassificationRule>& rules() const;

private:
    std::vector<ClassificationRule> rules_;
};

/// As regras revisadas a mao que acompanham o programa.
[[nodiscard]] std::vector<ClassificationRule> default_classification_rules();

}
