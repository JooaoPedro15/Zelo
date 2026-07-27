#pragma once

#include <cstdint>
#include <string>

namespace zelo::core {

/// Formata bytes na unidade mais legivel (KB, MB, GB, TB). Texto voltado ao
/// usuario final, entao usa virgula decimal como no portugues.
[[nodiscard]] std::string format_bytes(std::uint64_t bytes);

/// Formata uma proporcao de 0 a 1 como porcentagem inteira.
[[nodiscard]] std::string format_percentage(double ratio);

}
