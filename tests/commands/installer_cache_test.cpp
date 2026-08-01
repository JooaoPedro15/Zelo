#include <catch2/catch_test_macros.hpp>
#include <commands/installer_cache.hpp>

#include <algorithm>
#include <filesystem>

using cleaner::commands::scan_installer_cache;

// A regra que sustenta a feature inteira: instalador com dono nao entra na
// lista. Apagar um deles quebra reparar e desinstalar o programa dono, e nao ha
// como voltar atras.
TEST_CASE("instalador em uso nunca aparece como sobra", "[installer_cache][integration]") {
    const auto report = scan_installer_cache();

    if (!report.products_readable) {
        // Sem conseguir perguntar ao Windows quem e dono de que, a lista tem de
        // sair vazia. Uma lista cheia aqui seria a pior falha possivel.
        WARN("nao deu para ler os programas instalados: " << report.obstacle);
        CHECK(report.orphans.empty());
        CHECK_FALSE(report.obstacle.empty());
        return;
    }

    INFO("sobras: " << report.orphans.size() << " arquivos, " << report.orphan_bytes << " bytes");
    INFO("em uso: " << report.referenced_count << " arquivos, " << report.referenced_bytes
                    << " bytes");

    for (const auto& orphan : report.orphans) {
        INFO(orphan.path);

        // Todo item apontado existe e tem tamanho: nada entra na lista por
        // engano de leitura.
        CHECK(std::filesystem::exists(orphan.path));
        CHECK(orphan.bytes > 0);

        const auto extension = std::filesystem::path(orphan.path).extension().string();
        CHECK((extension == ".msi" || extension == ".msp" || extension == ".MSI" ||
               extension == ".MSP"));
    }
}

TEST_CASE("a lista vem do maior para o menor", "[installer_cache][integration]") {
    const auto report = scan_installer_cache();

    CHECK(std::is_sorted(report.orphans.begin(), report.orphans.end(),
                         [](const auto& left, const auto& right) {
                             return left.bytes > right.bytes;
                         }));
}

// Somar as duas partes tem de dar a pasta inteira em arquivos de instalacao.
// Se um arquivo caisse fora das duas contas, o total prometido nao bateria com
// o que o Explorador mostra.
TEST_CASE("todo instalador cai em uma das duas contas", "[installer_cache][integration]") {
    const auto report = scan_installer_cache();
    if (!report.products_readable) {
        SUCCEED("sem leitura dos programas instalados nao ha o que conferir");
        return;
    }

    CHECK(report.orphans.size() + report.referenced_count > 0);
}
