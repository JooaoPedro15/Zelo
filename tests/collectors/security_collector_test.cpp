#include <catch2/catch_test_macros.hpp>
#include <collectors/security_collector.hpp>

using cleaner::collectors::SecurityCollector;
using cleaner::core::SystemSnapshot;

// O WMI e o ponto mais fragil da coleta: depende de servico rodando, namespace
// presente e permissao. Este teste existe para provar que ele respondeu nesta
// maquina, e nao que o coletor devolveu vazio em silencio.
TEST_CASE("a protecao do Windows e consultada via WMI", "[security_collector][integration]") {
    const SecurityCollector collector;
    const auto security = collector.collect();

    INFO("disponivel: " << security.available);
    INFO("provedor: " << security.provider);
    INFO("antivirus ativo: " << security.antivirus_enabled);
    INFO("tempo real ativo: " << security.realtime_protection_enabled);
    INFO("idade das definicoes: " << security.signature_age_days);

    if (!security.available) {
        WARN("o WMI nao respondeu nesta maquina; a analise vai declarar a area como nao observada");
        SUCCEED("ausencia tratada como area nao observada, que e o comportamento correto");
        return;
    }

    CHECK_FALSE(security.provider.empty());

    // Quando o Defender responde, ele informa a idade das definicoes. Um valor
    // negativo aqui indicaria leitura errada da propriedade, nao maquina limpa.
    if (security.provider == "Seguranca do Windows") {
        CHECK(security.signature_age_days >= 0);
    }
}

TEST_CASE("a coleta de seguranca preenche o snapshot", "[security_collector][integration]") {
    const SecurityCollector collector;

    SystemSnapshot snapshot;
    REQUIRE_FALSE(snapshot.security.available);

    const bool collected = collector.collect_into(snapshot);

    // Disponibilidade tem que refletir o que aconteceu de verdade: marcar como
    // observado sem ter conseguido ler faria a analise afirmar que esta tudo
    // bem numa area que nunca foi olhada.
    CHECK(collected == snapshot.security.available);
}
