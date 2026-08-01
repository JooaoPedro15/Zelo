#include "collectors/detail/event_log.hpp"

#include "collectors/detail/text.hpp"

#include <spdlog/spdlog.h>

#include <array>
#include <cwchar>

#include <windows.h>

#include <winevt.h>

namespace cleaner::collectors::detail {

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
    EventHandle(EventHandle&&) = delete;
    EventHandle& operator=(EventHandle&&) = delete;

    [[nodiscard]] EVT_HANDLE get() const { return handle_; }
    [[nodiscard]] explicit operator bool() const { return handle_ != nullptr; }

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
    return to_utf8(buffer);
}

std::optional<std::string_view> between(std::string_view text, std::string_view open,
                                        std::string_view close) {
    const auto start = text.find(open);
    if (start == std::string_view::npos) {
        return std::nullopt;
    }

    const auto content = start + open.size();
    const auto end = text.find(close, content);
    if (end == std::string_view::npos) {
        return std::nullopt;
    }

    return text.substr(content, end - content);
}

}

std::optional<std::vector<std::string>> query_event_channel(const wchar_t* channel,
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
            const EventHandle event{batch.at(index)};
            if (auto xml = render_event(event.get()); !xml.empty()) {
                documents.push_back(std::move(xml));
            }
        }
    }

    return documents;
}

std::string event_time(std::string_view xml) {
    for (const auto* open : {"SystemTime='", "SystemTime=\""}) {
        const std::string_view close = open[std::char_traits<char>::length(open) - 1] == '\'' ? "'"
                                                                                              : "\"";
        if (const auto value = between(xml, open, close)) {
            return std::string(*value);
        }
    }
    return {};
}

std::string event_field(std::string_view xml, std::string_view name) {
    for (const auto quote : {'\'', '"'}) {
        const std::string open = std::string("<Data Name=") + quote + std::string(name) + quote + ">";
        if (const auto value = between(xml, open, "</Data>")) {
            return std::string(*value);
        }
    }
    return {};
}

}
