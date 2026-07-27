#include "core/risk/protected_paths.hpp"

#include <cctype>
#include <utility>

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

}

ProtectedPaths::ProtectedPaths(std::vector<std::string> roots) {
    roots_.reserve(roots.size());
    for (const auto& root : roots) {
        roots_.push_back(strip_trailing_separators(normalize(root)));
    }
}

bool ProtectedPaths::is_protected(std::string_view path) const {
    const std::string candidate = strip_trailing_separators(normalize(path));
    if (candidate.empty() || has_relative_component(candidate)) {
        return true;
    }
    for (const auto& root : roots_) {
        if (!candidate.starts_with(root)) {
            continue;
        }
        if (candidate.size() == root.size() || candidate[root.size()] == '\\') {
            return true;
        }
    }
    return false;
}

}
