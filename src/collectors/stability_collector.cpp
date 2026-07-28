#include "collectors/stability_collector.hpp"

#include "collectors/detail/text.hpp"
#include "collectors/event_log_parsing.hpp"

#include <spdlog/spdlog.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <windows.h>
#include <winevt.h>

namespace zelo::collectors {

namespace {

/// RAII para os handles do log de eventos: sao varios pontos de saida, e vazar
/// handle num aplicativo que o usuario deixa aberto e um problema real.
class EventHandle {
public:
    explicit EventHandle(EVT_HANDLE handle = nullptr) : handle_(handle) {}

    ~EventHandle() {
        if (handle_ != nullptr) {
            ::EvtClose(handle_);
        }
    }

    EventHandle(const EventHandle&) = delete;
    EventHandle& operator=(const EventHandle&) = delete;
    EventHandle(EventHandle&& other) noexcept : handle_(other.release()) {}

    EventHandle& operator=(EventHandle&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    [[nodiscard]] EVT_HANDLE get() const { return handle_; }
    [[nodiscard]] explicit operator bool() const { return handle_ != nullptr; }

    EVT_HANDLE release() {
        EVT_HANDLE released = handle_;
        handle_ = nullptr;
        return released;
    }

    void reset(EVT_HANDLE handle) {
        if (handle_ != nullptr) {
            ::EvtClose(handle_);
        }
        handle_ = handle;
    }

private:
    EVT_HANDLE handle_;
};

std::string render_event(EVT_HANDLE event) {
    DWORD needed = 0;
    DWORD produced = 0;
    ::EvtRender(nullptr, event, EvtRenderEventXml, 0, nullptr, &needed, &produced);

    if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER || needed == 0) {
        return {};
    }

    std::wstring buffer(needed / sizeof(wchar_t) + 1, L'\0');
    if (::EvtRender(nullptr, event, EvtRenderEventXml, needed, buffer.data(), &needed, &produced) ==
        FALSE) {
        return {};
    }

    buffer.resize(std::wcslen(buffer.c_str()));
    return detail::to_utf8(buffer);
}

/// Percorre um canal com a consulta dada, devolvendo o XML de cada evento.
///
/// Devolve vazio (`nullopt`) quando o canal nao pode ser consultado. Lista
/// vazia e resultado legitimo; nao conseguir olhar e outra coisa, e confundir
/// os dois faria a analise afirmar que esta tudo bem sem ter visto nada.
std::optional<std::vector<std::string>> query_channel(const wchar_t* channel,
                                                      const std::wstring& query,
                                                      std::size_t limit) {
    const EventHandle results(::EvtQuery(nullptr, channel, query.c_str(),
                                         EvtQueryChannelPath | EvtQueryReverseDirection));
    if (!results) {
        spdlog::warn("nao foi possivel consultar o canal de eventos (erro {})", ::GetLastError());
        return std::nullopt;
    }

    std::vector<std::string> documents;

    while (documents.size() < limit) {
        std::array<EVT_HANDLE, 32> batch{};
        DWORD returned = 0;

        if (::EvtNext(results.get(), static_cast<DWORD>(batch.size()), batch.data(), INFINITE, 0,
                      &returned) == FALSE) {
            if (const DWORD error = ::GetLastError(); error != ERROR_NO_MORE_ITEMS) {
                spdlog::warn("leitura de eventos interrompida (erro {})", error);
            }
            break;
        }

        for (DWORD index = 0; index < returned; ++index) {
            const EventHandle event(batch.at(index));
            if (auto xml = render_event(event.get()); !xml.empty()) {
                documents.push_back(std::move(xml));
            }
        }
    }

    return documents;
}

}

StabilityCollector::StabilityCollector(int window_days) : window_days_(window_days) {}

core::StabilityInfo StabilityCollector::collect() const {
    core::StabilityInfo info;
    info.window_days = window_days_;

    const auto window_ms = std::to_wstring(static_cast<long long>(window_days_) * 24 * 60 * 60 * 1000);

    // 1000 e falha de aplicativo, 1002 e programa que parou de responder.
    const std::wstring failures_query =
        L"*[System[(EventID=1000 or EventID=1002) and TimeCreated[timediff(@SystemTime) <= " +
        window_ms + L"]]]";

    const auto failure_documents = query_channel(L"Application", failures_query, 500);
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
    if (const auto shutdowns = query_channel(L"System", shutdown_query, 200)) {
        info.unexpected_shutdowns = shutdowns->size();
    }

    info.available = true;
    return info;
}

bool StabilityCollector::collect_into(core::SystemSnapshot& snapshot) const {
    snapshot.stability = collect();
    return snapshot.stability.available;
}

}
