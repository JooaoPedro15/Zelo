#include "storage/cleanup_service.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <utility>

namespace zelo::storage {

CleanupService::CleanupService(QuarantineStore quarantine, core::ProtectedPaths protected_paths)
    : quarantine_(std::move(quarantine)), protected_paths_(std::move(protected_paths)) {}

core::CleanupPlan CleanupService::plan(const std::vector<std::string>& paths,
                                       const std::string& recommendation_id) const {
    core::CleanupPlan plan;

    for (const auto& path : paths) {
        if (protected_paths_.is_protected(path)) {
            plan.rejected.push_back(path + " — caminho protegido");
            continue;
        }

        std::error_code error;
        const std::filesystem::path file = path;

        if (!std::filesystem::is_regular_file(file, error)) {
            // Diretorio, link ou caminho que nao existe mais. A limpeza so lida
            // com arquivo comum: apagar um link removeria o alvo por tabela.
            plan.rejected.push_back(path + " — nao e um arquivo comum");
            continue;
        }

        const auto size = std::filesystem::file_size(file, error);
        if (error) {
            plan.rejected.push_back(path + " — tamanho nao pode ser lido");
            continue;
        }

        plan.items.push_back(core::CleanupItem{
            .path = path, .size_bytes = size, .recommendation_id = recommendation_id});
    }

    return plan;
}

core::CleanupOutcome CleanupService::execute(const core::CleanupPlan& plan) const {
    core::CleanupOutcome outcome;

    for (const auto& item : plan.items) {
        // A protecao e verificada de novo, agora. O plano pode ter sido montado
        // ha minutos, e confiar nele deixaria uma janela entre decidir e agir.
        if (protected_paths_.is_protected(item.path)) {
            outcome.skipped.push_back(item.path + " — caminho protegido");
            spdlog::warn("limpeza recusou caminho protegido");
            continue;
        }

        const auto entry = quarantine_.take(item.path, item.recommendation_id);
        if (!entry) {
            // Costuma ser arquivo em uso. Nao e falha do plano, e informacao
            // para o usuario saber que sobrou menos espaco do que o estimado.
            outcome.skipped.push_back(item.path + " — nao pode ser removido agora");
            continue;
        }

        ++outcome.removed_count;
        outcome.freed_bytes += entry->size_bytes;
        outcome.quarantine_ids.push_back(entry->id);
    }

    spdlog::info("limpeza concluida: {} arquivos, {} bytes liberados, {} ignorados",
                 outcome.removed_count, outcome.freed_bytes, outcome.skipped.size());
    return outcome;
}

}
