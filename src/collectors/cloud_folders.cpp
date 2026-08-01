#include "collectors/cloud_folders.hpp"

#include <scanner/storage_scanner.hpp>

#include <windows.h>

#include <array>
#include <algorithm>

namespace zelo::collectors {

namespace {

std::string to_utf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }

    const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), length,
                        nullptr, nullptr);
    return utf8;
}

std::wstring read_string_value(HKEY key, const wchar_t* name) {
    DWORD type = 0;
    DWORD size = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS ||
        type != REG_SZ || size == 0) {
        return {};
    }

    std::wstring value(size / sizeof(wchar_t), L'\0');
    if (RegQueryValueExW(key, name, nullptr, nullptr, reinterpret_cast<LPBYTE>(value.data()),
                         &size) != ERROR_SUCCESS) {
        return {};
    }

    // O registro inclui o terminador na contagem; a string do C++ nao deve.
    while (!value.empty() && value.back() == L'\0') {
        value.pop_back();
    }
    return value;
}

/// Compara caminhos ignorando maiusculas, como o proprio Windows faz.
///
/// A dobra usa a tabela do sistema, e nao ASCII: um perfil chamado "João" e a
/// razao pela qual isso importa.
std::wstring fold(std::wstring text) {
    if (text.empty()) {
        return text;
    }

    const int folded = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, text.data(),
                                     static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr,
                                     0);
    if (folded <= 0) {
        return text;
    }

    std::wstring result(static_cast<std::size_t>(folded), L'\0');
    LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, text.data(),
                  static_cast<int>(text.size()), result.data(), folded, nullptr, nullptr, 0);

    while (!result.empty() && (result.back() == L'\\' || result.back() == L'/')) {
        result.pop_back();
    }
    return result;
}

}

std::vector<CloudFolder> cloud_folders() {
    std::vector<CloudFolder> folders;

    HKEY accounts = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\OneDrive\\Accounts", 0,
                      KEY_READ, &accounts) != ERROR_SUCCESS) {
        return folders;
    }

    for (DWORD index = 0;; ++index) {
        std::array<wchar_t, 256> name{};
        DWORD name_length = static_cast<DWORD>(name.size());
        if (RegEnumKeyExW(accounts, index, name.data(), &name_length, nullptr, nullptr, nullptr,
                          nullptr) != ERROR_SUCCESS) {
            break;
        }

        HKEY account = nullptr;
        if (RegOpenKeyExW(accounts, name.data(), 0, KEY_READ, &account) != ERROR_SUCCESS) {
            continue;
        }

        const auto folder = read_string_value(account, L"UserFolder");
        RegCloseKey(account);

        // Conta configurada sem pasta local existe: e a que foi adicionada e
        // nunca sincronizada. Ela nao ocupa disco e nao entra na lista.
        if (folder.empty()) {
            continue;
        }

        folders.push_back(CloudFolder{
            .path = to_utf8(folder),
            .service = "OneDrive",
            .account = to_utf8(std::wstring(name.data(), name_length)),
        });
    }

    RegCloseKey(accounts);
    return folders;
}

CloudSpace measure_cloud_folder(const std::filesystem::path& root) {
    CloudSpace space;
    space.path = root.string();

    // Guardar todo arquivo grande seria desperdicio: aqui so interessam os dois
    // totais, e a separacao entre local e nuvem ja vem do scanner.
    const scanner::StorageScanner scanner{scanner::ScanOptions{.largest_files_kept = 0,
                                                               .rollup_depth = 0}};
    const auto result = scanner.scan(root);

    space.complete = result.completed;
    space.online_only_bytes = result.online_only_bytes;
    space.online_only_files = result.online_only_file_count;

    // O total alocado do scanner ja exclui o que mora so na nuvem: aquilo nao
    // ocupa cluster nenhum aqui.
    space.local_bytes = result.allocated_bytes;
    space.local_files = result.file_count - result.online_only_file_count;

    return space;
}

bool is_inside_cloud_folder(const std::filesystem::path& path,
                            const std::vector<CloudFolder>& folders) {
    const auto candidate = fold(path.wstring());

    return std::any_of(folders.begin(), folders.end(), [&](const CloudFolder& folder) {
        const auto root = fold(std::filesystem::path(folder.path).wstring());
        if (root.empty() || candidate.size() < root.size()) {
            return false;
        }

        if (candidate.compare(0, root.size(), root) != 0) {
            return false;
        }

        // Igual a raiz, ou logo abaixo dela. Sem esta checagem,
        // "C:\Users\Joao\OneDriveAntigo" passaria por dentro do OneDrive.
        return candidate.size() == root.size() || candidate[root.size()] == L'\\';
    });
}

}
