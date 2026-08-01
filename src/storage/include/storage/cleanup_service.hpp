#pragma once

#include "storage/quarantine_store.hpp"

#include <core/models/cleanup_plan.hpp>
#include <core/risk/protected_paths.hpp>

#include <string>
#include <vector>

namespace cleaner::storage {

/// Executa a limpeza em duas etapas separadas de proposito.
///
/// `plan` monta a lista concreta do que sairia, sem tocar em nada, para o
/// usuario ver e aprovar arquivos de verdade em vez de uma promessa. `execute`
/// so age sobre um plano ja aprovado.
///
/// Nada e apagado: os arquivos vao para a quarentena, e a remocao definitiva
/// acontece depois, por prazo. Ate la, da para desfazer.
/// O que fazer com os arquivos que saem.
enum class RemovalMode {
    /// Apaga de vez, liberando o espaco na hora.
    ///
    /// E o modo certo para conteudo que o proprio programa dono recria: guardar
    /// copia de um cache que volta sozinho ocupa o mesmo espaco que se queria
    /// liberar, e o botao prometeria algo que nao acontece.
    Delete,

    /// Move para a quarentena, permitindo devolver depois.
    ///
    /// O espaco so e liberado na purga: ate la os arquivos continuam no disco,
    /// em outra pasta. Vale para conteudo que nao volta sozinho.
    Quarantine,
};

class CleanupService {
public:
    CleanupService(QuarantineStore quarantine, core::ProtectedPaths protected_paths);

    /// Monta o plano a partir dos caminhos sugeridos por uma recomendacao.
    /// Caminho protegido, inexistente ou que nao seja arquivo comum e recusado
    /// e aparece em `rejected`.
    [[nodiscard]] core::CleanupPlan plan(const std::vector<std::string>& paths,
                                         const std::string& recommendation_id) const;

    /// Monta o plano com todo o conteudo de uma pasta, recursivamente.
    ///
    /// A pasta em si nao entra: so os arquivos dentro dela. Remover a pasta
    /// faria o programa dono falhar ao procura-la, enquanto esvazia-la e o que
    /// ele espera de um cache limpo.
    ///
    /// A varredura nao atravessa links, entao limpar um cache nao alcanca o
    /// destino de um atalho que por acaso esteja la dentro.
    [[nodiscard]] core::CleanupPlan plan_folder(const std::string& folder,
                                                const std::string& recommendation_id) const;

    /// Move para a quarentena o que o plano listou.
    ///
    /// Revalida a protecao de cada caminho agora, sem confiar no plano: entre
    /// montar e executar o disco muda, e o que vale e o estado no momento de
    /// mexer nele.
    [[nodiscard]] core::CleanupOutcome execute(const core::CleanupPlan& plan,
                                               RemovalMode mode) const;

private:
    QuarantineStore quarantine_;
    core::ProtectedPaths protected_paths_;
};

}
