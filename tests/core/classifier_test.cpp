#include <catch2/catch_test_macros.hpp>
#include <core/classify/classifier.hpp>

using cleaner::core::ClassificationRule;
using cleaner::core::ContentClass;
using cleaner::core::ContentClassifier;
using cleaner::core::DataKind;

namespace {

const ContentClassifier& classifier() {
    static const ContentClassifier instance;
    return instance;
}

}

TEST_CASE("o que nao casa com regra nenhuma aparece declarado", "[classifier]") {
    const auto result = classifier().classify("C:\\Users\\Joao\\AppData\\Local\\ProgramaNovo");

    CHECK(result.content_class == ContentClass::NeedsReview);
    CHECK(result.kind == DataKind::Unknown);
    CHECK(result.confidence == 0.0);
    CHECK_FALSE(result.what_it_is.empty());

    // Sem consequencia porque nao ha remocao a oferecer. Prometer que "nada se
    // perde" sobre algo nao identificado seria o palpite que este nivel existe
    // para evitar.
    CHECK(result.consequence.empty());
    CHECK(result.cleaner_id.empty());
}

// A regra que sustenta a limpeza de navegador: dentro do mesmo perfil, cache
// sai e senha fica.
TEST_CASE("no perfil do navegador, cache sai e credencial fica", "[classifier]") {
    const std::string base =
        "C:\\Users\\Joao\\AppData\\Local\\Google\\Chrome\\User Data\\Default\\";

    CHECK(classifier().classify(base + "Cache").content_class == ContentClass::SafeToClean);
    CHECK(classifier().classify(base + "Code Cache").content_class == ContentClass::SafeToClean);
    CHECK(classifier().classify(base + "GPUCache").content_class == ContentClass::SafeToClean);

    CHECK(classifier().classify(base + "Login Data").content_class == ContentClass::Protected);
    CHECK(classifier().classify(base + "Cookies").content_class == ContentClass::Protected);
    CHECK(classifier().classify(base + "History").content_class == ContentClass::Protected);
    CHECK(classifier().classify(base + "Bookmarks").content_class == ContentClass::Protected);
    CHECK(classifier().classify(base + "Extensions").content_class == ContentClass::Protected);
}

// Dado offline nao e cache: sai so por escolha explicita, e com aviso.
TEST_CASE("dado de site offline pede consequencia declarada", "[classifier]") {
    const auto result = classifier().classify(
        "C:\\Users\\Joao\\AppData\\Local\\Microsoft\\Edge\\User Data\\Default\\IndexedDB");

    CHECK(result.content_class == ContentClass::CleanWithConsequence);
    CHECK_FALSE(result.consequence.empty());
    CHECK(result.needs_app_closed);
}

TEST_CASE("o perfil do navegador que nao foi reconhecido continua protegido", "[classifier]") {
    const auto result = classifier().classify(
        "C:\\Users\\Joao\\AppData\\Local\\Google\\Chrome\\User Data\\Default\\PastaNova");

    CHECK(result.content_class == ContentClass::Protected);
}

// Usar Python nao pode impedir limpar o cache de pacotes do Python; e ambiente
// nao pode ser confundido com cache.
TEST_CASE("cache de pacote e ambiente sao coisas diferentes", "[classifier]") {
    const auto cache =
        classifier().classify("C:\\Users\\Joao\\AppData\\Local\\uv\\cache\\wheels");
    CHECK(cache.content_class == ContentClass::CleanWithConsequence);
    CHECK(cache.kind == DataKind::Package);
    CHECK_FALSE(cache.cleaner_id.empty());

    const auto ambiente = classifier().classify("D:\\Projetos\\api\\.venv\\Lib");
    CHECK(ambiente.content_class == ContentClass::NeedsReview);
    CHECK(ambiente.cleaner_id.empty());
}

TEST_CASE("credencial e conversa nunca sao cache", "[classifier]") {
    CHECK(classifier().classify("C:\\Qualquer\\Caminho\\Credentials").content_class ==
          ContentClass::Protected);
    CHECK(classifier().classify("C:\\Users\\Joao\\.ferramenta\\conversations").content_class ==
          ContentClass::Protected);
    CHECK(classifier().classify("C:\\Users\\Joao\\.ferramenta\\sessions").content_class ==
          ContentClass::Protected);
}

TEST_CASE("areas do Windows sem metodo oficial ficam protegidas", "[classifier]") {
    CHECK(classifier().classify("C:\\Windows\\Installer").content_class ==
          ContentClass::Protected);
    CHECK(classifier().classify("C:\\Windows\\WinSxS\\Backup").content_class ==
          ContentClass::Protected);
    CHECK(classifier().classify("C:\\System Volume Information").content_class ==
          ContentClass::Protected);
}

// O carve-out aprovado: a propria Limpeza de Disco esvazia esta pasta.
TEST_CASE("a pasta de temporarios do Windows continua limpavel", "[classifier]") {
    CHECK(classifier().classify("C:\\Windows\\Temp").content_class == ContentClass::SafeToClean);
}

TEST_CASE("todo veredito com acao declara o que se perde", "[classifier]") {
    for (const auto& rule : classifier().rules()) {
        INFO(rule.result.what_it_is);

        if (rule.result.content_class == ContentClass::SafeToClean ||
            rule.result.content_class == ContentClass::CleanWithConsequence) {
            // Oferecer remocao sem dizer o que muda depois nao e consentimento
            // informado, e o projeto trata isso como parte da regra, nao da
            // interface.
            CHECK_FALSE(rule.result.consequence.empty());
            CHECK_FALSE(rule.result.reason.empty());
            CHECK(rule.result.confidence > 0.0);
        }

        if (rule.result.content_class == ContentClass::Protected) {
            CHECK(rule.result.cleaner_id.empty());
        }
    }
}

TEST_CASE("a primeira regra que casa vence", "[classifier]") {
    const ContentClassifier custom{{
        ClassificationRule{{"alvo", "dentro"},
                           false,
                           cleaner::core::Classification{.content_class =
                                                             ContentClass::SafeToClean}},
        ClassificationRule{{"alvo"},
                           false,
                           cleaner::core::Classification{.content_class = ContentClass::Protected}},
    }};

    CHECK(custom.classify("C:\\alvo\\dentro").content_class == ContentClass::SafeToClean);
    CHECK(custom.classify("C:\\alvo\\outra").content_class == ContentClass::Protected);
}

TEST_CASE("exact_leaf distingue a pasta do que ha abaixo dela", "[classifier]") {
    const ContentClassifier custom{{
        ClassificationRule{{"raiz"},
                           true,
                           cleaner::core::Classification{.content_class = ContentClass::Protected}},
    }};

    CHECK(custom.classify("C:\\raiz").content_class == ContentClass::Protected);
    CHECK(custom.classify("C:\\raiz\\filho").content_class == ContentClass::NeedsReview);
}
