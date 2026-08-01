#pragma once

#include <core/models/system_snapshot.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cleaner::collectors {

/// Os campos que interessam de um evento de falha de aplicativo.
struct ParsedFailureEvent {
    std::string application;
    std::string faulting_module;
    std::string when;
};

/// Extrai os campos do XML de um evento do Windows.
///
/// Fica separada da leitura do log para poder ser testada com XML fixo: montar
/// falhas de aplicativo de verdade na maquina para testar nao e viavel.
[[nodiscard]] std::optional<ParsedFailureEvent> parse_failure_event(std::string_view xml);

/// Junta os eventos por aplicativo. O usuario precisa saber que um programa
/// falhou doze vezes, nao ver doze linhas iguais.
[[nodiscard]] std::vector<core::AppFailureInfo> group_failures(
    const std::vector<ParsedFailureEvent>& events);

}
