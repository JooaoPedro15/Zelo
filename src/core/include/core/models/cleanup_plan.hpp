#pragma once

#include "core/models/recommendation.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace cleaner::core {

/// Um arquivo que a limpeza pretende remover.
struct CleanupItem {
    std::string path;
    std::uint64_t size_bytes = 0;

    /// De qual recomendacao este arquivo veio. O usuario precisa poder ligar
    /// cada arquivo ao motivo pelo qual ele apareceu na lista.
    std::string recommendation_id;
};

/// A lista exata do que sera removido, montada antes de qualquer alteracao.
///
/// Existe para que o usuario veja e aprove o conteudo concreto, nao uma
/// promessa de "limpar arquivos temporarios". Nenhuma limpeza acontece sem um
/// plano aprovado.
struct CleanupPlan {
    std::vector<CleanupItem> items;

    /// Caminhos que a regra sugeriu mas o plano descartou, com o motivo. Um
    /// item recusado precisa aparecer: sumir em silencio esconderia do usuario
    /// que a limpeza cobriu menos do que ele esperava.
    std::vector<std::string> rejected;

    [[nodiscard]] std::uint64_t total_bytes() const {
        std::uint64_t total = 0;
        for (const auto& item : items) {
            total += item.size_bytes;
        }
        return total;
    }

    [[nodiscard]] bool empty() const { return items.empty(); }
};

/// O que aconteceu de fato depois de executar um plano.
struct CleanupOutcome {
    std::size_t removed_count = 0;
    std::uint64_t freed_bytes = 0;

    /// Arquivos que nao puderam ser removidos, com o motivo. Costuma ser
    /// arquivo em uso, e nao e falha do plano: e informacao para o usuario.
    std::vector<std::string> skipped;

    /// Identificadores na quarentena, para desfazer.
    std::vector<std::string> quarantine_ids;
};

}
