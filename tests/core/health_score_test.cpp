#include <catch2/catch_test_macros.hpp>
#include <core/scoring/health_score.hpp>

#include <vector>

using zelo::core::HealthCategory;
using zelo::core::HealthDeduction;
using zelo::core::HealthScore;

TEST_CASE("sem deducoes todas as categorias valem 100", "[health_score]") {
    const HealthScore score = HealthScore::from_deductions({});

    CHECK(score.overall() == 100);
    CHECK(score.of(HealthCategory::Storage) == 100);
    CHECK(score.of(HealthCategory::Stability) == 100);
}

TEST_CASE("desconto reduz apenas a categoria a que pertence", "[health_score]") {
    const HealthScore score = HealthScore::from_deductions({
        {HealthCategory::Storage, 10, "disco C: com menos de 10 por cento livre"},
        {HealthCategory::Storage, 5, "arquivos temporarios ocupando 12 GB"},
        {HealthCategory::Startup, 3, "oito programas nao essenciais iniciam com o Windows"},
    });

    CHECK(score.of(HealthCategory::Storage) == 85);
    CHECK(score.of(HealthCategory::Startup) == 97);
    CHECK(score.of(HealthCategory::Disks) == 100);
}

// A pontuacao orienta, nao assusta. Mesmo com muitos problemas empilhados
// numa categoria ela para em zero em vez de virar um numero negativo.
TEST_CASE("categoria nao fica abaixo de zero", "[health_score]") {
    const HealthScore score = HealthScore::from_deductions({
        {HealthCategory::Storage, 80, "disco cheio"},
        {HealthCategory::Storage, 50, "sem espaco para arquivos temporarios"},
    });

    CHECK(score.of(HealthCategory::Storage) == 0);
}

// Os pesos da secao 13 do planejamento ficam fixados aqui: mudar a importancia
// de uma categoria exige mudar este teste, e portanto ser decisao consciente.
TEST_CASE("pontuacao geral e media ponderada das categorias", "[health_score]") {
    SECTION("armazenamento pesa 15 por cento") {
        const HealthScore score =
            HealthScore::from_deductions({{HealthCategory::Storage, 40, "disco C: quase cheio"}});

        CHECK(score.overall() == 94);
    }

    SECTION("inicializacao pesa 5 por cento") {
        const HealthScore score = HealthScore::from_deductions(
            {{HealthCategory::Startup, 40, "muitos programas na inicializacao"}});

        CHECK(score.overall() == 98);
    }

    SECTION("categoria zerada nao zera o geral") {
        const HealthScore score =
            HealthScore::from_deductions({{HealthCategory::Storage, 100, "disco C: sem espaco"}});

        CHECK(score.overall() == 85);
    }

    // Se os pesos nao somassem 1.0, zerar tudo daria um numero diferente de
    // zero — e a pontuacao geral estaria mentindo sobre a escala.
    SECTION("todas as categorias zeradas dao geral zero") {
        const HealthScore score = HealthScore::from_deductions({
            {HealthCategory::Storage, 100, "x"},
            {HealthCategory::WindowsIntegrity, 100, "x"},
            {HealthCategory::Disks, 100, "x"},
            {HealthCategory::Stability, 100, "x"},
            {HealthCategory::Performance, 100, "x"},
            {HealthCategory::Updates, 100, "x"},
            {HealthCategory::Security, 100, "x"},
            {HealthCategory::Startup, 100, "x"},
        });

        CHECK(score.overall() == 0);
    }
}

// Nenhum ponto some sem explicacao: a interface precisa conseguir mostrar por
// que a categoria perdeu pontos.
TEST_CASE("cada desconto fica visivel com a causa", "[health_score]") {
    const HealthScore score = HealthScore::from_deductions({
        {HealthCategory::Storage, 10, "disco C: com menos de 10 por cento livre"},
        {HealthCategory::Storage, 5, "arquivos temporarios ocupando 12 GB"},
        {HealthCategory::Startup, 3, "oito programas nao essenciais iniciam com o Windows"},
    });

    const std::vector<HealthDeduction>& storage = score.deductions_for(HealthCategory::Storage);

    REQUIRE(storage.size() == 2);
    CHECK(storage.front().points == 10);
    CHECK(storage.front().cause == "disco C: com menos de 10 por cento livre");
    CHECK(score.deductions_for(HealthCategory::Disks).empty());
}
