#include "collectors/startup_collector.hpp"

#include "collectors/detail/text.hpp"

#include <commands/startup_control.hpp>

#include <array>
#include <filesystem>
#include <string_view>

#include <windows.h>

namespace cleaner::collectors {

namespace {

using detail::contains_ignoring_case;
using detail::to_utf8;

struct RunKey {
    HKEY hive;
    const wchar_t* subkey;
    REGSAM view;
    core::StartupOrigin origin;
};

/// Ambas as visoes do registro: um programa de 32 bits instalado num Windows de
/// 64 bits escreve na visao redirecionada, e some se so a nativa for lida.
///
/// Funcao em vez de constante: as macros de hive do Windows sao casts de
/// inteiro para ponteiro e nao sobrevivem a avaliacao em tempo de compilacao.
std::array<RunKey, 4> run_keys() {
    constexpr const wchar_t* kRunPath = LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Run)";

    return {
        RunKey{HKEY_LOCAL_MACHINE, kRunPath, KEY_WOW64_64KEY, core::StartupOrigin::MachineRun},
        RunKey{HKEY_LOCAL_MACHINE, kRunPath, KEY_WOW64_32KEY, core::StartupOrigin::MachineRun32},
        RunKey{HKEY_CURRENT_USER, kRunPath, KEY_WOW64_64KEY, core::StartupOrigin::UserRun},
        RunKey{HKEY_CURRENT_USER, kRunPath, KEY_WOW64_32KEY, core::StartupOrigin::UserRun32},
    };
}

void read_run_key(const RunKey& key, std::vector<core::StartupItemInfo>& items) {
    HKEY handle = nullptr;
    if (::RegOpenKeyExW(key.hive, key.subkey, 0, KEY_READ | key.view, &handle) != ERROR_SUCCESS) {
        return;
    }

    for (DWORD index = 0;; ++index) {
        std::array<wchar_t, 16384> name{};
        std::array<BYTE, 8192> data{};
        auto name_size = static_cast<DWORD>(name.size());
        auto data_size = static_cast<DWORD>(data.size());
        DWORD type = 0;

        const LSTATUS status = ::RegEnumValueW(handle, index, name.data(), &name_size, nullptr,
                                               &type, data.data(), &data_size);
        if (status == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (status != ERROR_SUCCESS) {
            continue;
        }
        if (type != REG_SZ && type != REG_EXPAND_SZ) {
            continue;
        }

        std::wstring command(reinterpret_cast<const wchar_t*>(data.data()),
                             data_size / sizeof(wchar_t));
        command.resize(std::wcslen(command.c_str()));

        // REG_EXPAND_SZ guarda o caminho com variaveis por resolver, como
        // "%windir%\system32\...". Sem expandir, o caminho nao casaria com a
        // deny-list nem apontaria para lugar nenhum.
        if (type == REG_EXPAND_SZ) {
            std::array<wchar_t, 32767> expanded{};
            const DWORD written = ::ExpandEnvironmentStringsW(
                command.c_str(), expanded.data(), static_cast<DWORD>(expanded.size()));
            if (written > 0 && written <= expanded.size()) {
                command.assign(expanded.data(), written - 1);
            }
        }

        core::StartupItemInfo item;
        item.name = to_utf8(std::wstring_view(name.data(), name_size));
        item.path = executable_from_command(to_utf8(command));
        item.essential = looks_essential(item.name, item.path);
        item.origin = key.origin;
        item.enabled = commands::startup_is_enabled(item.name, item.origin);

        if (!item.name.empty()) {
            items.push_back(std::move(item));
        }
    }

    ::RegCloseKey(handle);
}

std::filesystem::path startup_folder(const wchar_t* variable, const wchar_t* suffix) {
    std::array<wchar_t, 32767> buffer{};

    const DWORD written = ::GetEnvironmentVariableW(variable, buffer.data(),
                                                    static_cast<DWORD>(buffer.size()));
    if (written == 0 || written >= buffer.size()) {
        return {};
    }
    return std::filesystem::path(std::wstring(buffer.data(), written)) / suffix;
}

void read_startup_folder(const std::filesystem::path& folder, core::StartupOrigin origin,
                         std::vector<core::StartupItemInfo>& items) {
    if (folder.empty()) {
        return;
    }

    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(folder, error)) {
        if (!entry.is_regular_file(error)) {
            continue;
        }

        const auto& path = entry.path();
        if (path.filename() == "desktop.ini") {
            continue;
        }

        core::StartupItemInfo item;
        item.name = path.stem().string();
        item.path = path.string();
        item.essential = looks_essential(item.name, item.path);
        item.origin = origin;

        // Para atalho, o Windows guarda o interruptor sob o nome do arquivo com
        // extensao — nao sob o nome exibido.
        item.enabled = commands::startup_is_enabled(path.filename().string(), item.origin);

        items.push_back(std::move(item));
    }
}

}

std::string executable_from_command(const std::string& command) {
    const auto first = command.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return {};
    }

    if (command[first] == '"') {
        const auto closing = command.find('"', first + 1);
        if (closing != std::string::npos) {
            return command.substr(first + 1, closing - first - 1);
        }
        return command.substr(first + 1);
    }

    // Sem aspas, o proprio Windows resolve a ambiguidade tentando o prefixo
    // mais longo que existe. Aqui basta cortar na extensao: e o que separa o
    // executavel dos argumentos em praticamente todo item de inicializacao.
    const auto extension = detail::find_ignoring_case(command, ".exe");
    if (extension != std::string::npos) {
        return command.substr(first, extension + 4 - first);
    }

    const auto space = command.find(' ', first);
    return command.substr(first, space == std::string::npos ? std::string::npos : space - first);
}

bool looks_essential(const std::string& name, const std::string& path) {
    // Reconhecimento por marca e por componente. A lista e curta de proposito:
    // cada entrada precisa ser algo que claramente nao deve entrar numa
    // sugestao de desativar.
    constexpr std::array kMarkers{
        // Seguranca
        std::string_view{"defender"}, std::string_view{"securityhealth"},
        std::string_view{"msmpeng"},  std::string_view{"avast"},
        std::string_view{"avg"},      std::string_view{"kaspersky"},
        std::string_view{"bitdefender"}, std::string_view{"mcafee"},
        std::string_view{"norton"},   std::string_view{"eset"},
        std::string_view{"malwarebytes"}, std::string_view{"sophos"},
        std::string_view{"trendmicro"},

        // Audio
        std::string_view{"realtek"},  std::string_view{"rtkaud"},
        std::string_view{"nahimic"},  std::string_view{"waves"},
        std::string_view{"audiosrv"}, std::string_view{"soundblaster"},

        // Video
        std::string_view{"nvidia"},   std::string_view{"nvcpl"},
        std::string_view{"igfx"},     std::string_view{"intelgraphics"},
        std::string_view{"radeon"},   std::string_view{"amdow"},
        std::string_view{"atibtmon"},

        // Entrada e ferramentas de hardware
        std::string_view{"syntp"},    std::string_view{"etdctrl"},
        std::string_view{"touchpad"}, std::string_view{"hotkey"},
        std::string_view{"logitech"}, std::string_view{"synaptics"},
    };

    for (const auto& marker : kMarkers) {
        if (contains_ignoring_case(name, marker) || contains_ignoring_case(path, marker)) {
            return true;
        }
    }
    return false;
}

std::vector<core::StartupItemInfo> StartupCollector::collect() const {
    std::vector<core::StartupItemInfo> items;

    for (const auto& key : run_keys()) {
        read_run_key(key, items);
    }

    read_startup_folder(
        startup_folder(L"APPDATA", LR"(Microsoft\Windows\Start Menu\Programs\Startup)"),
        core::StartupOrigin::UserFolder, items);
    read_startup_folder(
        startup_folder(L"ProgramData", LR"(Microsoft\Windows\Start Menu\Programs\StartUp)"),
        core::StartupOrigin::MachineFolder, items);

    return items;
}

bool StartupCollector::collect_into(core::SystemSnapshot& snapshot) const {
    snapshot.startup_items = collect();

    // Lista vazia e um resultado legitimo: existe maquina sem nada na
    // inicializacao. O que importa e ter conseguido olhar.
    snapshot.startup_available = true;
    return true;
}

}
