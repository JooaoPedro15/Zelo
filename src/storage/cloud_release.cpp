#include "storage/cloud_release.hpp"

#include <windows.h>

#include <deque>

namespace cleaner::storage {

namespace {

// Marcas que o servico de nuvem le. Nao estao nos cabecalhos do MinGW, entao
// aparecem aqui com os valores documentados pela Microsoft.
constexpr DWORD kAttributePinned = 0x00080000;
constexpr DWORD kAttributeUnpinned = 0x00100000;
constexpr DWORD kAttributeRecallOnOpen = 0x00040000;
constexpr DWORD kAttributeRecallOnDataAccess = 0x00400000;

bool is_online_only(DWORD attributes) {
    return (attributes & (kAttributeRecallOnOpen | kAttributeRecallOnDataAccess |
                          FILE_ATTRIBUTE_OFFLINE)) != 0;
}

std::uint64_t size_of(const WIN32_FIND_DATAW& entry) {
    return (static_cast<std::uint64_t>(entry.nFileSizeHigh) << 32) | entry.nFileSizeLow;
}

/// Percorre a pasta e chama `visit` para cada arquivo.
///
/// Nao atravessa junction nem link, pela mesma razao do scanner: seguir um deles
/// contaria o mesmo conteudo duas vezes, e um link circular nao terminaria. O
/// arquivo de nuvem tambem e reparse point, mas esse a funcao entrega — e
/// justamente o que interessa aqui.
template <typename Visit>
bool walk_files(const std::filesystem::path& root, Visit visit) {
    std::deque<std::filesystem::path> pending{root};
    bool complete = true;

    while (!pending.empty()) {
        const auto current = pending.front();
        pending.pop_front();

        WIN32_FIND_DATAW entry{};
        const HANDLE search = FindFirstFileExW((current / L"*").wstring().c_str(),
                                               FindExInfoBasic, &entry, FindExSearchNameMatch,
                                               nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (search == INVALID_HANDLE_VALUE) {
            complete = false;
            continue;
        }

        do {
            const std::wstring name = entry.cFileName;
            if (name == L"." || name == L"..") {
                continue;
            }

            const auto child = current / name;

            if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                if ((entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
                    pending.push_back(child);
                }
                continue;
            }

            visit(child, entry);
        } while (FindNextFileW(search, &entry) != 0);

        if (GetLastError() != ERROR_NO_MORE_FILES) {
            complete = false;
        }
        FindClose(search);
    }

    return complete;
}

}

CloudReleasePlan plan_cloud_release(const std::filesystem::path& root) {
    CloudReleasePlan plan;
    plan.root = root.string();

    plan.complete = walk_files(root, [&](const std::filesystem::path&,
                                         const WIN32_FIND_DATAW& entry) {
        if (is_online_only(entry.dwFileAttributes)) {
            // Ja esta so na nuvem: nao ha espaco local para liberar.
            return;
        }

        if ((entry.dwFileAttributes & kAttributePinned) != 0) {
            ++plan.pinned_kept;
            return;
        }

        ++plan.file_count;
        plan.bytes += size_of(entry);
    });

    return plan;
}

CloudReleaseOutcome release_cloud_space(const std::filesystem::path& root) {
    CloudReleaseOutcome outcome;

    walk_files(root, [&](const std::filesystem::path& file, const WIN32_FIND_DATAW& entry) {
        if (is_online_only(entry.dwFileAttributes) ||
            (entry.dwFileAttributes & kAttributePinned) != 0) {
            return;
        }

        // Tirar "sempre neste dispositivo" e por "liberar espaco". O servico de
        // nuvem le essas marcas e esvazia o conteudo local, mantendo o arquivo
        // na nuvem. Nenhuma exclusao acontece aqui.
        DWORD attributes = entry.dwFileAttributes;
        attributes &= ~kAttributePinned;
        attributes |= kAttributeUnpinned;

        if (SetFileAttributesW(file.wstring().c_str(), attributes) != 0) {
            ++outcome.released;
            outcome.bytes += size_of(entry);
        } else {
            ++outcome.failed;
        }
    });

    return outcome;
}

}
