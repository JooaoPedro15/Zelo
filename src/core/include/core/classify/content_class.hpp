#pragma once

#include <string>

namespace cleaner::core {

/// O que se pode fazer com um conteudo encontrado no disco.
///
/// Separado de `RiskLevel` de proposito. Risco fala da acao recomendada dentro
/// de um achado; isto fala do conteudo em si, que a arvore de espaco mostra
/// mesmo quando nao ha achado nenhum associado.
enum class ContentClass {
    /// Temporario ou cache comprovadamente recriavel.
    SafeToClean,

    /// Pode sair, mas custa alguma coisa: baixar de novo, recompilar, entrar de
    /// novo, perder historico.
    CleanWithConsequence,

    /// Grande o bastante para importar e sem identificacao confiavel.
    ///
    /// Nunca some da tela. Um limpador que esconde o que nao entendeu deixa o
    /// usuario sem saber onde procurar o resto do disco.
    NeedsReview,

    /// Nao sai por acao automatica.
    Protected,
};

/// Que especie de dado e aquilo. Serve para a interface agrupar e para o
/// usuario decidir sem precisar conhecer o programa.
enum class DataKind {
    Cache,
    Log,
    TemporaryFile,
    Configuration,
    Credential,
    Session,
    UserContent,
    Package,
    Environment,
    Extension,
    Database,
    Installer,
    SystemComponent,
    Unknown,
};

/// O veredito sobre um caminho.
struct Classification {
    ContentClass content_class = ContentClass::NeedsReview;
    DataKind kind = DataKind::Unknown;

    /// Programa dono, quando da para nomear.
    std::string application;

    /// O que ha ali, em linguagem de quem nao conhece o programa.
    std::string what_it_is;

    /// Por que caiu nesta classe. E a diferenca entre uma etiqueta e uma
    /// explicacao: sem o motivo, o usuario so pode acreditar ou nao.
    std::string reason;

    /// O que muda depois de remover. Vazio quando nao ha remocao a oferecer.
    std::string consequence;

    /// Quanto a identificacao se sustenta, de 0 a 1.
    double confidence = 0.0;

    /// O programa dono precisa estar fechado para a limpeza valer a pena.
    bool needs_app_closed = false;

    bool needs_admin = false;

    /// Qual limpador cuida deste conteudo. Vazio quando nao ha nenhum — e o
    /// caso da maioria do disco, que so e medido e mostrado.
    std::string cleaner_id;
};

[[nodiscard]] std::string to_string(ContentClass value);
[[nodiscard]] std::string to_string(DataKind value);

/// A classificacao de quem nao foi reconhecido.
///
/// Existe como funcao, e nao como valor padrao espalhado, para que o texto
/// mostrado ao usuario seja o mesmo em toda a interface.
[[nodiscard]] Classification unclassified();

}
