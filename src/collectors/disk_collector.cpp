#include "collectors/disk_collector.hpp"

#include "collectors/detail/wmi.hpp"

#include <map>
#include <string>

namespace zelo::collectors {

namespace {

/// O Windows devolve o tipo de midia como numero.
std::string media_type_name(std::int64_t value) {
    switch (value) {
    case 3:
        return "HDD";
    case 4:
        return "SSD";
    case 5:
        return "SCM";
    default:
        return "desconhecido";
    }
}

/// E a saude como enumeracao, nao como texto.
std::string health_name(std::int64_t value) {
    switch (value) {
    case 0:
        return "Healthy";
    case 1:
        return "Warning";
    case 2:
        return "Unhealthy";
    default:
        return "desconhecido";
    }
}

int optional_counter(const std::optional<std::int64_t>& value) {
    // Sem o contador, fica negativo: ausencia de dado nao pode virar zero, que
    // a analise leria como "nenhum erro registrado".
    return value.has_value() ? static_cast<int>(*value) : -1;
}

}

core::DisksInfo DiskCollector::collect() const {
    core::DisksInfo info;

    std::map<std::string, core::PhysicalDiskInfo> by_id;

    const bool queried = detail::query_wmi(
        LR"(ROOT\Microsoft\Windows\Storage)",
        L"SELECT DeviceId, FriendlyName, MediaType, Size, HealthStatus FROM MSFT_PhysicalDisk",
        [&by_id](const detail::WmiRow& row) {
            const auto id = row.text(L"DeviceId");
            if (!id) {
                return;
            }

            core::PhysicalDiskInfo disk;
            disk.model = row.text(L"FriendlyName").value_or("disco sem nome");
            disk.media_type = media_type_name(row.number(L"MediaType").value_or(-1));
            disk.size_bytes = static_cast<std::uint64_t>(row.number(L"Size").value_or(0));
            disk.health_status = health_name(row.number(L"HealthStatus").value_or(-1));

            by_id.emplace(*id, std::move(disk));
        });

    if (!queried) {
        return info;
    }

    // Os contadores vivem numa classe separada e podem simplesmente nao existir
    // para um disco. Falhar aqui nao invalida o que ja foi lido.
    detail::query_wmi(
        LR"(ROOT\Microsoft\Windows\Storage)",
        L"SELECT DeviceId, ReadErrorsTotal, WriteErrorsTotal, Temperature, Wear FROM "
        L"MSFT_StorageReliabilityCounter",
        [&by_id](const detail::WmiRow& row) {
            const auto id = row.text(L"DeviceId");
            if (!id) {
                return;
            }

            const auto entry = by_id.find(*id);
            if (entry == by_id.end()) {
                return;
            }

            entry->second.read_errors = optional_counter(row.number(L"ReadErrorsTotal"));
            entry->second.write_errors = optional_counter(row.number(L"WriteErrorsTotal"));
            entry->second.temperature_celsius = optional_counter(row.number(L"Temperature"));
            entry->second.wear_percent = optional_counter(row.number(L"Wear"));
        });

    info.disks.reserve(by_id.size());
    for (auto& [id, disk] : by_id) {
        info.disks.push_back(std::move(disk));
    }

    info.available = true;
    return info;
}

bool DiskCollector::collect_into(core::SystemSnapshot& snapshot) const {
    snapshot.disks = collect();
    return snapshot.disks.available;
}

}
