#include "cleaners/cleaner_engine.hpp"

#include <spdlog/spdlog.h>

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <deque>
#include <filesystem>

namespace cleaner::cleaners {

namespace {

bool same_name(const std::string& left, const std::string& right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (std::tolower(static_cast<unsigned char>(left[index])) !=
            std::tolower(static_cast<unsigned char>(right[index]))) {
            return false;
        }
    }
    return true;
}

bool is_preserved(const core::CleanerSpec& spec, const std::filesystem::path& path) {
    const auto name = path.filename().string();
    return std::any_of(spec.preserved_names.begin(), spec.preserved_names.end(),
                       [&name](const std::string& kept) { return same_name(kept, name); });
}

/// O caminho final que o Windows enxerga, com link ja resolvido.
///
/// Existe para a checagem de area valer sobre o destino, nao sobre o atalho: um
/// link dentro da pasta permitida pode apontar para fora dela, e apagar
/// seguindo o link removeria conteudo que o limpador nunca teve permissao de
/// tocar.
std::wstring final_path_of(const std::filesystem::path& path) {
    const HANDLE handle =
        CreateFileW(path.wstring().c_str(), FILE_READ_ATTRIBUTES,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return {};
    }

    std::array<wchar_t, 32768> buffer{};
    const DWORD written = GetFinalPathNameByHandleW(handle, buffer.data(),
                                                    static_cast<DWORD>(buffer.size()), VOLUME_NAME_DOS);
    CloseHandle(handle);

    if (written == 0 || written >= buffer.size()) {
        return {};
    }

    std::wstring resolved(buffer.data(), written);

    // GetFinalPathNameByHandleW devolve com o prefixo de caminho longo. Tirar
    // deixa a comparacao com a raiz declarada direta.
    constexpr std::wstring_view kPrefix = LR"(\\?\)";
    if (resolved.rfind(kPrefix, 0) == 0) {
        resolved.erase(0, kPrefix.size());
    }
    return resolved;
}

std::wstring fold(std::wstring text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](wchar_t letter) { return static_cast<wchar_t>(::towupper(letter)); });
    while (!text.empty() && (text.back() == L'\\' || text.back() == L'/')) {
        text.pop_back();
    }
    return text;
}

/// As raizes declaradas, resolvidas do mesmo jeito que os arquivos serao.
///
/// Precisa ser o mesmo jeito nos dois lados. A raiz pode chegar com nome curto
/// 8.3 — "%TEMP%" costuma expandir para "C:\Users\JOO~1\..." em perfil com
/// acento — enquanto o caminho final do arquivo vem com o nome longo. Comparar
/// as duas formas nunca casa, e um limpador que nunca casa recusa tudo em
/// silencio.
///
/// Raiz que nao puder ser resolvida fica de fora: nao dar para conferir a area
/// e motivo para nao apagar, nao para apagar assim mesmo.
std::vector<std::wstring> resolved_roots_of(const core::CleanerSpec& spec) {
    std::vector<std::wstring> roots;

    for (const auto& root : spec.allowed_roots) {
        if (auto resolved = final_path_of(root); !resolved.empty()) {
            roots.push_back(fold(std::move(resolved)));
        }
    }
    return roots;
}

/// O caminho esta dentro de alguma raiz permitida?
///
/// A comparacao respeita limite de componente: sem isso,
/// "C:\cache_do_vizinho" passaria por dentro de "C:\cache".
bool inside_allowed_root(const std::vector<std::wstring>& roots, const std::wstring& resolved) {
    const auto candidate = fold(resolved);
    if (candidate.empty()) {
        return false;
    }

    return std::any_of(roots.begin(), roots.end(), [&candidate](const std::wstring& root) {
        if (root.empty() || candidate.size() < root.size()) {
            return false;
        }
        if (candidate.compare(0, root.size(), root) != 0) {
            return false;
        }
        return candidate.size() == root.size() || candidate[root.size()] == L'\\';
    });
}

std::uint64_t size_of(const WIN32_FIND_DATAW& entry) {
    return (static_cast<std::uint64_t>(entry.nFileSizeHigh) << 32) | entry.nFileSizeLow;
}

std::uint64_t free_bytes_of(const std::filesystem::path& path) {
    ULARGE_INTEGER caller{};
    ULARGE_INTEGER total{};
    ULARGE_INTEGER free_total{};

    const auto volume = path.root_path();
    if (GetDiskFreeSpaceExW(volume.wstring().c_str(), &caller, &total, &free_total) == 0) {
        return 0;
    }
    return free_total.QuadPart;
}

/// Percorre uma raiz chamando `visit` para cada arquivo que sobrevive aos
/// filtros da declaracao.
template <typename Visit>
bool walk(const core::CleanerSpec& spec, const std::filesystem::path& root,
          const core::ProtectedPaths& protected_paths, std::stop_token token,
          std::vector<std::string>& rejected, Visit visit) {
    std::deque<std::filesystem::path> pending{root};
    bool complete = true;

    while (!pending.empty()) {
        if (token.stop_requested()) {
            return false;
        }

        const auto current = pending.front();
        pending.pop_front();

        WIN32_FIND_DATAW entry{};
        const HANDLE search =
            FindFirstFileExW((current / L"*").wstring().c_str(), FindExInfoBasic, &entry,
                             FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (search == INVALID_HANDLE_VALUE) {
            complete = false;
            if (rejected.size() < 20) {
                rejected.push_back(current.string() + ": nao foi possivel abrir");
            }
            continue;
        }

        do {
            const std::wstring name = entry.cFileName;
            if (name == L"." || name == L"..") {
                continue;
            }

            const auto child = current / name;

            if (is_preserved(spec, child)) {
                continue;
            }

            if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                // Link nao e atravessado: seguir um deles apagaria conteudo de
                // fora da area permitida sob um nome de dentro dela.
                if ((entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
                    pending.push_back(child);
                }
                continue;
            }

            if (protected_paths.is_protected(child.string())) {
                if (rejected.size() < 20) {
                    rejected.push_back(child.string() + ": area protegida");
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

CleanerEngine::CleanerEngine(core::ProtectedPaths protected_paths)
    : protected_paths_(std::move(protected_paths)) {}

core::CleanerPreview CleanerEngine::preview(const core::CleanerSpec& spec,
                                            std::stop_token token) const {
    core::CleanerPreview preview;
    preview.cleaner_id = spec.id;
    preview.complete = true;

    for (const auto& root : spec.allowed_roots) {
        std::error_code error;
        if (!std::filesystem::is_directory(root, error)) {
            continue;
        }

        // A raiz declarada tambem passa pela deny-list. A declaracao e revisada,
        // mas quem decide e sempre a deny-list.
        if (protected_paths_.is_protected(root)) {
            preview.rejected.push_back(root + ": area protegida");
            preview.complete = false;
            continue;
        }

        const bool finished =
            walk(spec, root, protected_paths_, token, preview.rejected,
                 [&preview](const std::filesystem::path&, const WIN32_FIND_DATAW& entry) {
                     ++preview.file_count;
                     preview.bytes += size_of(entry);
                 });

        preview.complete = preview.complete && finished;
    }

    return preview;
}

core::CleanerOutcome CleanerEngine::execute(const core::CleanerSpec& spec,
                                            std::stop_token token) const {
    core::CleanerOutcome outcome;
    outcome.cleaner_id = spec.id;

    if (spec.allowed_roots.empty()) {
        return outcome;
    }

    const std::filesystem::path first_root(spec.allowed_roots.front());
    const auto free_before = free_bytes_of(first_root);

    const auto allowed = resolved_roots_of(spec);
    if (allowed.empty()) {
        return outcome;
    }

    std::size_t denied = 0;
    std::size_t in_use = 0;
    std::size_t outside = 0;

    for (const auto& root : spec.allowed_roots) {
        std::error_code error;
        if (!std::filesystem::is_directory(root, error) || protected_paths_.is_protected(root)) {
            continue;
        }

        std::vector<std::string> ignored;
        walk(spec, root, protected_paths_, token, ignored,
             [&](const std::filesystem::path& file, const WIN32_FIND_DATAW& entry) {
                 // Ultima conferencia, com o disco no estado de agora. O arquivo
                 // pode ter virado link para fora da area entre a medicao e este
                 // instante, e e nesse intervalo que um limpador apaga o que
                 // nunca teve permissao de tocar.
                 const auto resolved = final_path_of(file);
                 if (resolved.empty() || !inside_allowed_root(allowed, resolved)) {
                     ++outside;
                     ++outcome.skipped_count;
                     return;
                 }

                 if (protected_paths_.is_protected(file.string())) {
                     ++outcome.skipped_count;
                     return;
                 }

                 const auto bytes = size_of(entry);

                 if (DeleteFileW(file.wstring().c_str()) != 0) {
                     ++outcome.removed_count;
                     outcome.bytes += bytes;
                     return;
                 }

                 ++outcome.failed_count;
                 switch (GetLastError()) {
                 case ERROR_ACCESS_DENIED:
                     ++denied;
                     break;
                 case ERROR_SHARING_VIOLATION:
                 case ERROR_LOCK_VIOLATION:
                     ++in_use;
                     break;
                 default:
                     break;
                 }
             });
    }

    if (in_use > 0) {
        outcome.reasons.push_back(std::to_string(in_use) +
                                  " arquivos estavam em uso por algum programa aberto");
    }
    if (denied > 0) {
        outcome.reasons.push_back(std::to_string(denied) + " arquivos nao puderam ser acessados");
    }
    if (outside > 0) {
        outcome.reasons.push_back(
            std::to_string(outside) +
            " caminhos apontavam para fora da area permitida e foram deixados de lado");
    }

    const auto free_after = free_bytes_of(first_root);
    outcome.free_space_delta =
        static_cast<std::int64_t>(free_after) - static_cast<std::int64_t>(free_before);

    spdlog::info("limpador {} removeu {} arquivos, {} bytes, {} ignorados", spec.id,
                 outcome.removed_count, outcome.bytes, outcome.skipped_count);

    return outcome;
}

}
