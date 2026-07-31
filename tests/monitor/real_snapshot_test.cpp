#include <catch2/catch_test_macros.hpp>
#include <monitor/snapshot_taker.hpp>

#include <chrono>

using zelo::monitor::SnapshotOptions;
using zelo::monitor::SnapshotTaker;

// Marcado com ponto: nao roda na suite normal, porque percorre o disco inteiro.
// Serve para medir o custo real antes de decidir quando o retrato e tirado.
TEST_CASE("retrato do disco de sistema", "[.][retrato_real]") {
    const SnapshotTaker taker;

    const auto started = std::chrono::steady_clock::now();
    const auto snapshot = taker.take("C:\\", "C:");
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - started);

    WARN("tempo: " << elapsed.count() << " s");
    WARN("pastas guardadas: " << snapshot.folders.size());
    WARN("arquivos grandes: " << snapshot.files.size());
    WARN("completo: " << snapshot.complete);

    CHECK(snapshot.complete);
    CHECK_FALSE(snapshot.folders.empty());
}
