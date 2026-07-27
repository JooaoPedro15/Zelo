#pragma once

namespace zelo::core {

/// Risco da ACAO recomendada — nao mede o quanto a analise esta certa
/// (isso e confianca, calculada a parte).
enum class RiskLevel {
    /// Normalmente seguro: consultas, temporarios conhecidos, itens recriaveis.
    Green,
    /// Exige revisao do usuario antes de executar.
    Yellow,
    /// Apenas explicado. O aplicativo nunca executa por conta propria.
    Red,
};

}
