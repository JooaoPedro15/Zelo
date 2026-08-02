#pragma once

#include "core/classify/content_class.hpp"
#include "core/risk/risk_level.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace cleaner::core {

/// O que um limpador declara sobre si mesmo.
///
/// Tudo que um limpador pode fazer esta escrito aqui, em dados. Ele nao recebe
/// caminho de fora, nao monta comando e nao decide em tempo de execucao o que
/// vai apagar: as pastas permitidas, o que entra, o que fica e o que se perde
/// sao parte da declaracao, revisada e testada.
struct CleanerSpec {
    std::string id;
    std::string display_name;

    /// O que este limpador remove, em uma frase.
    std::string description;

    std::string application;

    /// Pastas que este limpador pode tocar, ja com as variaveis de ambiente
    /// resolvidas por quem monta a lista.
    ///
    /// Fora delas o limpador nao apaga nada, aconteca o que acontecer: um link
    /// que aponte para fora, um caminho trocado no meio do caminho, uma
    /// variavel manipulada. A checagem e refeita imediatamente antes de cada
    /// exclusao.
    std::vector<std::string> allowed_roots;

    /// Nomes de arquivo ou pasta que ficam, mesmo estando dentro de uma raiz
    /// permitida. Comparados sem diferenciar maiusculas.
    std::vector<std::string> preserved_names;

    ContentClass content_class = ContentClass::NeedsReview;
    RiskLevel risk = RiskLevel::Unknown;

    /// Quanto a identificacao do conteudo se sustenta, de 0 a 1.
    double confidence = 0.0;

    /// O que muda depois da limpeza.
    std::string consequence;

    /// O que fica para tras, dito ao usuario antes de confirmar. Sem isto a
    /// confirmacao so informa o que sai, e quem aprova nao sabe o que sobra.
    std::string preserved_description;

    bool needs_app_closed = false;
    bool needs_admin = false;
    bool needs_restart = false;

    /// Falso quando o conteudo nao volta sozinho. Nao promete restauracao pelo
    /// Cleaner: diz apenas se o programa dono recria o que foi removido.
    bool regenerates_itself = true;

    /// Versao da regra. Muda quando o conteudo do limpador muda, para o
    /// historico saber sob qual regra cada limpeza aconteceu.
    int rule_version = 1;
};

/// O que uma limpeza faria, medido sem alterar nada.
struct CleanerPreview {
    std::string cleaner_id;
    std::size_t file_count = 0;
    std::uint64_t bytes = 0;

    /// Arquivos que a propria declaracao manda preservar.
    std::size_t preserved_count = 0;

    /// Caminhos recusados, com o motivo. Recusa em silencio esconde justamente
    /// o caso em que o limpador prometeu espaco que nao consegue liberar.
    std::vector<std::string> rejected;

    bool complete = false;
};

/// O que aconteceu de fato.
struct CleanerOutcome {
    std::string cleaner_id;

    std::size_t removed_count = 0;
    std::size_t skipped_count = 0;
    std::size_t failed_count = 0;

    /// Soma dos tamanhos dos arquivos removidos.
    std::uint64_t bytes = 0;

    /// Diferenca no espaco livre do volume, medida antes e depois.
    ///
    /// Nao e igual a soma acima: outros programas escrevem enquanto a limpeza
    /// roda. Mostrar os dois lado a lado e o que separa estimativa de
    /// resultado.
    std::int64_t free_space_delta = 0;

    /// Por que cada arquivo ficou, agrupado. Vai para o historico e para a tela
    /// final.
    std::vector<std::string> reasons;
};

}
