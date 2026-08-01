#pragma once

namespace cleaner::core {

/// Risco da ACAO recomendada — nao mede o quanto a analise esta certa
/// (isso e confianca, calculada a parte).
enum class RiskLevel {
    /// Normalmente seguro: consultas, temporarios conhecidos, itens recriaveis.
    Green,
    /// Exige revisao do usuario antes de executar.
    Yellow,
    /// Apenas explicado. O aplicativo nunca executa por conta propria.
    Red,

    /// O conteudo nao foi identificado com seguranca.
    ///
    /// Diferente de vermelho, que e uma afirmacao ("isto e perigoso"), este
    /// nivel e uma confissao: o Cleaner nao sabe o que ha ali. Nunca recebe acao
    /// automatica, e a interface diz que nao sabe em vez de inventar um palpite
    /// — que e como um limpador acaba apagando o que fazia falta.
    Unknown,
};

}
