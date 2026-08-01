#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace zelo::storage {

/// O que aconteceria — ou o que aconteceu — ao liberar espaco local de uma pasta
/// sincronizada.
struct CloudReleasePlan {
    std::string root;

    /// Arquivos baixados que voltariam a ser apenas ponteiro para a nuvem.
    std::size_t file_count = 0;

    /// Quanto isso libera no disco. O conteudo continua na nuvem: este numero e
    /// espaco recuperado, nao arquivo perdido.
    std::uint64_t bytes = 0;

    /// Arquivos que o usuario marcou para manter sempre no dispositivo. Ficam de
    /// fora: aquela marca foi uma escolha dele, nao um descuido.
    std::size_t pinned_kept = 0;

    /// Falso quando a leitura da pasta nao terminou.
    bool complete = false;
};

/// Mede o que daria para liberar, sem alterar nada.
[[nodiscard]] CloudReleasePlan plan_cloud_release(const std::filesystem::path& root);

struct CloudReleaseOutcome {
    std::size_t released = 0;
    std::size_t failed = 0;

    /// Bytes que devem voltar ao disco livre quando o servico terminar de
    /// esvaziar os arquivos.
    std::uint64_t bytes = 0;
};

/// Libera o espaco local de uma pasta sincronizada.
///
/// Nao apaga nada. Marca os arquivos como "liberar espaco", que e o mesmo que o
/// Explorador faz, e o servico de nuvem esvazia o conteudo local mantendo o
/// arquivo na nuvem e nos outros dispositivos.
///
/// O espaco nao aparece livre no mesmo instante: quem esvazia e o servico, no
/// ritmo dele. Prometer o numero imediato seria prometer o que nao depende do
/// Zelo.
CloudReleaseOutcome release_cloud_space(const std::filesystem::path& root);

}
