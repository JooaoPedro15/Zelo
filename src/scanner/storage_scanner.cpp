#include "scanner/storage_scanner.hpp"

#include <algorithm>
#include <deque>

#include <windows.h>

namespace zelo::scanner {

namespace {

/// O Win32 so aceita caminhos acima de MAX_PATH com este prefixo. Aplicado
/// sempre, para que nao exista um limite silencioso na varredura.
std::wstring with_long_path_prefix(const std::filesystem::path& path) {
    std::wstring native = path.native();
    std::replace(native.begin(), native.end(), L'/', L'\\');

    if (native.starts_with(LR"(\\?\)") || native.starts_with(LR"(\\.\)")) {
        return native;
    }
    if (native.starts_with(LR"(\\)")) {
        return LR"(\\?\UNC\)" + native.substr(2);
    }
    return LR"(\\?\)" + native;
}

std::uint64_t file_size_of(const WIN32_FIND_DATAW& entry) {
    return (static_cast<std::uint64_t>(entry.nFileSizeHigh) << 32U) | entry.nFileSizeLow;
}

bool is_dot_entry(const wchar_t* name) {
    return std::wcscmp(name, L".") == 0 || std::wcscmp(name, L"..") == 0;
}

/// Junctions e links criam duas rotas para o mesmo conteudo. Seguir um deles
/// contaria os mesmos bytes duas vezes e, se o link apontar para um ancestral,
/// a varredura nunca terminaria.
bool is_reparse_point(const WIN32_FIND_DATAW& entry) {
    return (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

struct PendingDirectory {
    std::filesystem::path path;
    std::size_t depth;
};

}

StorageScanner::StorageScanner(ScanOptions options) : options_(options) {}

ScanResult StorageScanner::scan(const std::filesystem::path& root) const {
    return scan(root, std::stop_token{});
}

ScanResult StorageScanner::scan(const std::filesystem::path& root, std::stop_token token) const {
    ScanResult result;

    std::deque<PendingDirectory> pending;
    pending.push_back({root, 0});

    while (!pending.empty()) {
        if (token.stop_requested()) {
            return result;
        }

        const PendingDirectory current = pending.front();
        pending.pop_front();

        const std::wstring pattern = with_long_path_prefix(current.path) + L"\\*";

        WIN32_FIND_DATAW entry{};
        // FindExInfoBasic dispensa o nome 8.3 e FIND_FIRST_EX_LARGE_FETCH busca
        // em blocos maiores: juntos reduzem bastante o custo em arvores grandes.
        const HANDLE search = ::FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &entry,
                                                 FindExSearchNameMatch, nullptr,
                                                 FIND_FIRST_EX_LARGE_FETCH);
        if (search == INVALID_HANDLE_VALUE) {
            // Permissao negada, diretorio removido no meio do caminho ou volume
            // desconectado. Contabiliza e segue: uma pasta ilegivel nao pode
            // derrubar a varredura inteira.
            ++result.skipped_count;
            continue;
        }

        std::uint64_t directory_bytes = 0;
        std::size_t directory_files = 0;

        do {
            if (token.stop_requested()) {
                ::FindClose(search);
                return result;
            }

            if (is_dot_entry(entry.cFileName)) {
                continue;
            }

            const std::filesystem::path child = current.path / entry.cFileName;

            if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                if (is_reparse_point(entry)) {
                    continue;
                }
                ++result.directory_count;
                pending.push_back({child, current.depth + 1});
                continue;
            }

            if (is_reparse_point(entry)) {
                continue;
            }

            const std::uint64_t bytes = file_size_of(entry);
            result.total_bytes += bytes;
            ++result.file_count;
            directory_bytes += bytes;
            ++directory_files;

            result.largest_files.push_back(LargeFile{child.string(), bytes});
        } while (::FindNextFileW(search, &entry) != 0);

        const DWORD last_error = ::GetLastError();
        ::FindClose(search);

        if (last_error != ERROR_NO_MORE_FILES) {
            ++result.skipped_count;
        }

        if (current.depth <= options_.rollup_depth && directory_files > 0) {
            result.directories.push_back(
                DirectoryRollup{current.path.string(), directory_bytes, directory_files});
        }

        // Poda continua em vez de ordenar no fim: manter todos os arquivos de um
        // disco cheio em memoria nao e viavel.
        if (result.largest_files.size() > options_.largest_files_kept * 2) {
            std::partial_sort(result.largest_files.begin(),
                              result.largest_files.begin() +
                                  static_cast<std::ptrdiff_t>(options_.largest_files_kept),
                              result.largest_files.end(),
                              [](const LargeFile& left, const LargeFile& right) {
                                  return left.bytes > right.bytes;
                              });
            result.largest_files.resize(options_.largest_files_kept);
        }
    }

    std::sort(result.largest_files.begin(), result.largest_files.end(),
              [](const LargeFile& left, const LargeFile& right) { return left.bytes > right.bytes; });
    if (result.largest_files.size() > options_.largest_files_kept) {
        result.largest_files.resize(options_.largest_files_kept);
    }

    std::sort(result.directories.begin(), result.directories.end(),
              [](const DirectoryRollup& left, const DirectoryRollup& right) {
                  return left.total_bytes > right.total_bytes;
              });

    result.completed = true;
    return result;
}

}
