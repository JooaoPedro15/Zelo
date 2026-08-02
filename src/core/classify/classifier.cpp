#include "core/classify/classifier.hpp"

#include <algorithm>
#include <cctype>

namespace cleaner::core {

namespace {

std::vector<std::string> split_segments(const std::string& path) {
    std::vector<std::string> segments;
    std::string current;

    for (const char letter : path) {
        if (letter == '\\' || letter == '/') {
            if (!current.empty()) {
                segments.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(letter);
    }

    if (!current.empty()) {
        segments.push_back(current);
    }
    return segments;
}

bool same_segment(const std::string& left, const std::string& right) {
    if (left.size() != right.size()) {
        return false;
    }

    // Dobra ASCII basta aqui: os segmentos das regras sao nomes de programa e de
    // pasta padrao, todos sem acento. Um nome de usuario acentuado nunca e
    // comparado — ele entra no caminho, nao na regra.
    for (std::size_t index = 0; index < left.size(); ++index) {
        const auto a = static_cast<unsigned char>(left[index]);
        const auto b = static_cast<unsigned char>(right[index]);
        if (std::tolower(a) != std::tolower(b)) {
            return false;
        }
    }
    return true;
}

/// A regra casa se seus segmentos aparecerem no caminho, na ordem.
///
/// Devolve a posicao logo apos o ultimo segmento casado, ou `npos`. A posicao
/// serve para `exact_leaf`: se sobrou caminho depois, nao era a pasta em si.
std::size_t match_position(const std::vector<std::string>& path,
                           const std::vector<std::string>& wanted) {
    std::size_t index = 0;

    for (const auto& segment : wanted) {
        bool found = false;
        while (index < path.size()) {
            const bool hit = same_segment(path[index], segment);
            ++index;
            if (hit) {
                found = true;
                break;
            }
        }
        if (!found) {
            return std::string::npos;
        }
    }
    return index;
}

}

ContentClassifier::ContentClassifier() : rules_(default_classification_rules()) {}

ContentClassifier::ContentClassifier(std::vector<ClassificationRule> rules)
    : rules_(std::move(rules)) {}

const std::vector<ClassificationRule>& ContentClassifier::rules() const {
    return rules_;
}

Classification ContentClassifier::classify(const std::string& path) const {
    const auto segments = split_segments(path);

    for (const auto& rule : rules_) {
        if (rule.segments.empty()) {
            continue;
        }

        const auto position = match_position(segments, rule.segments);
        if (position == std::string::npos) {
            continue;
        }

        if (rule.exact_leaf && position != segments.size()) {
            continue;
        }

        return rule.result;
    }

    return unclassified();
}

}
