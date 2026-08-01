#include "collectors/disk_collector.hpp"

#include "collectors/detail/event_log.hpp"
#include "collectors/detail/wmi.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>

namespace cleaner::collectors {

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

DiskCollector::DiskCollector(int window_days) : window_days_(window_days) {}

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

    // Os eventos do sistema de arquivos vem do log, nao do WMI: falhar la nao
    // invalida o que ja foi lido sobre os discos fisicos.
    collect_filesystem_events(info);

    info.available = true;
    return info;
}

void DiskCollector::collect_filesystem_events(core::DisksInfo& info) const {
    const auto window_ms =
        std::to_wstring(static_cast<long long>(window_days_) * 24 * 60 * 60 * 1000);

    // Evento 55 do NTFS: "corrupcao detectada na estrutura do sistema de
    // arquivos". E o aviso mais direto que o Windows da de que um volume
    // precisa de verificacao.
    const std::wstring query = L"*[System[Provider[@Name='Ntfs'] and EventID=55 and "
                               L"TimeCreated[timediff(@SystemTime) <= " +
                               window_ms + L"]]]";

    const auto documents = detail::query_event_channel(L"System", query, 200);
    if (!documents) {
        return;
    }

    std::set<std::string> volumes;
    for (const auto& document : *documents) {
        ++info.filesystem_corruption.event_count;

        if (const auto volume = detail::event_field(document, "DriveName"); !volume.empty()) {
            volumes.insert(volume);
        }
        if (const auto when = detail::event_time(document); !when.empty()) {
            info.filesystem_corruption.last_seen =
                std::max(info.filesystem_corruption.last_seen, when);
        }
    }

    info.filesystem_corruption.affected_volumes.assign(volumes.begin(), volumes.end());
    info.filesystem_events_available = true;
}

bool DiskCollector::collect_into(core::SystemSnapshot& snapshot) const {
    snapshot.disks = collect();
    return snapshot.disks.available;
}

}
