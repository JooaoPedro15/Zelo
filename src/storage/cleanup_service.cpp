#include "storage/cleanup_service.hpp"

#include <scanner/storage_scanner.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <limits>
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

core::CleanupPlan CleanupService::plan_folder(const std::string& folder,
                                              const std::string& recommendation_id) const {
    core::CleanupPlan plan;

    if (protected_paths_.is_protected(folder)) {
        plan.rejected.push_back(folder + " — caminho protegido");
        return plan;
    }

    std::error_code error;
    if (!std::filesystem::is_directory(folder, error)) {
        plan.rejected.push_back(folder + " — pasta nao encontrada");
        return plan;
    }

    // O scanner ja resolve o que importa aqui: nao atravessa links, aguenta
    // caminho longo e nao cai por causa de uma subpasta sem permissao.
    const scanner::StorageScanner storage_scanner{
        scanner::ScanOptions{.largest_files_kept = std::numeric_limits<std::size_t>::max(),
                             .rollup_depth = 0}};

    const auto result = storage_scanner.scan(folder);
    if (!result.completed) {
        plan.rejected.push_back(folder + " — a varredura nao pode ser concluida");
        return plan;
    }

    std::vector<std::string> files;
    files.reserve(result.largest_files.size());
    for (const auto& file : result.largest_files) {
        files.push_back(file.path);
    }

    core::CleanupPlan expanded = this->plan(files, recommendation_id);
    expanded.rejected.insert(expanded.rejected.end(), plan.rejected.begin(), plan.rejected.end());
    return expanded;
}

core::CleanupOutcome CleanupService::execute(const core::CleanupPlan& plan,
                                             RemovalMode mode) const {
    core::CleanupOutcome outcome;

    for (const auto& item : plan.items) {
        // A protecao e verificada de novo, agora. O plano pode ter sido montado
        // ha minutos, e confiar nele deixaria uma janela entre decidir e agir.
        if (protected_paths_.is_protected(item.path)) {
            outcome.skipped.push_back(item.path + " — caminho protegido");
            spdlog::warn("limpeza recusou caminho protegido");
            continue;
        }

        if (mode == RemovalMode::Delete) {
            std::error_code error;
            const auto size = std::filesystem::file_size(item.path, error);
            if (error) {
                outcome.skipped.push_back(item.path + " — nao pode ser lido agora");
                continue;
            }

            if (!std::filesystem::remove(item.path, error) || error) {
                // Costuma ser arquivo em uso por um programa aberto. Nao e
                // falha do plano: e informacao para o usuario entender por que
                // sobrou menos espaco do que a estimativa dizia.
                outcome.skipped.push_back(item.path + " — em uso ou sem permissao");
                continue;
            }

            ++outcome.removed_count;
            outcome.freed_bytes += size;
            continue;
        }

        const auto entry = quarantine_.take(item.path, item.recommendation_id);
        if (!entry) {
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
