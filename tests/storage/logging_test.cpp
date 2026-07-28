#include <catch2/catch_test_macros.hpp>
#include <storage/logging.hpp>

#include <spdlog/spdlog.h>

#include <fstream>
#include <string>

using zelo::storage::apply_log_retention;
using zelo::storage::current_log_file;
using zelo::storage::initialize_logging;

TEST_CASE("o log e criado no diretorio pedido", "[logging]") {
    const auto directory = std::filesystem::temp_directory_path() / "zelo-log-test";

    std::error_code error;
    std::filesystem::remove_all(directory, error);

    initialize_logging(directory);
    spdlog::info("linha de teste");
    spdlog::default_logger()->flush();

    const auto file = current_log_file();

    REQUIRE_FALSE(file.empty());
    CHECK(std::filesystem::exists(file));
    CHECK(file.parent_path() == directory);
    CHECK(file.extension() == ".log");
}

// O perfil de quem usa o Zelo costuma ter acento — "C:\Users\João". O %TEMP%
// desta maquina usa nome curto sem acento, entao um teste que so use TEMP
// passaria sem nunca exercitar esse caminho.
TEST_CASE("o log funciona em caminho com acento", "[logging]") {
    const auto directory = std::filesystem::temp_directory_path() / "zelo-João-Programação";

    std::error_code error;
    std::filesystem::remove_all(directory, error);

    zelo::storage::reset_logging_for_test();
    initialize_logging(directory);

    REQUIRE_FALSE(current_log_file().empty());

    spdlog::info("linha em caminho acentuado");
    spdlog::default_logger()->flush();

    CHECK(std::filesystem::exists(current_log_file()));
    CHECK(std::filesystem::file_size(current_log_file(), error) > 0);

    std::filesystem::remove_all(directory, error);
}

// Registro e diagnostico, nao requisito para analisar. Nao conseguir gravar
// nao pode impedir o aplicativo de rodar.
TEST_CASE("diretorio invalido nao derruba a inicializacao", "[logging]") {
    CHECK_NOTHROW(initialize_logging(std::filesystem::path{}));
    CHECK_NOTHROW(spdlog::info("segue funcionando"));
}

TEST_CASE("a retencao apaga apenas os logs antigos", "[logging]") {
    const auto directory = std::filesystem::temp_directory_path() / "zelo-log-retention";

    std::error_code error;
    std::filesystem::remove_all(directory, error);
    std::filesystem::create_directories(directory, error);

    const auto recent = directory / "zelo-recente.log";
    const auto old = directory / "zelo-antigo.log";
    const auto other = directory / "nao-e-log.txt";

    for (const auto& file : {recent, old, other}) {
        std::ofstream{file} << "conteudo";
    }

    std::filesystem::last_write_time(
        old, std::filesystem::file_time_type::clock::now() - std::chrono::hours(24 * 40), error);

    apply_log_retention(directory, 30);

    CHECK(std::filesystem::exists(recent));
    CHECK_FALSE(std::filesystem::exists(old));

    // Arquivo que nao e log fica de fora: a limpeza cuida do que ela criou.
    CHECK(std::filesystem::exists(other));

    std::filesystem::remove_all(directory, error);
}
