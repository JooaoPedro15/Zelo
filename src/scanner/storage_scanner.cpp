#include "scanner/storage_scanner.hpp"

#include "scanner/volume_geometry.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <deque>
#include <optional>
#include <unordered_set>
#include <utility>

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

/// Arquivo cujo conteudo mora na nuvem.
///
/// Servicos como o OneDrive marcam esses arquivos como reparse point, entao
/// descarta-los junto com junctions os tornava invisiveis — e o usuario
/// enxergava um total menor que o do Explorador, sem explicacao.
bool is_cloud_placeholder(const WIN32_FIND_DATAW& entry) {
    constexpr DWORD kRecallOnDataAccess = 0x00400000;
    constexpr DWORD kRecallOnOpen = 0x00040000;

    return (entry.dwFileAttributes &
            (kRecallOnDataAccess | kRecallOnOpen | FILE_ATTRIBUTE_OFFLINE)) != 0;
}

std::string to_iso8601(const FILETIME& time) {
    if (time.dwLowDateTime == 0 && time.dwHighDateTime == 0) {
        return {};
    }

    SYSTEMTIME utc{};
    if (::FileTimeToSystemTime(&time, &utc) == 0) {
        return {};
    }

    std::array<char, 32> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%04u-%02u-%02uT%02u:%02u:%02u", utc.wYear,
                  utc.wMonth, utc.wDay, utc.wHour, utc.wMinute, utc.wSecond);
    return buffer.data();
}

/// O Windows costuma vir com a atualizacao de ultimo acesso desligada, porque
/// gravar a cada leitura custa caro. Sem consultar isso, a data de ultimo
/// acesso pareceria confiavel e levaria o usuario a apagar por engano.
bool last_access_is_tracked() {
    HKEY handle = nullptr;
    if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, LR"(SYSTEM\CurrentControlSet\Control\FileSystem)", 0,
                        KEY_READ, &handle) != ERROR_SUCCESS) {
        return false;
    }

    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = 0;
    const bool read = ::RegQueryValueExW(handle, L"NtfsDisableLastAccessUpdate", nullptr, &type,
                                         reinterpret_cast<LPBYTE>(&value),
                                         &size) == ERROR_SUCCESS;
    ::RegCloseKey(handle);

    if (!read) {
        return false;
    }

    // Os dois bits baixos descrevem o modo; valores pares mantem o registro
    // ligado. Na duvida, tratar como nao confiavel.
    return (value & 1U) == 0;
}

/// O espaco que o arquivo realmente ocupa.
///
/// Consulta o sistema apenas quando o arquivo e compactado ou esparso, casos em
/// que o tamanho declarado engana. Nos demais, arredondar para o cluster da o
/// mesmo resultado sem custo de chamada por arquivo — e sao centenas de
/// milhares deles numa varredura de disco.
std::uint64_t allocated_size_of(const std::filesystem::path& path, const WIN32_FIND_DATAW& entry,
                                std::uint64_t logical, std::uint32_t cluster_size) {
    const bool needs_query =
        (entry.dwFileAttributes & (FILE_ATTRIBUTE_COMPRESSED | FILE_ATTRIBUTE_SPARSE_FILE)) != 0;

    if (needs_query) {
        DWORD high = 0;
        const DWORD low = ::GetCompressedFileSizeW(path.c_str(), &high);
        if (low != INVALID_FILE_SIZE || ::GetLastError() == NO_ERROR) {
            return (static_cast<std::uint64_t>(high) << 32U) | low;
        }
    }

    return rounded_to_cluster(logical, cluster_size);
}

/// Identidade do arquivo no volume, para reconhecer o mesmo conteudo aparecendo
/// sob nomes diferentes.
///
/// Exige abrir o arquivo, entao so vale a pena acima de um tamanho: um hard
/// link de poucos kilobytes contado duas vezes nao muda decisao nenhuma, e
/// abrir cada arquivo de uma varredura inteira custaria caro demais.
std::optional<std::pair<std::uint64_t, std::uint64_t>> file_identity(
    const std::filesystem::path& path) {
    const HANDLE file =
        ::CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                      nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }

    BY_HANDLE_FILE_INFORMATION information{};
    const bool ok = ::GetFileInformationByHandle(file, &information) != 0;
    ::CloseHandle(file);

    if (!ok || information.nNumberOfLinks <= 1) {
        return std::nullopt;
    }

    return std::pair{static_cast<std::uint64_t>(information.dwVolumeSerialNumber),
                     (static_cast<std::uint64_t>(information.nFileIndexHigh) << 32U) |
                         information.nFileIndexLow};
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

    const std::uint32_t cluster_size = cluster_size_for(root);
    const bool last_access_tracked = last_access_is_tracked();

    // Identidades ja vistas, para nao somar duas vezes o mesmo conteudo quando
    // ele aparece sob mais de um nome.
    std::unordered_set<std::string> seen_identities;

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
        std::uint64_t directory_allocated = 0;
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

            const bool online_only = is_cloud_placeholder(entry);

            // Reparse point que nao seja arquivo de nuvem e link para outro
            // lugar: contar seguiria o mesmo conteudo duas vezes.
            if (is_reparse_point(entry) && !online_only) {
                continue;
            }

            const std::uint64_t bytes = file_size_of(entry);

            std::uint64_t allocated = 0;
            if (online_only) {
                // O conteudo esta na nuvem: nao ocupa espaco local, e apagar
                // isto nao libera nada.
                result.online_only_bytes += bytes;
                ++result.online_only_file_count;
            } else {
                allocated = allocated_size_of(child, entry, bytes, cluster_size);

                if (bytes >= kHardLinkCheckThreshold) {
                    if (const auto identity = file_identity(child)) {
                        const std::string key = std::to_string(identity->first) + ":" +
                                                std::to_string(identity->second);
                        if (!seen_identities.insert(key).second) {
                            ++result.hard_link_duplicates;
                            allocated = 0;
                        }
                    }
                }

                result.allocated_bytes += allocated;
            }

            result.total_bytes += bytes;
            ++result.file_count;
            directory_bytes += bytes;
            directory_allocated += allocated;
            ++directory_files;

            result.largest_files.push_back(LargeFile{
                .path = child.string(),
                .bytes = bytes,
                .allocated_bytes = allocated,
                .created_at = to_iso8601(entry.ftCreationTime),
                .modified_at = to_iso8601(entry.ftLastWriteTime),
                .last_access_at = last_access_tracked ? to_iso8601(entry.ftLastAccessTime)
                                                      : std::string{},
                .last_access_reliable = last_access_tracked,
                .cloud_state = online_only ? CloudState::OnlineOnly : CloudState::Local,
            });
        } while (::FindNextFileW(search, &entry) != 0);

        const DWORD last_error = ::GetLastError();
        ::FindClose(search);

        if (last_error != ERROR_NO_MORE_FILES) {
            ++result.skipped_count;
        }

        const bool within_depth = current.depth <= options_.rollup_depth;
        if (options_.emit_all_directories || (within_depth && directory_files > 0)) {
            result.directories.push_back(DirectoryRollup{.path = current.path.string(),
                                                         .total_bytes = directory_bytes,
                                                         .file_count = directory_files,
                                                         .allocated_bytes = directory_allocated,
                                                         .depth = current.depth});
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
