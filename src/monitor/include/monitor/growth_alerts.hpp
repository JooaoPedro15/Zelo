#pragma once

#include "monitor/growth_report.hpp"
#include "monitor/snapshot.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace zelo::monitor {

/// O que disparou o alerta.
enum class AlertKind {
    /// Espaco livre abaixo do minimo configurado.
    LowFreeSpace,

    /// Muito espaco consumido em pouco tempo.
    FastGrowth,

    /// Uma unica pasta cresceu muito.
    FolderJump,
};

struct Alert {
    AlertKind kind = AlertKind::LowFreeSpace;

    std::string title;

    /// O que sustenta o alerta, em numeros. A regra do projeto vale aqui
    /// tambem: nada aparece sem o usuario poder conferir de onde veio.
    std::string evidence;

    /// Quando o crescimento comecou, quando da para dizer.
    std::string since;

    /// Pastas envolvidas, da que mais cresceu para a que menos.
    std::vector<std::string> folders;

    std::string suggested_action;
};

/// Quando avisar. Valores pensados para uso comum; a interface permitira
/// ajustar.
struct AlertThresholds {
    /// Abaixo disto o Windows comeca a falhar em coisas basicas.
    std::uint64_t minimum_free_bytes = 10ULL * 1024 * 1024 * 1024;

    /// Fracao livre minima. Disco grande com 10 GB livres ainda pode estar
    /// apertado em proporcao.
    double minimum_free_ratio = 0.05;

    /// Consumo que justifica aviso, e em quanto tempo.
    std::uint64_t fast_growth_bytes = 5ULL * 1024 * 1024 * 1024;
    int fast_growth_hours = 24;

    /// Uma pasta sozinha crescendo isto merece explicacao.
    std::uint64_t folder_jump_bytes = 2ULL * 1024 * 1024 * 1024;
};

/// Avalia o que merece aviso, a partir do retrato atual e do que mudou.
///
/// Alerta so existe com numero atras. Avisar sem poder mostrar o que sustenta o
/// aviso e alarmismo — e o projeto ja decidiu que a pontuacao nunca serve para
/// assustar.
[[nodiscard]] std::vector<Alert> evaluate_alerts(const Snapshot& current,
                                                 const GrowthReport& growth,
                                                 const AlertThresholds& thresholds = {});

}
