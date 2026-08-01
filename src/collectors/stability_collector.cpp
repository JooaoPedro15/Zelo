#include "collectors/stability_collector.hpp"

#include "collectors/detail/event_log.hpp"
#include "collectors/detail/text.hpp"
#include "collectors/event_log_parsing.hpp"

#include <spdlog/spdlog.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <windows.h>
#include <winevt.h>

namespace cleaner::collectors {

StabilityCollector::StabilityCollector(int window_days) : window_days_(window_days) {}

core::StabilityInfo StabilityCollector::collect() const {
    core::StabilityInfo info;
    info.window_days = window_days_;

    const auto window_ms = std::to_wstring(static_cast<long long>(window_days_) * 24 * 60 * 60 * 1000);

    // 1000 e falha de aplicativo, 1002 e programa que parou de responder.
    const std::wstring failures_query =
        L"*[System[(EventID=1000 or EventID=1002) and TimeCreated[timediff(@SystemTime) <= " +
        window_ms + L"]]]";

    const auto failure_documents = detail::query_event_channel(L"Application", failures_query, 500);
    if (!failure_documents) {
        // Sem conseguir ler o canal principal nao ha o que afirmar sobre
        // estabilidade. A analise dira que nao olhou.
        return info;
    }

    std::vector<ParsedFailureEvent> events;
    events.reserve(failure_documents->size());
    for (const auto& document : *failure_documents) {
        if (auto parsed = parse_failure_event(document)) {
            events.push_back(std::move(*parsed));
        }
    }
    info.app_failures = group_failures(events);

    // 41 e Kernel-Power (a maquina desligou sem encerrar), 6008 e o proprio
    // Windows registrando que o desligamento anterior foi inesperado.
    const std::wstring shutdown_query =
        L"*[System[(EventID=41 or EventID=6008) and TimeCreated[timediff(@SystemTime) <= " +
        window_ms + L"]]]";

    // O canal System pode exigir elevacao. Nao conseguir le-lo nao invalida o
    // que ja foi lido do canal Application.
    if (const auto shutdowns = detail::query_event_channel(L"System", shutdown_query, 200)) {
        info.unexpected_shutdowns = shutdowns->size();
    }

    info.available = true;
    return info;
}

core::IntegrityInfo StabilityCollector::collect_integrity() const {
    core::IntegrityInfo info;
    info.window_days = window_days_;

    const auto window_ms =
        std::to_wstring(static_cast<long long>(window_days_) * 24 * 60 * 60 * 1000);
    const std::wstring window_clause =
        L" and TimeCreated[timediff(@SystemTime) <= " + window_ms + L"]";

    // O servico de componentes registra aqui quando encontra algo danificado na
    // propria instalacao do Windows.
    const auto servicing = detail::query_event_channel(
        L"Setup",
        L"*[System[Provider[@Name='Microsoft-Windows-Servicing'] and Level=2" + window_clause + L"]]",
        200);

    // SideBySide registra assembly ou manifesto invalido â€” componente do
    // Windows danificado de verdade. O relatorio de erros do sistema nao serve
    // aqui: ele registra tela azul, que e problema de estabilidade, nao de
    // integridade de arquivo.
    const auto components = detail::query_event_channel(
        L"Application", L"*[System[Provider[@Name='SideBySide'] and Level=2" + window_clause + L"]]",
        200);

    if (!servicing && !components) {
        // Nenhum dos dois canais respondeu: nao ha o que afirmar sobre
        // integridade, e a analise dira que nao olhou.
        return info;
    }

    info.corruption_events = servicing ? servicing->size() : 0;
    info.component_events = components ? components->size() : 0;

    for (const auto* documents : {&servicing, &components}) {
        if (!*documents) {
            continue;
        }
        for (const auto& document : **documents) {
            if (const auto parsed = parse_failure_event(document); parsed && !parsed->when.empty()) {
                info.last_seen = std::max(info.last_seen, parsed->when);
            }
        }
    }

    info.available = true;
    return info;
}

bool StabilityCollector::collect_integrity_into(core::SystemSnapshot& snapshot) const {
    snapshot.integrity = collect_integrity();
    return snapshot.integrity.available;
}

bool StabilityCollector::collect_into(core::SystemSnapshot& snapshot) const {
    snapshot.stability = collect();
    return snapshot.stability.available;
}

}
