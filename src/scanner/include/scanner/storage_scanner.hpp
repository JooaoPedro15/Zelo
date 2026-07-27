#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stop_token>
#include <string>
#include <vector>

namespace zelo::scanner {

struct LargeFile {
    std::string path;
    std::uint64_t bytes = 0;
};

struct DirectoryRollup {
    std::string path;
    std::uint64_t total_bytes = 0;
    std::size_t file_count = 0;
};

struct ScanResult {
    std::uint64_t total_bytes = 0;
    std::size_t file_count = 0;
    std::size_t directory_count = 0;

    /// Entradas que a varredura nao conseguiu ler (permissao negada, arquivo em
    /// uso, disco removido). Ficam contadas para que o total possa ser
    /// apresentado como aproximado em vez de passar por exato.
    std::size_t skipped_count = 0;

    /// Falso quando a varredura foi cancelada ou interrompida. O resultado
    /// continua utilizavel, mas parcial — e a interface precisa dizer isso.
    bool completed = false;

    std::vector<DirectoryRollup> directories;
    std::vector<LargeFile> largest_files;
};

struct ScanOptions {
    /// Quantos arquivos maiores guardar. A varredura inteira nao cabe em
    /// memoria em disco grande, entao so o topo e retido.
    std::size_t largest_files_kept = 100;

    /// Ate que profundidade agregar por diretorio. Mais fundo que isso vira
    /// ruido na interface e explode o tamanho do historico.
    std::size_t rollup_depth = 2;
};

/// Percorre uma arvore de diretorios somando tamanho, sem alterar nada.
///
/// Nao atravessa reparse points (junctions e links): seguir um deles causaria
/// contagem dobrada e, num link circular, laco infinito.
class StorageScanner {
public:
    explicit StorageScanner(ScanOptions options = {});

    [[nodiscard]] ScanResult scan(const std::filesystem::path& root) const;

    /// Versao cancelavel. A varredura confere o token a cada entrada, entao
    /// para em bem menos de um segundo mesmo em arvore grande.
    [[nodiscard]] ScanResult scan(const std::filesystem::path& root, std::stop_token token) const;

private:
    ScanOptions options_;
};

}
