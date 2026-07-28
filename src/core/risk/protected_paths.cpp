#include "core/risk/protected_paths.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <stdexcept>

namespace zelo::core {

namespace {

/// Deixa o caminho comparavel: separador unico e caixa dobrada.
///
/// As APIs do Windows devolvem caixa variada — `GetWindowsDirectoryW` responde
/// "C:\WINDOWS". Parar no ASCII deixaria um perfil como "C:\Users\João" sem
/// casar com "C:\USERS\JOÃO", e o caminho ficaria desprotegido.
///
/// Cobre ASCII e o bloco Latin-1 (À-Þ), suficiente para portugues, espanhol,
/// frances e alemao. Nao cobre Latin Extended (polones, tcheco, turco), grego
/// nem cirilico: um caminho nessas escritas so casa se a caixa for igual.
std::string normalize(std::string_view text) {
    constexpr unsigned char kLatin1Lead = 0xC3;
    constexpr unsigned char kMultiplicationSign = 0x97;
    constexpr unsigned char kCaseBit = 0x20;

    std::string result(text);
    for (std::size_t index = 0; index < result.size(); ++index) {
        const auto byte = static_cast<unsigned char>(result[index]);

        if (byte == '/') {
            result[index] = '\\';
            continue;
        }

        if (byte < 0x80) {
            result[index] = static_cast<char>(std::tolower(byte));
            continue;
        }

        // Em UTF-8 as maiusculas acentuadas do Latin-1 vao de C3 80 a C3 9E, e
        // as minusculas correspondentes de C3 A0 a C3 BE: a diferenca e um bit.
        // O sinal de multiplicacao (C3 97) mora no meio da faixa e nao tem par.
        if (byte == kLatin1Lead && index + 1 < result.size()) {
            const auto next = static_cast<unsigned char>(result[index + 1]);
            if (next >= 0x80 && next <= 0x9E && next != kMultiplicationSign) {
                result[index + 1] = static_cast<char>(next | kCaseBit);
            }
            ++index;
        }
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
    : ProtectedPaths(ProtectedPathsSpec{.subtree_roots = std::move(roots),
                                        .exceptions = std::move(exceptions)}) {}

ProtectedPaths::ProtectedPaths(ProtectedPathsSpec spec)
    : subtree_roots_(canonicalize_all(spec.subtree_roots)),
      exact_paths_(canonicalize_all(spec.exact_paths)),
      exceptions_(canonicalize_all(spec.exceptions)) {
    for (const auto& exception : exceptions_) {
        const bool carves_out_a_root = std::any_of(
            subtree_roots_.begin(), subtree_roots_.end(),
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

    // Caminho exato vem antes do carve-out: proteger a propria pasta e uma
    // afirmacao sobre ela, nao sobre o que ha dentro.
    for (const auto& exact : exact_paths_) {
        if (candidate == exact) {
            return true;
        }
    }

    for (const auto& exception : exceptions_) {
        if (is_at_or_under(candidate, exception)) {
            return false;
        }
    }

    for (const auto& root : subtree_roots_) {
        if (is_at_or_under(candidate, root)) {
            return true;
        }
    }
    return false;
}

}
