#pragma once

namespace cleaner::core {

/// As areas em que a saude do computador e medida.
///
/// Vive junto dos modelos, e nao do calculo da pontuacao, porque cada
/// recomendacao precisa declarar a que area pertence. Deduzir isso do tipo da
/// acao nao funciona: uma leitura de eventos e uma leitura de disco sao ambas
/// analise somente leitura, e mesmo assim falam de areas diferentes.
enum class HealthCategory {
    Storage,
    WindowsIntegrity,
    Startup,
    Disks,
    Updates,
    Security,
    Performance,
    Stability,
};

}
