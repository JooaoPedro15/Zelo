#pragma once

#include <core/models/known_location.hpp>
#include <core/models/system_snapshot.hpp>
#include <core/risk/protected_paths.hpp>

#include <stop_token>
#include <vector>

namespace cleaner::collectors {

/// O catalogo de locais conhecidos, com os caminhos ja resolvidos para esta
/// maquina. Entradas cujo caminho nao existe aqui saem com `present = false`.
///
/// O catalogo e fixo no binario e revisado a mao. Nao ha deteccao automatica de
/// "pasta que parece cache": adivinhar o que uma pasta guarda e exatamente o
/// tipo de palpite que faz um limpador apagar algo que fazia falta.
[[nodiscard]] std::vector<core::KnownLocation> known_locations_catalog();

/// Mede quanto cada local ocupa nesta maquina.
class ReclaimableCollector {
public:
    explicit ReclaimableCollector(core::ProtectedPaths protected_paths);

    bool collect_into(core::SystemSnapshot& snapshot, std::stop_token token = {}) const;

    [[nodiscard]] core::ReclaimableInfo collect(std::stop_token token = {}) const;

private:
    core::ProtectedPaths protected_paths_;
};

}
