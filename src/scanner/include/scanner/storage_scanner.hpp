#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stop_token>
#include <string>
#include <vector>

namespace zelo::scanner {

/// Estado de um arquivo gerenciado por servico de nuvem.
enum class CloudState {
    /// Nao e arquivo de nuvem, ou esta inteiramente no disco.
    Local,

    /// O conteudo vive na nuvem; o que esta aqui e so o marcador. Apagar isto
    /// nao libera espaco — e, dependendo de como for apagado, remove o arquivo
    /// da nuvem e dos outros dispositivos.
    OnlineOnly,
};

struct LargeFile {
    std::string path;

    /// O tamanho que o arquivo declara ter.
    std::uint64_t bytes = 0;

    /// O que ele realmente ocupa no disco. Difere do declarado em arquivo
    /// compactado, esparso ou menor que um cluster. E este o numero que importa
    /// para prometer espaco liberado.
    std::uint64_t allocated_bytes = 0;

    std::string created_at;
    std::string modified_at;
    std::string last_access_at;

    /// O Windows costuma vir com a atualizacao de ultimo acesso desligada. Sem
    /// esta ressalva, a data levaria o usuario a apagar algo por engano.
    bool last_access_reliable = false;

    CloudState cloud_state = CloudState::Local;
};

struct DirectoryRollup {
    std::string path;
    std::uint64_t total_bytes = 0;
    std::size_t file_count = 0;
};

struct ScanResult {
    /// Soma dos tamanhos declarados pelos arquivos.
    std::uint64_t total_bytes = 0;

    /// Soma do espaco realmente ocupado no disco, com hard link contado uma vez
    /// so. E este o numero que pode ser prometido como espaco a liberar.
    std::uint64_t allocated_bytes = 0;

    /// Conteudo que mora na nuvem e nao ocupa espaco local. Fica separado para
    /// nao ser confundido com espaco recuperavel.
    std::uint64_t online_only_bytes = 0;

    std::size_t file_count = 0;
    std::size_t directory_count = 0;

    /// Arquivos que apareceram com mais de um nome no disco. O espaco deles
    /// entra na conta uma vez.
    std::size_t hard_link_duplicates = 0;

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

/// A partir deste tamanho o scanner confere se o arquivo ja foi contado sob
/// outro nome (hard link).
///
/// Conferir exige abrir o arquivo, e uma varredura de disco passa por centenas
/// de milhares deles. O limite e o meio-termo: abaixo dele, um hard link pode
/// ser contado duas vezes — o erro fica na casa dos kilobytes, irrelevante para
/// um total em gigabytes.
inline constexpr std::uint64_t kHardLinkCheckThreshold = 1024ULL * 1024ULL;

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
