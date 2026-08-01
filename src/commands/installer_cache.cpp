#include "commands/installer_cache.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <set>

namespace cleaner::commands {

namespace {

// Contextos e estados da API do Installer. Valores documentados pela Microsoft.
// Ficam aqui porque as funcoes sao carregadas em tempo de execucao, e nesse
// caminho os cabecalhos nao participam.
constexpr DWORD kContextAll = 7;
constexpr DWORD kPatchStateAll = 15;

/// SID que a API entende como "todos os usuarios da maquina".
constexpr const wchar_t* kAllUsers = L"s-1-1-0";

using MsiEnumProductsExW_t = UINT(WINAPI*)(LPCWSTR, LPCWSTR, DWORD, DWORD, WCHAR*, DWORD*, LPWSTR,
                                           LPDWORD);
using MsiGetProductInfoExW_t = UINT(WINAPI*)(LPCWSTR, LPCWSTR, DWORD, LPCWSTR, LPWSTR, LPDWORD);
using MsiEnumPatchesExW_t = UINT(WINAPI*)(LPCWSTR, LPCWSTR, DWORD, DWORD, DWORD, WCHAR*, WCHAR*,
                                          DWORD*, LPWSTR, LPDWORD);
using MsiGetPatchInfoExW_t = UINT(WINAPI*)(LPCWSTR, LPCWSTR, LPCWSTR, DWORD, LPCWSTR, LPWSTR,
                                           LPDWORD);

struct MsiApi {
    HMODULE module = nullptr;
    MsiEnumProductsExW_t enum_products = nullptr;
    MsiGetProductInfoExW_t product_info = nullptr;
    MsiEnumPatchesExW_t enum_patches = nullptr;
    MsiGetPatchInfoExW_t patch_info = nullptr;

    [[nodiscard]] bool complete() const {
        return enum_products != nullptr && product_info != nullptr && enum_patches != nullptr &&
               patch_info != nullptr;
    }
};

/// Carrega a API em tempo de execucao.
///
/// Ligar em tempo de compilacao tornaria a ausencia da biblioteca um erro de
/// carregamento do programa inteiro. Aqui a ausencia vira um relatorio que diz
/// "nao consegui perguntar", que e a resposta honesta.
MsiApi load_msi() {
    MsiApi api;
    api.module = LoadLibraryW(L"msi.dll");
    if (api.module == nullptr) {
        return api;
    }

    api.enum_products =
        reinterpret_cast<MsiEnumProductsExW_t>(
            reinterpret_cast<void*>(GetProcAddress(api.module, "MsiEnumProductsExW")));
    api.product_info =
        reinterpret_cast<MsiGetProductInfoExW_t>(
            reinterpret_cast<void*>(GetProcAddress(api.module, "MsiGetProductInfoExW")));
    api.enum_patches =
        reinterpret_cast<MsiEnumPatchesExW_t>(
            reinterpret_cast<void*>(GetProcAddress(api.module, "MsiEnumPatchesExW")));
    api.patch_info =
        reinterpret_cast<MsiGetPatchInfoExW_t>(
            reinterpret_cast<void*>(GetProcAddress(api.module, "MsiGetPatchInfoExW")));

    return api;
}

/// UTF-8 de volta para o caminho nativo.
///
/// O caminho sai do relatorio como texto e volta para o Windows como chamada de
/// arquivo. Converter na mao, declarando UTF-8, evita a classe de erro que ja
/// apareceu neste projeto varias vezes: caminho com acento virando bytes
/// invalidos no meio do trajeto.
std::wstring widen(const std::string& text) {
    if (text.empty()) {
        return {};
    }

    const int length =
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(),
                        length);
    return wide;
}

std::wstring fold(std::wstring text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](wchar_t letter) { return static_cast<wchar_t>(::towupper(letter)); });
    return text;
}

/// O caminho completo vira so o nome do arquivo, em maiusculas.
///
/// A API devolve o caminho como o Windows o gravou, que nem sempre casa letra a
/// letra com o que a pasta lista. O nome do arquivo e o unico identificador
/// estavel dos dois lados.
std::wstring cache_key(const std::wstring& path) {
    const auto slash = path.find_last_of(L"\\/");
    return fold(slash == std::wstring::npos ? path : path.substr(slash + 1));
}

std::wstring local_package_of_product(const MsiApi& api, const wchar_t* product,
                                      const wchar_t* sid, DWORD context) {
    std::array<wchar_t, MAX_PATH> buffer{};
    DWORD size = static_cast<DWORD>(buffer.size());

    if (api.product_info(product, sid, context, L"LocalPackage", buffer.data(), &size) !=
        ERROR_SUCCESS) {
        return {};
    }
    return std::wstring(buffer.data(), size);
}

void collect_patches(const MsiApi& api, const wchar_t* product, const wchar_t* sid, DWORD context,
                     std::set<std::wstring>& referenced) {
    for (DWORD index = 0;; ++index) {
        std::array<wchar_t, 39> patch{};
        std::array<wchar_t, 39> target{};
        std::array<wchar_t, 256> target_sid{};
        DWORD target_sid_size = static_cast<DWORD>(target_sid.size());
        DWORD target_context = 0;

        const UINT status =
            api.enum_patches(product, sid, context, kPatchStateAll, index, patch.data(),
                             target.data(), &target_context, target_sid.data(), &target_sid_size);
        if (status != ERROR_SUCCESS) {
            break;
        }

        std::array<wchar_t, MAX_PATH> buffer{};
        DWORD size = static_cast<DWORD>(buffer.size());
        if (api.patch_info(patch.data(), product, sid, context, L"LocalPackage", buffer.data(),
                           &size) == ERROR_SUCCESS) {
            referenced.insert(cache_key(std::wstring(buffer.data(), size)));
        }
    }
}

/// Todos os instaladores que algum programa instalado ainda reivindica.
std::set<std::wstring> referenced_packages(const MsiApi& api, bool& ok, UINT& last_status) {
    std::set<std::wstring> referenced;
    ok = false;
    last_status = ERROR_SUCCESS;

    for (DWORD index = 0;; ++index) {
        std::array<wchar_t, 39> product{};
        std::array<wchar_t, 256> sid{};
        DWORD sid_size = static_cast<DWORD>(sid.size());
        DWORD context = 0;

        const UINT status = api.enum_products(nullptr, kAllUsers, kContextAll, index,
                                              product.data(), &context, sid.data(), &sid_size);
        if (status == ERROR_NO_MORE_ITEMS) {
            // Chegou ao fim da lista: a pergunta foi respondida por completo.
            ok = true;
            break;
        }
        if (status != ERROR_SUCCESS) {
            // Parou no meio. Metade da lista de donos produziria orfaos falsos,
            // entao o resultado inteiro e descartado.
            last_status = status;
            return {};
        }

        const wchar_t* product_sid = sid_size > 0 ? sid.data() : nullptr;

        if (const auto package = local_package_of_product(api, product.data(), product_sid, context);
            !package.empty()) {
            referenced.insert(cache_key(package));
        }

        collect_patches(api, product.data(), product_sid, context, referenced);
    }

    return referenced;
}

std::filesystem::path installer_folder() {
    std::array<wchar_t, MAX_PATH> buffer{};
    const UINT written = GetWindowsDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
    if (written == 0 || written >= buffer.size()) {
        return {};
    }
    return std::filesystem::path(std::wstring(buffer.data(), written)) / L"Installer";
}

bool is_package(const std::filesystem::path& file) {
    const auto extension = fold(file.extension().wstring());
    return extension == L".MSI" || extension == L".MSP";
}

}

InstallerCacheReport scan_installer_cache() {
    InstallerCacheReport report;

    const MsiApi api = load_msi();
    if (!api.complete()) {
        report.obstacle = "A biblioteca do Windows Installer nao respondeu neste computador.";
        if (api.module != nullptr) {
            FreeLibrary(api.module);
        }
        return report;
    }

    bool ok = false;
    UINT status = ERROR_SUCCESS;
    const auto referenced = referenced_packages(api, ok, status);
    FreeLibrary(api.module);

    if (!ok) {
        // Perguntar quem e dono dos instaladores da maquina inteira exige
        // privilegio. Sem ele a resposta vem pela metade — e meia lista de
        // donos transforma instalador em uso em sobra.
        report.obstacle =
            status == ERROR_ACCESS_DENIED
                ? "Esta conferencia precisa do Cleaner aberto como administrador. Sem isso o "
                  "Windows so conta parte dos programas instalados, e um instalador em uso "
                  "pareceria sobra."
                : "A lista de programas instalados nao pode ser lida por completo (codigo " +
                      std::to_string(status) +
                      "). Sem ela, um instalador em uso pareceria sobra, entao nada e apontado.";
        return report;
    }

    report.products_readable = true;

    const auto folder = installer_folder();
    if (folder.empty()) {
        report.products_readable = false;
        report.obstacle = "A pasta de instaladores do Windows nao foi encontrada.";
        return report;
    }

    std::error_code error;

    // Somente os arquivos soltos na pasta. As subpastas guardam icones e
    // componentes que nao seguem esta regra, e entrar nelas seria julgar o que
    // esta consulta nao sabe julgar.
    for (const auto& entry : std::filesystem::directory_iterator(folder, error)) {
        if (!entry.is_regular_file(error) || !is_package(entry.path())) {
            continue;
        }

        const auto bytes = static_cast<std::uint64_t>(entry.file_size(error));
        if (error) {
            continue;
        }

        if (referenced.count(fold(entry.path().filename().wstring())) > 0) {
            ++report.referenced_count;
            report.referenced_bytes += bytes;
            continue;
        }

        report.orphans.push_back(OrphanInstaller{.path = entry.path().string(), .bytes = bytes});
        report.orphan_bytes += bytes;
    }

    // Maiores primeiro: e a ordem em que o usuario decide.
    std::sort(report.orphans.begin(), report.orphans.end(),
              [](const OrphanInstaller& left, const OrphanInstaller& right) {
                  return left.bytes > right.bytes;
              });

    return report;
}

InstallerCleanupOutcome remove_orphan_installers(const std::vector<OrphanInstaller>& orphans) {
    InstallerCleanupOutcome outcome;

    for (const auto& orphan : orphans) {
        if (DeleteFileW(widen(orphan.path).c_str()) != 0) {
            ++outcome.removed;
            outcome.bytes += orphan.bytes;
            continue;
        }

        ++outcome.failed;
        if (GetLastError() == ERROR_ACCESS_DENIED) {
            outcome.needs_elevation = true;
        }
    }

    return outcome;
}

}
