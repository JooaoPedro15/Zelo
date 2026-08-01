#include "commands/startup_control.hpp"

#include <windows.h>

#include <array>
#include <cstring>

namespace cleaner::commands {

namespace {

using core::StartupOrigin;

struct ApprovalKey {
    HKEY hive;
    const wchar_t* subkey;
    REGSAM view;
};

/// Onde o Windows guarda o interruptor de cada item.
///
/// A chave e a mesma que o Gerenciador de Tarefas usa. Escrever aqui nao mexe na
/// entrada original: a chave Run continua com o comando, e o atalho continua na
/// pasta.
ApprovalKey approval_key(StartupOrigin origin) {
    constexpr const wchar_t* kBase =
        LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\)";

    switch (origin) {
    case StartupOrigin::UserRun:
        return {HKEY_CURRENT_USER, LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run)",
                KEY_WOW64_64KEY};
    case StartupOrigin::UserRun32:
        return {HKEY_CURRENT_USER, LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run32)",
                KEY_WOW64_64KEY};
    case StartupOrigin::MachineRun:
        return {HKEY_LOCAL_MACHINE, LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run)",
                KEY_WOW64_64KEY};
    case StartupOrigin::MachineRun32:
        return {HKEY_LOCAL_MACHINE, LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run32)",
                KEY_WOW64_64KEY};
    case StartupOrigin::UserFolder:
        return {HKEY_CURRENT_USER, LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\StartupFolder)",
                KEY_WOW64_64KEY};
    case StartupOrigin::MachineFolder:
        return {HKEY_LOCAL_MACHINE, LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\StartupFolder)",
                KEY_WOW64_64KEY};
    }

    // Inalcancavel: o switch cobre o enum e o compilador cobra por isso.
    static_cast<void>(kBase);
    return {HKEY_CURRENT_USER, LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run)",
            KEY_WOW64_64KEY};
}

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

// O primeiro byte carrega o estado. O resto e a hora em que o item foi
// desativado, que o Windows preenche e o Cleaner zera ao religar.
constexpr BYTE kEnabled = 0x02;
constexpr BYTE kDisabled = 0x03;
constexpr std::size_t kValueSize = 12;

}

bool startup_is_enabled(const std::string& name, StartupOrigin origin) {
    const auto key = approval_key(origin);

    HKEY handle = nullptr;
    if (RegOpenKeyExW(key.hive, key.subkey, 0, KEY_READ | key.view, &handle) != ERROR_SUCCESS) {
        // Sem a chave, nada foi desativado neste computador.
        return true;
    }

    std::array<BYTE, 64> data{};
    DWORD size = static_cast<DWORD>(data.size());
    DWORD type = 0;

    const LSTATUS status =
        RegQueryValueExW(handle, widen(name).c_str(), nullptr, &type, data.data(), &size);
    RegCloseKey(handle);

    if (status != ERROR_SUCCESS || size == 0) {
        // Item sem registro proprio nunca foi desativado.
        return true;
    }

    return (data[0] & 0x01) == 0;
}

StartupChange set_startup_enabled(const std::string& name, StartupOrigin origin, bool enabled) {
    if (startup_is_enabled(name, origin) == enabled) {
        return StartupChange::Unchanged;
    }

    const auto key = approval_key(origin);

    HKEY handle = nullptr;
    const LSTATUS opened = RegCreateKeyExW(key.hive, key.subkey, 0, nullptr,
                                           REG_OPTION_NON_VOLATILE, KEY_SET_VALUE | key.view,
                                           nullptr, &handle, nullptr);
    if (opened == ERROR_ACCESS_DENIED) {
        return StartupChange::Denied;
    }
    if (opened != ERROR_SUCCESS) {
        return StartupChange::Failed;
    }

    std::array<BYTE, kValueSize> value{};
    value[0] = enabled ? kEnabled : kDisabled;

    if (!enabled) {
        // O Windows guarda aqui a hora da desativacao. Preencher com a hora de
        // agora deixa o item igual ao que o Gerenciador de Tarefas produziria.
        FILETIME now{};
        GetSystemTimeAsFileTime(&now);
        std::memcpy(value.data() + 4, &now, sizeof(now));
    }

    const LSTATUS written = RegSetValueExW(handle, widen(name).c_str(), 0, REG_BINARY, value.data(),
                                           static_cast<DWORD>(value.size()));
    RegCloseKey(handle);

    if (written == ERROR_ACCESS_DENIED) {
        return StartupChange::Denied;
    }
    if (written != ERROR_SUCCESS) {
        return StartupChange::Failed;
    }

    return enabled ? StartupChange::Enabled : StartupChange::Disabled;
}

}
