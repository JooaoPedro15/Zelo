#pragma once

#include "monitor/snapshot.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <stop_token>
#include <string>
#include <vector>

namespace zelo::monitor {

struct SnapshotOptions {
    /// Pastas ate esta profundidade entram sempre no retrato, mesmo pequenas.
    /// Sao os niveis que a interface mostra primeiro.
    int always_keep_depth = 4;

    /// Mais fundo que isso, a pasta so entra se for grande o bastante para
    /// explicar uma mudanca perceptivel no disco.
    std::uint64_t deep_folder_threshold = 50ULL * 1024 * 1024;

    /// Arquivos deste tamanho para cima sao guardados individualmente: um unico
    /// deles pode explicar sozinho o crescimento de uma pasta.
    std::uint64_t tracked_file_threshold = 100ULL * 1024 * 1024;

    /// Caminhos que nao entram no retrato. O diretorio de dados do proprio
    /// Zelo vai aqui: o banco de retratos crescendo apareceria como consumo
    /// misterioso, e seria o monitor acusando a si mesmo.
    std::vector<std::string> excluded_paths;
};

/// Percorre um volume e monta o retrato.
///
/// O total de cada pasta inclui tudo que ha abaixo dela — e a resposta para
/// "quanto AppData\Local\Google esta ocupando". A varredura anda em largura e
/// nao sabe, ao terminar uma pasta, se ainda ha filhas por visitar, entao a
/// soma da subarvore e feita depois, de baixo para cima.
class SnapshotTaker {
public:
    explicit SnapshotTaker(SnapshotOptions options = {});

    /// Progresso em pastas visitadas, para a interface nao ficar muda durante
    /// uma varredura longa.
    using ProgressCallback = std::function<void(std::size_t folders_seen)>;

    [[nodiscard]] Snapshot take(const std::filesystem::path& root, const std::string& volume,
                                std::stop_token token = {},
                                const ProgressCallback& progress = {}) const;

private:
    SnapshotOptions options_;
};

/// Soma os totais de cada subarvore a partir dos tamanhos diretos por pasta.
///
/// Separada da varredura para poder ser testada com dados montados a mao, sem
/// precisar de um disco com a forma exata que o teste quer.
[[nodiscard]] std::vector<FolderSize> accumulate_subtrees(std::vector<FolderSize> direct_sizes);

}
