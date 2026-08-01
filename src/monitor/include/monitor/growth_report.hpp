#pragma once

#include "monitor/snapshot.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace cleaner::monitor {

/// O quanto se sabe sobre quem gerou os arquivos.
///
/// Existe para o relatorio nunca afirmar mais do que consegue provar: dizer que
/// um programa criou um arquivo sem evidencia levaria o usuario a desinstalar
/// ou culpar a coisa errada.
enum class AttributionConfidence {
    /// Um evento do sistema ligou a gravacao a um processo.
    Confirmed,

    /// A pasta pertence exclusivamente ao programa.
    HighlyLikely,

    /// O caminho bate com um mapa conhecido, mas outros programas escrevem ali.
    PossiblyRelated,

    /// Nada permitiu identificar a origem.
    Unknown,
};

/// Uma linha do relatorio de crescimento.
struct GrowthItem {
    std::string path;

    /// Quanto esta pasta cresceu, ja descontado o que as subpastas listadas
    /// explicam. Somar esta coluna da o crescimento real, sem repetir bytes.
    std::int64_t exclusive_bytes = 0;

    /// Quanto a subarvore inteira cresceu, incluindo as subpastas.
    std::int64_t subtree_bytes = 0;

    std::string application;
    AttributionConfidence attribution = AttributionConfidence::Unknown;
};

struct GrowthReport {
    std::string from_taken_at;
    std::string to_taken_at;

    /// Quanto o espaco livre mudou. Negativo quando o disco encheu.
    std::int64_t free_space_delta = 0;

    /// Pastas que cresceram, da maior para a menor.
    std::vector<GrowthItem> items;

    /// Pastas que encolheram — explicam espaco recuperado.
    std::vector<GrowthItem> shrunk;
};

/// Abaixo disso a linha vira ruido: uma lista com centenas de pastas que
/// cresceram alguns kilobytes esconde as que importam.
inline constexpr std::int64_t kGrowthNoiseFloor = 10LL * 1024 * 1024;

/// Transforma a comparacao entre dois retratos em algo que responde "o que
/// cresceu".
///
/// O trabalho principal e nao contar o mesmo byte varias vezes: uma pasta que
/// cresce faz todos os seus pais crescerem junto, e somar as linhas daria um
/// total muitas vezes maior que o espaco que realmente sumiu.
[[nodiscard]] GrowthReport build_growth_report(const SnapshotDiff& diff);

}
