#include "collectors/temporary_files_collector.hpp"

#include "collectors/detail/text.hpp"

#include <scanner/storage_scanner.hpp>

#include <algorithm>
#include <array>
#include <system_error>
#include <utility>

#include <windows.h>

namespace zelo::collectors {

namespace {

std::filesystem::path environment_path(const wchar_t* name) {
    std::array<wchar_t, 32767> buffer{};

    const DWORD written = ::GetEnvironmentVariableW(name, buffer.data(),
                                                    static_cast<DWORD>(buffer.size()));
    if (written == 0 || written >= buffer.size()) {
        return {};
    }
    return std::filesystem::path(std::wstring(buffer.data(), written));
}

}

TemporaryFilesCollector::TemporaryFilesCollector(core::ProtectedPaths protected_paths)
    : protected_paths_(std::move(protected_paths)) {}

std::vector<std::filesystem::path> TemporaryFilesCollector::folders() const {
    std::array<wchar_t, MAX_PATH + 1> windows_directory{};
    const UINT written = ::GetWindowsDirectoryW(windows_directory.data(),
                                                static_cast<UINT>(windows_directory.size()));

    std::vector<std::filesystem::path> candidates{environment_path(L"TEMP")};
    if (written > 0 && written < windows_directory.size()) {
        candidates.emplace_back(std::filesystem::path(std::wstring(windows_directory.data(), written)) /
                                "Temp");
    }

    std::vector<std::filesystem::path> folders;
    for (const auto& candidate : candidates) {
        if (candidate.empty()) {
            continue;
        }

        std::error_code error;
        if (!std::filesystem::is_directory(candidate, error)) {
            continue;
        }

        // A deny-list decide o que pode ser alvo de limpeza. Uma pasta
        // protegida nao entra na conta de espaco recuperavel, senao o app
        // prometeria liberar algo que ele proprio nunca vai tocar.
        if (protected_paths_.is_protected(candidate.string())) {
            continue;
        }

        const bool already_listed =
            std::any_of(folders.begin(), folders.end(),
                        [&candidate](const std::filesystem::path& listed) {
                            return detail::find_ignoring_case(listed.string(), candidate.string()) == 0 &&
                                   listed.string().size() == candidate.string().size();
                        });
        if (!already_listed) {
            folders.push_back(candidate);
        }
    }

    return folders;
}

bool TemporaryFilesCollector::collect_into(core::SystemSnapshot& snapshot,
                                           std::stop_token token) const {
    const scanner::StorageScanner storage_scanner;

    core::TemporaryFilesInfo info;
    bool scanned_anything = false;

    for (const auto& folder : folders()) {
        const scanner::ScanResult result = storage_scanner.scan(folder, token);
        if (!result.completed) {
            // Varredura interrompida: informar um total parcial faria o usuario
            // decidir com base num numero menor que a realidade.
            return false;
        }

        info.total_bytes += result.total_bytes;
        info.file_count += result.file_count;
        scanned_anything = true;
    }

    if (!scanned_anything) {
        return false;
    }

    info.available = true;
    snapshot.temporary_files = info;
    return true;
}

}
