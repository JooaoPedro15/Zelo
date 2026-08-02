#pragma once

#include <core/cleaners/cleaner_spec.hpp>
#include <core/risk/protected_paths.hpp>

#include <stop_token>
#include <vector>

namespace cleaner::cleaners {

/// Os limpadores que acompanham o programa, com os caminhos ja resolvidos para
/// esta maquina. Entradas cujas pastas nao existem aqui saem da lista.
[[nodiscard]] std::vector<core::CleanerSpec> available_cleaners();

/// Executa limpezas declaradas.
///
/// Nao aceita caminho de fora. O que cada limpador pode tocar vem da declaracao
/// dele, e a deny-list e consultada por cima disso — duas vezes, ao planejar e
/// imediatamente antes de cada exclusao.
class CleanerEngine {
public:
    explicit CleanerEngine(core::ProtectedPaths protected_paths);

    /// Mede o que sairia, sem alterar nada.
    [[nodiscard]] core::CleanerPreview preview(const core::CleanerSpec& spec,
                                               std::stop_token token = {}) const;

    /// Apaga o que a previa apontou, conferindo tudo de novo.
    ///
    /// A previa nao e usada como lista de exclusao: o disco pode ter mudado
    /// desde entao, e um arquivo trocado por um link entre a medicao e a acao e
    /// exatamente como um limpador acaba apagando fora da area permitida.
    [[nodiscard]] core::CleanerOutcome execute(const core::CleanerSpec& spec,
                                               std::stop_token token = {}) const;

private:
    core::ProtectedPaths protected_paths_;
};

}
