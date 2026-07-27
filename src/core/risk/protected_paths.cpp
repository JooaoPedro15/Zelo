#include "core/risk/protected_paths.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <stdexcept>

namespace zelo::core {

namespace {

// Dobra de caixa apenas ASCII: basta para as raizes protegidas do Windows,
// que sao todas ASCII (Windows, System32, Program Files, ProgramData).
std::string normalize(std::string_view text) {
    std::string result(text);
    for (char& character : result) {
        if (character == '/') {
            character = '\\';
            continue;
        }
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return result;
}

// Guardar as raizes sem separador no fim deixa uma unica regra de comparacao
// valer tanto para "C:\Windows" quanto para "C:\Windows\" ou "C:\".
std::string strip_trailing_separators(std::string value) {
    while (!value.empty() && value.back() == '\\') {
        value.pop_back();
    }
    return value;
}

std::string canonical_form(std::string_view path) {
    return strip_trailing_separators(normalize(path));
}

bool has_relative_component(std::string_view path) {
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t end = path.find('\\', start);
        const std::string_view component =
            path.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
        if (component == "." || component == "..") {
            return true;
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return false;
}

// Casa a raiz inteira ou um filho dela. Comparar so o prefixo textual faria
// "C:\Program Files Backup" casar com a raiz "C:\Program Files".
bool is_at_or_under(const std::string& candidate, const std::string& root) {
    if (!candidate.starts_with(root)) {
        return false;
    }
    return candidate.size() == root.size() || candidate[root.size()] == '\\';
}

bool is_strictly_under(const std::string& candidate, const std::string& root) {
    return candidate.size() > root.size() && is_at_or_under(candidate, root);
}

std::vector<std::string> canonicalize_all(const std::vector<std::string>& values) {
    std::vector<std::string> result;
    result.reserve(values.size());
    for (const auto& value : values) {
        result.push_back(canonical_form(value));
    }
    return result;
}

}

ProtectedPaths::ProtectedPaths(std::vector<std::string> roots, std::vector<std::string> exceptions)
    : roots_(canonicalize_all(roots)), exceptions_(canonicalize_all(exceptions)) {
    for (const auto& exception : exceptions_) {
        const bool carves_out_a_root = std::any_of(
            roots_.begin(), roots_.end(),
            [&exception](const std::string& root) { return is_strictly_under(exception, root); });

        // Uma excecao igual a uma raiz (ou acima dela) desligaria a protecao
        // inteira. Falhar aqui e barulhento de proposito: a lista e fixa no
        // binario, entao o erro aparece no teste, nunca no computador do usuario.
        if (!carves_out_a_root) {
            throw std::invalid_argument(
                "excecao precisa estar estritamente dentro de uma raiz protegida: " + exception);
        }
    }
}

bool ProtectedPaths::is_protected(std::string_view path) const {
    const std::string candidate = canonical_form(path);
    if (candidate.empty() || has_relative_component(candidate)) {
        return true;
    }

    for (const auto& exception : exceptions_) {
        if (is_at_or_under(candidate, exception)) {
            return false;
        }
    }

    for (const auto& root : roots_) {
        if (is_at_or_under(candidate, root)) {
            return true;
        }
    }
    return false;
}

}
