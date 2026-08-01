#pragma once

#include <string>
#include <string_view>

namespace cleaner::collectors::detail {

/// As APIs do Windows falam UTF-16; o resto do projeto fala UTF-8. Toda
/// travessia dessa fronteira passa por aqui.
[[nodiscard]] std::string to_utf8(std::wstring_view text);

/// Converte de UTF-8 para o formato que as APIs do Windows esperam.
[[nodiscard]] std::wstring to_wide(std::string_view text);

/// Compara ignorando caixa, para casar nome de programa e caminho contra as
/// listas de reconhecimento.
[[nodiscard]] bool contains_ignoring_case(std::string_view haystack, std::string_view needle);

/// Posicao da primeira ocorrencia, ignorando caixa, ou `npos`.
[[nodiscard]] std::size_t find_ignoring_case(std::string_view haystack, std::string_view needle);

}
