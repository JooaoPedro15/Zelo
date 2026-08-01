#include "collectors/update_collector.hpp"

#include <array>
#include <string>

#include <windows.h>

namespace cleaner::collectors {

namespace {

struct RebootMarker {
    HKEY hive;
    const wchar_t* subkey;
    const wchar_t* value;
    const char* reason;
};

/// Os lugares onde o Windows anota que falta reiniciar. Cada componente usa o
/// seu, entao olhar so um deixaria casos passar.
const std::array<RebootMarker, 4>& reboot_markers() {
    static const std::array<RebootMarker, 4> markers{
        RebootMarker{HKEY_LOCAL_MACHINE,
                     LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootPending)",
                     nullptr, "instalacao de componentes do Windows aguardando reinicio"},
        RebootMarker{HKEY_LOCAL_MACHINE,
                     LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired)",
                     nullptr, "atualizacao do Windows aguardando reinicio"},
        RebootMarker{HKEY_LOCAL_MACHINE,
                     LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootInProgress)",
                     nullptr, "manutencao de componentes em andamento"},
        RebootMarker{HKEY_LOCAL_MACHINE, LR"(SYSTEM\CurrentControlSet\Control\Session Manager)",
                     L"PendingFileRenameOperations",
                     "arquivos aguardando substituicao no proximo inicio"},
    };
    return markers;
}

bool marker_present(const RebootMarker& marker) {
    HKEY handle = nullptr;
    if (::RegOpenKeyExW(marker.hive, marker.subkey, 0, KEY_READ | KEY_WOW64_64KEY, &handle) !=
        ERROR_SUCCESS) {
        return false;
    }

    bool present = false;
    if (marker.value == nullptr) {
        // A propria existencia da chave e o sinal.
        present = true;
    } else {
        DWORD size = 0;
        present = ::RegQueryValueExW(handle, marker.value, nullptr, nullptr, nullptr, &size) ==
                      ERROR_SUCCESS &&
                  size > 0;
    }

    ::RegCloseKey(handle);
    return present;
}

}

core::UpdatesInfo UpdateCollector::collect() const {
    core::UpdatesInfo info;

    for (const auto& marker : reboot_markers()) {
        if (marker_present(marker)) {
            info.reboot_pending = true;
            info.reboot_reasons.emplace_back(marker.reason);
        }
    }

    // Ler o registro nao falha por permissao aqui: estas chaves sao legiveis
    // por usuario comum. Nao encontrar marcador nenhum e resposta legitima.
    info.available = true;
    return info;
}

bool UpdateCollector::collect_into(core::SystemSnapshot& snapshot) const {
    snapshot.updates = collect();
    return snapshot.updates.available;
}

}
