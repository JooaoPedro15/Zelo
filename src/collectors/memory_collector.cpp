#include "collectors/memory_collector.hpp"

#include <windows.h>

namespace zelo::collectors {

core::MemoryInfo MemoryCollector::collect() const {
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);

    core::MemoryInfo info;
    if (::GlobalMemoryStatusEx(&status) == 0) {
        return info;
    }

    info.total_bytes = status.ullTotalPhys;
    info.available_bytes = status.ullAvailPhys;
    info.load_percent = static_cast<int>(status.dwMemoryLoad);
    info.available = true;
    return info;
}

bool MemoryCollector::collect_into(core::SystemSnapshot& snapshot) const {
    snapshot.memory = collect();
    return snapshot.memory.available;
}

}
