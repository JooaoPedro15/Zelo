#include "collectors/event_log_parsing.hpp"

#include <algorithm>
#include <map>
#include <string>

namespace zelo::collectors {

namespace {

/// Extrai o conteudo entre a primeira ocorrencia de `open` e o `close`
/// seguinte, a partir de `from`.
std::optional<std::string_view> between(std::string_view text, std::string_view open,
                                        std::string_view close, std::size_t from = 0) {
    const auto start = text.find(open, from);
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

/// Busca `<Data Name='...'>`. O evento 1000 nomeia seus campos, e usar o nome
/// e mais seguro do que contar posicoes que podem mudar entre versoes do
/// Windows.
std::optional<std::string_view> named_field(std::string_view xml, std::string_view name) {
    for (const auto quote : {'\'', '"'}) {
        const std::string open = std::string("<Data Name=") + quote + std::string(name) + quote + ">";
        if (const auto value = between(xml, open, "</Data>")) {
            return value;
        }
    }
    return std::nullopt;
}

/// Nem todo evento nomeia os campos — o 1002, de programa que parou de
/// responder, usa `<Data>` sem nome e em ordem fixa.
std::vector<std::string_view> positional_fields(std::string_view xml) {
    std::vector<std::string_view> fields;

    std::size_t position = 0;
    while (const auto field = between(xml, "<Data>", "</Data>", position)) {
        fields.push_back(*field);
        position = static_cast<std::size_t>(field->data() - xml.data()) + field->size();
    }

    return fields;
}

}

std::optional<ParsedFailureEvent> parse_failure_event(std::string_view xml) {
    ParsedFailureEvent event;

    if (const auto application = named_field(xml, "AppName")) {
        event.application = std::string(*application);
        if (const auto module = named_field(xml, "ModuleName")) {
            event.faulting_module = std::string(*module);
        }
    } else {
        const auto fields = positional_fields(xml);
        if (fields.empty()) {
            return std::nullopt;
        }

        event.application = std::string(fields.front());

        constexpr std::size_t kFaultingModuleIndex = 3;
        if (fields.size() > kFaultingModuleIndex) {
            event.faulting_module = std::string(fields.at(kFaultingModuleIndex));
        }
    }

    if (event.application.empty()) {
        return std::nullopt;
    }

    if (const auto when = between(xml, "SystemTime='", "'")) {
        event.when = std::string(*when);
    } else if (const auto quoted = between(xml, "SystemTime=\"", "\"")) {
        event.when = std::string(*quoted);
    }

    return event;
}

std::vector<core::AppFailureInfo> group_failures(const std::vector<ParsedFailureEvent>& events) {
    std::map<std::string, core::AppFailureInfo> grouped;

    for (const auto& event : events) {
        auto& failure = grouped[event.application];

        if (failure.count == 0) {
            failure.application = event.application;
            failure.faulting_module = event.faulting_module;
            failure.first_seen = event.when;
            failure.last_seen = event.when;
        }

        ++failure.count;

        // Os eventos chegam do mais recente para o mais antigo, mas nao da para
        // depender disso: comparar texto ISO 8601 ordena corretamente.
        if (!event.when.empty()) {
            if (failure.first_seen.empty() || event.when < failure.first_seen) {
                failure.first_seen = event.when;
            }
            if (event.when > failure.last_seen) {
                failure.last_seen = event.when;
            }
        }
    }

    std::vector<core::AppFailureInfo> failures;
    failures.reserve(grouped.size());
    for (auto& [application, failure] : grouped) {
        failures.push_back(std::move(failure));
    }

    std::sort(failures.begin(), failures.end(),
              [](const core::AppFailureInfo& left, const core::AppFailureInfo& right) {
                  return left.count > right.count;
              });

    return failures;
}

}
