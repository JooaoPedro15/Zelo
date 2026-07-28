#include <catch2/catch_test_macros.hpp>
#include <collectors/disk_collector.hpp>

using zelo::collectors::DiskCollector;
using zelo::core::SystemSnapshot;

TEST_CASE("os discos fisicos da maquina sao lidos", "[disk_collector][integration]") {
    const DiskCollector collector;
    const auto disks = collector.collect();

    INFO("disponivel: " << disks.available);
    INFO("discos encontrados: " << disks.disks.size());

    if (!disks.available) {
        WARN("o WMI de armazenamento nao respondeu; a area sera declarada como nao observada");
        SUCCEED("ausencia tratada como area nao observada");
        return;
    }

    REQUIRE_FALSE(disks.disks.empty());

    for (const auto& disk : disks.disks) {
        INFO("disco: " << disk.model << " | " << disk.media_type << " | " << disk.health_status
                       << " | erros leitura: " << disk.read_errors
                       << " | temperatura: " << disk.temperature_celsius);

        CHECK_FALSE(disk.model.empty());
        CHECK_FALSE(disk.health_status.empty());

        // Contador ausente tem que ficar negativo. Se virasse zero, a analise
        // leria "nenhum erro registrado" num disco que nunca foi medido.
        CHECK(disk.read_errors >= -1);
        CHECK(disk.write_errors >= -1);
    }
}

TEST_CASE("a coleta de discos reflete o que conseguiu ler", "[disk_collector][integration]") {
    const DiskCollector collector;

    SystemSnapshot snapshot;
    REQUIRE_FALSE(snapshot.disks.available);

    const bool collected = collector.collect_into(snapshot);

    CHECK(collected == snapshot.disks.available);
}
