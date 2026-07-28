#include "storage/history_store.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <system_error>
#include <utility>

#include <windows.h>

namespace zelo::storage {

namespace {

constexpr const char* kExtension = ".json";

std::wstring to_wide(const std::filesystem::path& path) {
    return path.native();
}

bool replace_atomically(const std::filesystem::path& temporary,
                        const std::filesystem::path& target) {
    // MoveFileEx com REPLACE_EXISTING troca o arquivo de uma vez so. Sem isso,
    // uma queda no meio da gravacao deixaria o historico truncado.
    return ::MoveFileExW(to_wide(temporary).c_str(), to_wide(target).c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

std::vector<std::filesystem::path> session_files(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> files;

    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (entry.is_regular_file(error) && entry.path().extension() == kExtension) {
            files.push_back(entry.path());
        }
    }

    // O nome comeca com a data, entao ordem alfabetica decrescente ja e a mais
    // recente primeiro.
    std::sort(files.begin(), files.end(), std::greater<>());
    return files;
}

std::optional<StoredSession> read_session(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }

    try {
        return session_from_json(nlohmann::json::parse(input));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}

std::filesystem::path default_data_directory() {
    std::array<wchar_t, 32767> buffer{};

    const DWORD written = ::GetEnvironmentVariableW(L"LOCALAPPDATA", buffer.data(),
                                                    static_cast<DWORD>(buffer.size()));
    if (written == 0 || written >= buffer.size()) {
        return std::filesystem::temp_directory_path() / "Zelo";
    }
    return std::filesystem::path(std::wstring(buffer.data(), written)) / "Zelo";
}

HistoryStore::HistoryStore(std::filesystem::path directory) : directory_(std::move(directory)) {
    std::error_code error;
    std::filesystem::create_directories(directory_, error);
}

const std::filesystem::path& HistoryStore::directory() const {
    return directory_;
}

void HistoryStore::save(const StoredSession& session) const {
    const std::filesystem::path target = directory_ / (session.id + kExtension);
    const std::filesystem::path temporary = directory_ / (session.id + ".tmp");

    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << to_json(session).dump(2);
        output.flush();
    }

    if (!replace_atomically(temporary, target)) {
        std::error_code error;
        std::filesystem::remove(temporary, error);
    }
}

std::vector<StoredSession> HistoryStore::load_all() const {
    std::vector<StoredSession> sessions;

    for (const auto& file : session_files(directory_)) {
        if (auto session = read_session(file)) {
            sessions.push_back(std::move(*session));
        }
    }

    return sessions;
}

std::optional<StoredSession> HistoryStore::load(const std::string& id) const {
    return read_session(directory_ / (id + kExtension));
}

std::size_t HistoryStore::quarantine_unreadable() const {
    std::size_t quarantined = 0;

    for (const auto& file : session_files(directory_)) {
        if (read_session(file)) {
            continue;
        }

        std::error_code error;
        std::filesystem::rename(file, std::filesystem::path(file).replace_extension(".corrupt"),
                                error);
        if (!error) {
            ++quarantined;
        }
    }

    return quarantined;
}

void HistoryStore::apply_retention(std::size_t keep) const {
    const auto files = session_files(directory_);
    if (files.size() <= keep) {
        return;
    }

    std::error_code error;
    for (std::size_t index = keep; index < files.size(); ++index) {
        std::filesystem::remove(files.at(index), error);
    }
}

}
