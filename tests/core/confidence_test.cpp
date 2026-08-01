#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <core/confidence/confidence.hpp>

#include <string>
#include <vector>

using Catch::Approx;
using cleaner::core::Confidence;

TEST_CASE("confianca soma o peso dos sinais observados", "[confidence]") {
    const Confidence confidence = Confidence::from_signals({
        {"instalado ha mais de um ano", 0.20},
        {"nenhum processo ou servico ativo", 0.20},
    });

    CHECK(confidence.value() == Approx(0.40));
}

// O app trabalha com indicios, nunca com certeza. Exibir 100% prometeria ao
// usuario algo que a analise nao consegue sustentar.
TEST_CASE("confianca nunca passa de 95 por cento", "[confidence]") {
    const Confidence confidence = Confidence::from_signals({
        {"sinal forte", 0.60},
        {"outro sinal forte", 0.60},
    });

    CHECK(confidence.value() == Approx(0.95));
}

// A recomendacao mostra ao usuario por que chegou a conclusao. Listar um sinal
// que nao pontuou daria a impressao de evidencia que nao existe.
TEST_CASE("motivos sao exatamente os sinais que pontuaram", "[confidence]") {
    const Confidence confidence = Confidence::from_signals({
        {"instalado ha mais de um ano", 0.20},
        {"nenhum arquivo modificado em seis meses", 0.15},
        {"data de ultimo acesso indisponivel", 0.0},
    });

    const std::vector<std::string> expected{
        "instalado ha mais de um ano",
        "nenhum arquivo modificado em seis meses",
    };

    CHECK(confidence.reasons() == expected);
}

// Um sinal pode pesar contra a conclusao — o mais forte deles e o usuario
// dizendo que usa o programa. A soma nao pode furar o piso: confianca vive
// entre 0 e 95 por cento.
TEST_CASE("confianca nunca fica negativa", "[confidence]") {
    const Confidence confidence = Confidence::from_signals({
        {"instalado ha mais de um ano", 0.20},
        {"usuario marcou que usa o programa", -1.00},
    });

    CHECK(confidence.value() == Approx(0.0));
    CHECK(confidence.reasons().size() == 2);
}

// Sem sinal coletado nao ha confianca. Ausencia de dado nunca vira certeza:
// "nao encontrei indicio de uso" e diferente de "o programa nao e usado".
TEST_CASE("sem sinais a confianca e zero", "[confidence]") {
    const Confidence confidence = Confidence::from_signals({});

    CHECK(confidence.value() == Approx(0.0));
    CHECK(confidence.reasons().empty());
}
