#include "collectors/security_collector.hpp"

#include "collectors/detail/wmi.hpp"

namespace zelo::collectors {

core::SecurityInfo SecurityCollector::collect() const {
    core::SecurityInfo info;

    bool found_status = false;
    const bool queried = detail::query_wmi(
        LR"(ROOT\Microsoft\Windows\Defender)", L"SELECT * FROM MSFT_MpComputerStatus",
        [&info, &found_status](const detail::WmiRow& row) {
            found_status = true;

            info.antivirus_enabled = row.boolean(L"AntivirusEnabled").value_or(false);
            info.realtime_protection_enabled =
                row.boolean(L"RealTimeProtectionEnabled").value_or(false);

            if (const auto age = row.number(L"AntivirusSignatureAge")) {
                info.signature_age_days = static_cast<int>(*age);
            }
            if (const auto age = row.number(L"QuickScanAge")) {
                info.last_quick_scan_age_days = static_cast<int>(*age);
            }
        });

    if (!queried) {
        return info;
    }

    if (!found_status) {
        // O namespace do Defender responde mas nao devolve estado. Costuma
        // acontecer quando um antivirus de terceiros assumiu a protecao.
        detail::query_wmi(LR"(ROOT\SecurityCenter2)",
                          L"SELECT displayName FROM AntiVirusProduct",
                          [&info, &found_status](const detail::WmiRow& row) {
                              if (const auto name = row.text(L"displayName")) {
                                  info.provider = *name;
                                  found_status = true;
                              }
                          });

        // Antivirus de terceiros presente e um estado conhecido: ha protecao,
        // apenas nao e a do Windows, e o Zelo nao tem como avaliar a dele.
        info.available = found_status;
        return info;
    }

    info.provider = "Seguranca do Windows";
    info.available = true;
    return info;
}

bool SecurityCollector::collect_into(core::SystemSnapshot& snapshot) const {
    snapshot.security = collect();
    return snapshot.security.available;
}

}
