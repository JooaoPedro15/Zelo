#pragma once

#include <cstdint>
#include <filesystem>

namespace zelo::scanner {

/// O tamanho de cluster do volume, em bytes.
///
/// Todo arquivo ocupa um numero inteiro de clusters. Milhares de arquivos
/// pequenos ocupam bem mais que a soma dos seus tamanhos, e ignorar isso faria
/// o aplicativo prometer um espaco que a limpeza nao entrega.
///
/// Devolve 4096 quando o volume nao responde: e o valor usual do NTFS, e errar
/// para o padrao e melhor do que tratar o arquivo como se nao ocupasse nada.
[[nodiscard]] std::uint32_t cluster_size_for(const std::filesystem::path& path);

/// Arredonda para cima ao multiplo de cluster.
[[nodiscard]] std::uint64_t rounded_to_cluster(std::uint64_t bytes, std::uint32_t cluster_size);

}
