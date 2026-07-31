#pragma once

#include "storage/quarantine_store.hpp"

#include <core/models/cleanup_plan.hpp>
#include <core/risk/protected_paths.hpp>

#include <string>
#include <vector>

namespace zelo::storage {

/// Executa a limpeza em duas etapas separadas de proposito.
///
/// `plan` monta a lista concreta do que sairia, sem tocar em nada, para o
/// usuario ver e aprovar arquivos de verdade em vez de uma promessa. `execute`
/// so age sobre um plano ja aprovado.
///
/// Nada e apagado: os arquivos vao para a quarentena, e a remocao definitiva
/// acontece depois, por prazo. Ate la, da para desfazer.
class CleanupService {
public:
    CleanupService(QuarantineStore quarantine, core::ProtectedPaths protected_paths);

    /// Monta o plano a partir dos caminhos sugeridos por uma recomendacao.
    /// Caminho protegido, inexistente ou que nao seja arquivo comum e recusado
    /// e aparece em `rejected`.
    [[nodiscard]] core::CleanupPlan plan(const std::vector<std::string>& paths,
                                         const std::string& recommendation_id) const;

    /// Move para a quarentena o que o plano listou.
    ///
    /// Revalida a protecao de cada caminho agora, sem confiar no plano: entre
    /// montar e executar o disco muda, e o que vale e o estado no momento de
    /// mexer nele.
    [[nodiscard]] core::CleanupOutcome execute(const core::CleanupPlan& plan) const;

private:
    QuarantineStore quarantine_;
    core::ProtectedPaths protected_paths_;
};

}
