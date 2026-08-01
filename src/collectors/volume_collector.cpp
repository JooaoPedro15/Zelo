#include "collectors/volume_collector.hpp"

#include <array>
#include <cwchar>

#include <windows.h>

namespace cleaner::collectors {

namespace {

std::string to_utf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }

    const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }

    std::string result(static_cast<std::size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(),
                          size, nullptr, nullptr);
    return result;
}

std::wstring system_drive_root() {
    std::array<wchar_t, MAX_PATH + 1> buffer{};

    const UINT written = ::GetWindowsDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
    if (written < 3) {
        return {};
    }
    return std::wstring(buffer.data(), 3);
}

}

std::vector<core::VolumeInfo> VolumeCollector::collect() const {
    std::array<wchar_t, 512> buffer{};

    const DWORD written = ::GetLogicalDriveStringsW(static_cast<DWORD>(buffer.size()),
                                                    buffer.data());
    if (written == 0 || written >= buffer.size()) {
        return {};
    }

    const std::wstring system_root = system_drive_root();

    std::vector<core::VolumeInfo> volumes;
    for (const wchar_t* entry = buffer.data(); *entry != L'\0'; entry += std::wcslen(entry) + 1) {
        const std::wstring root = entry;

        // Removiveis e unidades de rede ficam de fora: o produto fala sobre o
        // armazenamento da maquina, e midia ejetada distorceria a analise.
        if (::GetDriveTypeW(root.c_str()) != DRIVE_FIXED) {
            continue;
        }

        ULARGE_INTEGER free_for_caller{};
        ULARGE_INTEGER total{};
        ULARGE_INTEGER total_free{};
        if (::GetDiskFreeSpaceExW(root.c_str(), &free_for_caller, &total, &total_free) == 0) {
            // Volume presente mas sem responder. Omitir e melhor do que relatar
            // zero, que a analise leria como disco cheio.
            continue;
        }

        core::VolumeInfo volume;
        volume.letter = to_utf8(root.substr(0, 2));
        volume.total_bytes = total.QuadPart;
        volume.free_bytes = free_for_caller.QuadPart;
        volume.is_system = !system_root.empty() && root.starts_with(system_root.substr(0, 2));

        volumes.push_back(std::move(volume));
    }

    return volumes;
}

bool VolumeCollector::collect_into(core::SystemSnapshot& snapshot) const {
    auto volumes = collect();
    if (volumes.empty()) {
        return false;
    }

    snapshot.volumes = std::move(volumes);
    snapshot.volumes_available = true;
    return true;
}

}
