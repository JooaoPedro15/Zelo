#include <catch2/catch_test_macros.hpp>
#include <collectors/volume_collector.hpp>

using cleaner::collectors::VolumeCollector;
using cleaner::core::SystemSnapshot;

TEST_CASE("os volumes fixos da maquina sao lidos", "[volume_collector][integration]") {
    const VolumeCollector collector;
    const auto volumes = collector.collect();

    REQUIRE_FALSE(volumes.empty());

    bool found_system_volume = false;
    for (const auto& volume : volumes) {
        INFO("volume " << volume.letter);

        CHECK_FALSE(volume.letter.empty());
        CHECK(volume.total_bytes > 0);
        CHECK(volume.free_bytes <= volume.total_bytes);

        const double ratio = volume.free_ratio();
        CHECK(ratio >= 0.0);
        CHECK(ratio <= 1.0);

        found_system_volume = found_system_volume || volume.is_system;
    }

    // Sem identificar o volume de sistema, a regra de pouco espaco livre nunca
    // dispararia — e ela e o principal motivo desta coleta existir.
    CHECK(found_system_volume);
}

TEST_CASE("a coleta marca o snapshot como disponivel", "[volume_collector][integration]") {
    const VolumeCollector collector;

    SystemSnapshot snapshot;
    REQUIRE_FALSE(snapshot.volumes_available);

    REQUIRE(collector.collect_into(snapshot));

    CHECK(snapshot.volumes_available);
    CHECK_FALSE(snapshot.volumes.empty());
}
