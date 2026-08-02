#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cleaner::core {

/// Uma pasta e tudo que ha abaixo dela.
struct SpaceNode {
    std::string path;

    /// Nome curto para a arvore. Na raiz, o caminho inteiro.
    std::string display_name;

    /// O que ocupa disco de verdade, somando a subarvore. E este o numero que
    /// pode virar espaco livre.
    std::uint64_t allocated_bytes = 0;

    /// O que os arquivos declaram ter. Difere do alocado em arquivo compactado,
    /// esparso ou menor que um cluster.
    std::uint64_t logical_bytes = 0;

    /// Conteudo que mora na nuvem. Nao ocupa disco: fica separado para nao ser
    /// confundido com espaco recuperavel.
    std::uint64_t online_only_bytes = 0;

    std::size_t file_count = 0;

    /// Quantas pastas abaixo desta nao puderam ser lidas. Enquanto for maior
    /// que zero, o tamanho e um piso, nao um total.
    std::size_t unreadable_count = 0;

    std::vector<SpaceNode> children;

    [[nodiscard]] bool complete() const { return unreadable_count == 0; }
};

/// O retrato de um volume: o que o Windows diz, o que a varredura viu, e a
/// diferenca entre os dois.
///
/// A diferenca e o ponto. Um limpador que so mostra o que reconhece deixa o
/// usuario com a impressao de que o disco esta cheio de nada; declarar quanto
/// ficou sem explicacao transforma isso em uma pergunta que da para perseguir.
struct SpaceSurvey {
    std::string volume;

    /// Informados pelo Windows.
    std::uint64_t volume_total_bytes = 0;
    std::uint64_t volume_free_bytes = 0;

    /// Somado pela varredura.
    std::uint64_t identified_bytes = 0;

    std::size_t unreadable_count = 0;
    std::vector<std::string> unreadable_examples;

    SpaceNode root;

    /// Falso quando a varredura foi cancelada. O resultado continua utilizavel,
    /// mas parcial.
    bool complete = false;

    [[nodiscard]] std::uint64_t used_bytes() const {
        return volume_total_bytes > volume_free_bytes ? volume_total_bytes - volume_free_bytes : 0;
    }

    /// Quanto do espaco usado a varredura nao conseguiu apontar.
    ///
    /// Pode ser negativo: hard link contado uma vez so, arquivo que cresceu
    /// durante a leitura e o proprio Windows arredondando fazem os dois lados
    /// nao baterem exatamente. Negativo perto de zero e ruido; positivo grande
    /// e um lugar onde falta olhar.
    [[nodiscard]] std::int64_t unexplained_bytes() const {
        return static_cast<std::int64_t>(used_bytes()) -
               static_cast<std::int64_t>(identified_bytes);
    }

    /// Fracao do espaco usado que a varredura explica, de 0 a 1.
    [[nodiscard]] double coverage() const {
        const auto used = used_bytes();
        if (used == 0) {
            return 1.0;
        }
        const double ratio = static_cast<double>(identified_bytes) / static_cast<double>(used);
        return ratio > 1.0 ? 1.0 : ratio;
    }
};

}
