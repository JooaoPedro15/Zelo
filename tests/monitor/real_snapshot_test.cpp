#include <catch2/catch_test_macros.hpp>
#include <monitor/snapshot_taker.hpp>

#include <array>

#include <chrono>

#include <windows.h>

using cleaner::monitor::SnapshotOptions;
using cleaner::monitor::SnapshotTaker;

namespace {

/// Le a variavel em wide. A versao estreita devolve o caminho em ANSI, e
/// construir um `path` com "C:\Users\João" assim falha com sequencia ilegal —
/// a mesma armadilha ja encontrada no log, na expansao de variaveis e no
/// registro da quarentena.
std::filesystem::path local_appdata() {
    std::array<wchar_t, 32767> buffer{};

    const DWORD written = ::GetEnvironmentVariableW(L"LOCALAPPDATA", buffer.data(),
                                                    static_cast<DWORD>(buffer.size()));
    if (written == 0 || written >= buffer.size()) {
        return {};
    }
    return std::filesystem::path{std::wstring(buffer.data(), written)};
}

}

// Uma arvore de verdade, com tudo que so aparece fora do laboratorio: caminho
// longo, pasta sem permissao, link, arquivo em uso. Vale mais que qualquer
// fixture — e cabe em segundos.
//
// Varrer o disco inteiro ficou de fora da suite. A medida que interessava ja foi
// feita (221 s e 45.730 pastas nesta maquina, o que definiu o retrato como acao
// pedida pelo usuario em vez de automatica), e um teste de quatro minutos deixa
// de ser rodado a cada mudanca — justamente quando ele serviria.
TEST_CASE("o retrato funciona numa arvore real do sistema", "[retrato_real][integration]") {
    const auto root = local_appdata();
    if (root.empty() || !std::filesystem::is_directory(root)) {
        SUCCEED("sem diretorio de dados local; teste ignorado");
        return;
    }

    const SnapshotTaker taker;

    const auto started = std::chrono::steady_clock::now();
    const auto snapshot = taker.take(root, "C:");
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - started);

    INFO("tempo: " << elapsed.count() << " s");
    INFO("pastas guardadas: " << snapshot.folders.size());

    CHECK(snapshot.complete);
    CHECK_FALSE(snapshot.folders.empty());

    // Acesso negado e caminho longo nao podem derrubar a varredura: numa arvore
    // desse tamanho os dois aparecem.
    CHECK(snapshot.total_bytes > 0);
}
