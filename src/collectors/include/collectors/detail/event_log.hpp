#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cleaner::collectors::detail {

/// Percorre um canal de eventos do Windows com a consulta XPath dada e devolve
/// o XML de cada evento.
///
/// Devolve vazio (`nullopt`) quando o canal nao pode ser consultado. Lista
/// vazia e resultado legitimo; nao conseguir olhar e outra coisa, e confundir
/// os dois faria a analise afirmar que esta tudo bem sem ter visto nada.
[[nodiscard]] std::optional<std::vector<std::string>> query_event_channel(const wchar_t* channel,
                                                                         const std::wstring& query,
                                                                         std::size_t limit);

/// O instante do evento, em ISO 8601, ou vazio quando ausente.
[[nodiscard]] std::string event_time(std::string_view xml);

/// O valor de um campo nomeado do evento, ou vazio quando ausente.
[[nodiscard]] std::string event_field(std::string_view xml, std::string_view name);

}
