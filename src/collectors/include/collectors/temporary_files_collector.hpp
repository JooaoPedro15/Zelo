#pragma once

#include <core/models/system_snapshot.hpp>
#include <core/risk/protected_paths.hpp>

#include <filesystem>
#include <stop_token>
#include <vector>

namespace zelo::collectors {

/// Mede quanto espaco os arquivos temporarios conhecidos estao ocupando.
///
/// Somente leitura: percorre e soma, nunca apaga. As pastas medidas sao as que
/// o Windows e os programas usam para arquivos descartaveis, e cada uma passa
/// pela deny-list antes de ser percorrida — se um caminho estiver protegido, e
/// porque nao deve virar alvo de limpeza, e entao nem entra na conta.
class TemporaryFilesCollector {
public:
    explicit TemporaryFilesCollector(core::ProtectedPaths protected_paths);

    /// As pastas de temporarios que existem nesta maquina e nao estao
    /// protegidas.
    [[nodiscard]] std::vector<std::filesystem::path> folders() const;

    bool collect_into(core::SystemSnapshot& snapshot, std::stop_token token = {}) const;

    /// Os arquivos temporarios encontrados agora, para montar um plano de
    /// limpeza.
    ///
    /// Fica separado da coleta de propósito: a analise so precisa do total, e
    /// guardar milhares de caminhos no historico nao faz sentido. A lista e
    /// levantada na hora de limpar, entao reflete o disco no momento de agir e
    /// nao o de minutos atras.
    [[nodiscard]] std::vector<std::filesystem::path> list_files(std::stop_token token = {}) const;

private:
    core::ProtectedPaths protected_paths_;
};

}
