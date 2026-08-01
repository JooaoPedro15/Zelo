#include "storage/logging.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <ctime>
#include <system_error>
#include <vector>

namespace cleaner::storage {

namespace {

std::filesystem::path g_log_file;

std::string today_stamp() {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::tm parts{};
    ::localtime_s(&parts, &now);

    char buffer[16]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &parts);
    return buffer;
}

}

void initialize_logging(const std::filesystem::path& directory) {
    if (!g_log_file.empty()) {
        return;
    }

    std::error_code error;
    std::filesystem::create_directories(directory, error);

    g_log_file = directory / ("cleaner-" + today_stamp() + ".log");

    try {
        // Caminho em wide: o perfil do usuario costuma ter acento, e passar o
        // caminho como string estreita faz o CRT ler os bytes UTF-8 como ANSI,
        // apontando para um diretorio que nao existe.
        auto logger = spdlog::basic_logger_mt("cleaner", g_log_file.wstring(), false);
        logger->set_pattern("%Y-%m-%d %H:%M:%S [%l] %v");
        logger->flush_on(spdlog::level::info);
        spdlog::set_default_logger(std::move(logger));
    } catch (const spdlog::spdlog_ex&) {
        // Sem poder gravar log o aplicativo continua funcionando: registro e
        // diagnostico, nao requisito para analisar.
        g_log_file.clear();
    }
}

std::filesystem::path current_log_file() {
    return g_log_file;
}

void reset_logging_for_test() {
    spdlog::drop_all();
    spdlog::set_default_logger(spdlog::null_logger_mt("null"));
    spdlog::drop("null");
    g_log_file.clear();
}

void apply_log_retention(const std::filesystem::path& directory, int keep_days) {
    const auto cutoff = std::chrono::file_clock::now() - std::chrono::hours(24 * keep_days);

    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (!entry.is_regular_file(error) || entry.path().extension() != ".log") {
            continue;
        }

        const auto written = std::filesystem::last_write_time(entry.path(), error);
        if (!error && written < cutoff) {
            std::filesystem::remove(entry.path(), error);
        }
    }
}

}
