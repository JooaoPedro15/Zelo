#include "collectors/running_apps.hpp"

#include "collectors/detail/text.hpp"

#include <algorithm>
#include <map>

#include <windows.h>

#include <tlhelp32.h>

namespace zelo::collectors {

std::vector<RunningApp> running_apps() {
    const HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    std::map<std::string, std::size_t> counted;

    if (::Process32FirstW(snapshot, &entry) != 0) {
        do {
            counted[detail::to_utf8(std::wstring_view(entry.szExeFile))] += 1;
        } while (::Process32NextW(snapshot, &entry) != 0);
    }

    ::CloseHandle(snapshot);

    std::vector<RunningApp> apps;
    apps.reserve(counted.size());
    for (const auto& [executable, instances] : counted) {
        apps.push_back(RunningApp{.executable = executable, .instances = instances});
    }
    return apps;
}

bool is_running(const std::string& executable) {
    const auto apps = running_apps();

    return std::any_of(apps.begin(), apps.end(), [&executable](const RunningApp& app) {
        return detail::find_ignoring_case(app.executable, executable) == 0 &&
               app.executable.size() == executable.size();
    });
}

}
