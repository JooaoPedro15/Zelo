#include <catch2/catch_test_macros.hpp>
#include <collectors/event_log_parsing.hpp>

#include <string>
#include <vector>

using cleaner::collectors::ParsedFailureEvent;
using cleaner::collectors::group_failures;
using cleaner::collectors::parse_failure_event;

namespace {

/// Formato de um evento 1000 real, copiado do canal Application desta maquina.
/// Provocar falhas de verdade para testar nao e viavel, entao o XML fixo e a
/// unica forma honesta de exercitar isto — desde que seja o formato de verdade.
std::string failure_xml(const std::string& application, const std::string& module,
                        const std::string& when) {
    return R"(<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'><System>)"
           R"(<Provider Name='Application Error'/><EventID>1000</EventID><Level>2</Level>)"
           R"(<TimeCreated SystemTime=')" +
           when + R"('/><Channel>Application</Channel></System><EventData>)"
                  R"(<Data Name='AppName'>)" +
           application + R"(</Data><Data Name='AppVersion'>6.8.3.0</Data>)"
                         R"(<Data Name='AppTimeStamp'>67d82e79</Data>)"
                         R"(<Data Name='ModuleName'>)" +
           module + R"(</Data><Data Name='ModuleVersion'>0.0.0.0</Data>)"
                    R"(<Data Name='ExceptionCode'>c0000005</Data>)"
                    R"(</EventData></Event>)";
}

/// O evento 1002, de programa que parou de responder, nao nomeia os campos.
std::string hang_xml(const std::string& application) {
    return R"(<Event><System><EventID>1002</EventID>)"
           R"(<TimeCreated SystemTime='2026-07-20T10:15:00.000Z'/></System><EventData>)"
           R"(<Data>)" +
           application + R"(</Data><Data>1.0.0.0</Data><Data>0x1</Data>)"
                         R"(<Data>modulo.dll</Data></EventData></Event>)";
}

}

TEST_CASE("os campos de uma falha de aplicativo sao extraidos", "[event_log]") {
    const auto event =
        parse_failure_event(failure_xml("Programa.exe", "ntdll.dll", "2026-07-20T10:15:00.000Z"));

    REQUIRE(event.has_value());
    CHECK(event->application == "Programa.exe");
    CHECK(event->faulting_module == "ntdll.dll");
    CHECK(event->when == "2026-07-20T10:15:00.000Z");
}

// Nem todo evento nomeia os campos. O 1002 usa ordem fixa, e ignorar isso
// deixaria de fora justamente os programas que travam sem fechar.
TEST_CASE("evento sem nomes nos campos tambem e lido", "[event_log]") {
    const auto event = parse_failure_event(hang_xml("Travado.exe"));

    REQUIRE(event.has_value());
    CHECK(event->application == "Travado.exe");
    CHECK(event->faulting_module == "modulo.dll");
}

TEST_CASE("evento sem dados uteis e descartado", "[event_log]") {
    CHECK_FALSE(parse_failure_event("<Event></Event>").has_value());
    CHECK_FALSE(parse_failure_event("").has_value());
    CHECK_FALSE(parse_failure_event("<Event><EventData><Data></Data></EventData></Event>")
                    .has_value());
}

// Um evento truncado ou em formato inesperado nao pode derrubar a analise.
TEST_CASE("xml malformado nao quebra a leitura", "[event_log]") {
    CHECK_NOTHROW(parse_failure_event("<Event><Data>sem fechamento"));
    CHECK_NOTHROW(parse_failure_event("<<<>>>"));
    CHECK_NOTHROW(parse_failure_event(std::string(5000, '<')));
}

// O usuario precisa ver que um programa falhou doze vezes, nao doze linhas
// iguais.
TEST_CASE("falhas do mesmo programa sao agrupadas e contadas", "[event_log]") {
    const std::vector<ParsedFailureEvent> events{
        {.application = "Editor.exe", .faulting_module = "a.dll", .when = "2026-07-20T10:00:00Z"},
        {.application = "Jogo.exe", .faulting_module = "b.dll", .when = "2026-07-21T10:00:00Z"},
        {.application = "Editor.exe", .faulting_module = "a.dll", .when = "2026-07-22T10:00:00Z"},
        {.application = "Editor.exe", .faulting_module = "a.dll", .when = "2026-07-18T10:00:00Z"},
    };

    const auto grouped = group_failures(events);

    REQUIRE(grouped.size() == 2);

    // Ordenado por quantidade: o que mais falha aparece primeiro.
    CHECK(grouped.at(0).application == "Editor.exe");
    CHECK(grouped.at(0).count == 3);
    CHECK(grouped.at(1).count == 1);

    // O periodo cobre a ocorrencia mais antiga e a mais recente, independente
    // da ordem em que os eventos chegaram.
    CHECK(grouped.at(0).first_seen == "2026-07-18T10:00:00Z");
    CHECK(grouped.at(0).last_seen == "2026-07-22T10:00:00Z");
}

TEST_CASE("sem eventos o agrupamento devolve lista vazia", "[event_log]") {
    CHECK(group_failures({}).empty());
}
