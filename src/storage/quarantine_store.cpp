#include "storage/quarantine_store.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <ctime>
#include <fstream>
#include <utility>

namespace zelo::storage {

namespace {

std::string now_iso8601() {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::tm parts{};
    ::localtime_s(&parts, &now);

    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &parts);
    return buffer;
}

std::string make_id() {
    static int sequence = 0;

    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::tm parts{};
    ::localtime_s(&parts, &now);

    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y%m%d-%H%M%S", &parts);
    return std::string(buffer) + "-" + std::to_string(sequence++);
}

}

QuarantineStore::QuarantineStore(std::filesystem::path root, core::ProtectedPaths protected_paths)
    : root_(std::move(root)), protected_paths_(std::move(protected_paths)) {
    std::error_code error;
    std::filesystem::create_directories(root_ / "arquivos", error);
}

std::filesystem::path QuarantineStore::manifest_path() const {
    return root_ / "quarentena.json";
}

std::filesystem::path QuarantineStore::storage_path(const std::string& id) const {
    return root_ / "arquivos" / id;
}

std::vector<QuarantineEntry> QuarantineStore::entries() const {
    std::ifstream file(manifest_path());
    if (!file) {
        return {};
    }

    nlohmann::json document;
    try {
        file >> document;
    } catch (const nlohmann::json::exception&) {
        spdlog::warn("registro da quarentena ilegivel; tratando como vazio");
        return {};
    }

    std::vector<QuarantineEntry> entries;
    for (const auto& item : document.value("entries", nlohmann::json::array())) {
        entries.push_back(QuarantineEntry{
            .id = item.value("id", std::string{}),
            .original_path = item.value("original_path", std::string{}),
            .size_bytes = item.value("size_bytes", std::uint64_t{0}),
            .quarantined_at = item.value("quarantined_at", std::string{}),
            .reason = item.value("reason", std::string{}),
        });
    }
    return entries;
}

void QuarantineStore::write_entries(const std::vector<QuarantineEntry>& entries) const {
    nlohmann::json items = nlohmann::json::array();
    for (const auto& entry : entries) {
        items.push_back(nlohmann::json{
            {"id", entry.id},
            {"original_path", entry.original_path},
            {"size_bytes", entry.size_bytes},
            {"quarantined_at", entry.quarantined_at},
            {"reason", entry.reason},
        });
    }

    const nlohmann::json document{{"schema", 1}, {"entries", items}};

    // Escrita atomica: uma queda no meio da gravacao nao pode deixar o registro
    // pela metade, senao a quarentena perde a referencia de arquivos que ja
    // saiu do lugar de origem.
    const auto temporary = manifest_path().string() + ".tmp";
    {
        std::ofstream file(temporary, std::ios::trunc);
        file << document.dump(2);
    }

    std::error_code error;
    std::filesystem::rename(temporary, manifest_path(), error);
    if (error) {
        std::filesystem::remove(manifest_path(), error);
        std::filesystem::rename(temporary, manifest_path(), error);
    }
}

std::optional<QuarantineEntry> QuarantineStore::take(const std::filesystem::path& file,
                                                     const std::string& reason) const {
    std::error_code error;

    if (!std::filesystem::is_regular_file(file, error)) {
        return std::nullopt;
    }

    // A deny-list decide de novo, agora. A recomendacao pode ter sido criada
    // minutos atras, e o que vale e o estado no momento de mexer no disco.
    if (protected_paths_.is_protected(file.string())) {
        spdlog::warn("quarentena recusada: caminho protegido");
        return std::nullopt;
    }

    const auto size = std::filesystem::file_size(file, error);
    if (error) {
        return std::nullopt;
    }

    QuarantineEntry entry{
        .id = make_id(),
        .original_path = file.string(),
        .size_bytes = size,
        .quarantined_at = now_iso8601(),
        .reason = reason,
    };

    // Mover em vez de copiar e apagar: dentro do mesmo volume a operacao e
    // atomica, entao nao existe instante em que o arquivo esta nos dois lugares
    // ou em nenhum.
    std::filesystem::rename(file, storage_path(entry.id), error);
    if (error) {
        // Volumes diferentes ou arquivo em uso. Copiar e so entao remover.
        std::filesystem::copy_file(file, storage_path(entry.id),
                                   std::filesystem::copy_options::overwrite_existing, error);
        if (error) {
            spdlog::warn("nao foi possivel mover para a quarentena: {}", error.message());
            return std::nullopt;
        }

        std::filesystem::remove(file, error);
        if (error) {
            // A copia ficou, mas o original nao saiu. Desfazer a copia mantem
            // os dois lados consistentes.
            std::error_code cleanup;
            std::filesystem::remove(storage_path(entry.id), cleanup);
            spdlog::warn("original nao pode ser removido; quarentena desfeita");
            return std::nullopt;
        }
    }

    auto all = entries();
    all.push_back(entry);
    write_entries(all);

    return entry;
}

bool QuarantineStore::restore(const std::string& id) const {
    auto all = entries();

    const auto found = std::find_if(all.begin(), all.end(),
                                    [&id](const QuarantineEntry& entry) { return entry.id == id; });
    if (found == all.end()) {
        return false;
    }

    const std::filesystem::path destination = found->original_path;

    std::error_code error;
    if (std::filesystem::exists(destination, error)) {
        // O dono do arquivo o recriou. Sobrescrever destruiria a versao nova,
        // que e justamente o oposto do que a quarentena existe para fazer.
        spdlog::warn("restauracao recusada: ja existe arquivo no destino");
        return false;
    }

    std::filesystem::create_directories(destination.parent_path(), error);

    std::filesystem::rename(storage_path(id), destination, error);
    if (error) {
        std::filesystem::copy_file(storage_path(id), destination,
                                   std::filesystem::copy_options::overwrite_existing, error);
        if (error) {
            spdlog::warn("nao foi possivel restaurar: {}", error.message());
            return false;
        }
        std::filesystem::remove(storage_path(id), error);
    }

    all.erase(found);
    write_entries(all);
    return true;
}

std::size_t QuarantineStore::purge_older_than(int days) const {
    const auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(24 * days);
    const auto cutoff_time = std::chrono::system_clock::to_time_t(cutoff);

    std::tm parts{};
    ::localtime_s(&parts, &cutoff_time);

    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &parts);
    const std::string cutoff_text = buffer;

    auto all = entries();
    std::vector<QuarantineEntry> kept;
    std::size_t purged = 0;

    for (auto& entry : all) {
        if (entry.quarantined_at < cutoff_text) {
            std::error_code error;
            std::filesystem::remove(storage_path(entry.id), error);
            ++purged;
            continue;
        }
        kept.push_back(std::move(entry));
    }

    if (purged > 0) {
        write_entries(kept);
    }
    return purged;
}

}
