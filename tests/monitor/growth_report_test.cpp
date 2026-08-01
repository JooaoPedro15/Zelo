#include <catch2/catch_test_macros.hpp>
#include <monitor/growth_report.hpp>

#include <algorithm>
#include <string>

using cleaner::monitor::FolderChange;
using cleaner::monitor::GrowthItem;
using cleaner::monitor::SnapshotDiff;
using cleaner::monitor::build_growth_report;

namespace {

constexpr std::int64_t kMegabyte = 1024LL * 1024LL;

SnapshotDiff diff_with(std::vector<FolderChange> changes) {
    SnapshotDiff diff;
    diff.from_taken_at = "2026-07-30T10:00:00";
    diff.to_taken_at = "2026-07-31T10:00:00";
    diff.changes = std::move(changes);
    return diff;
}

FolderChange grew(const std::string& path, std::int64_t megabytes) {
    return FolderChange{.path = path,
                        .before_bytes = 0,
                        .after_bytes = static_cast<std::uint64_t>(megabytes * kMegabyte),
                        .delta_bytes = megabytes * kMegabyte};
}

const GrowthItem* find(const std::vector<GrowthItem>& items, const std::string& path) {
    const auto found = std::find_if(items.begin(), items.end(),
                                    [&path](const auto& item) { return item.path == path; });
    return found == items.end() ? nullptr : &*found;
}

}

// O problema central do relatorio. Uma pasta que cresce faz todos os seus pais
// crescerem junto: somar as linhas daria varias vezes o espaco que sumiu de
// verdade, e o usuario procuraria gigabytes que nunca existiram.
TEST_CASE("crescimento do filho nao e contado de novo no pai", "[growth_report]") {
    const auto report = build_growth_report(diff_with({
        grew("C:\\Users\\Joao\\AppData", 2000),
        grew("C:\\Users\\Joao\\AppData\\Local", 2000),
        grew("C:\\Users\\Joao\\AppData\\Local\\Google", 2000),
    }));

    // A soma do relatorio bate com o que realmente cresceu.
    std::int64_t total = 0;
    for (const auto& item : report.items) {
        total += item.exclusive_bytes;
    }
    CHECK(total == 2000 * kMegabyte);

    // A pasta que explica o crescimento e a mais funda; os pais nao repetem.
    const auto* google = find(report.items, "C:\\Users\\Joao\\AppData\\Local\\Google");
    REQUIRE(google != nullptr);
    CHECK(google->exclusive_bytes == 2000 * kMegabyte);

    // Os pais nao repetem o numero: o crescimento deles ja esta explicado.
    CHECK(find(report.items, "C:\\Users\\Joao\\AppData") == nullptr);
    CHECK(find(report.items, "C:\\Users\\Joao\\AppData\\Local") == nullptr);
}

TEST_CASE("pastas irmas aparecem separadas", "[growth_report]") {
    const auto report = build_growth_report(diff_with({
        grew("C:\\raiz", 3000),
        grew("C:\\raiz\\a", 2000),
        grew("C:\\raiz\\b", 1000),
    }));

    const auto* a = find(report.items, "C:\\raiz\\a");
    const auto* b = find(report.items, "C:\\raiz\\b");

    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    CHECK(a->exclusive_bytes == 2000 * kMegabyte);
    CHECK(b->exclusive_bytes == 1000 * kMegabyte);

    // A raiz cresceu exatamente o que as filhas explicam, entao nao sobra nada
    // para ela reivindicar — e uma linha dizendo "cresceu 0" seria so ruido.
    CHECK(find(report.items, "C:\\raiz") == nullptr);
}

// Pasta que cresceu mais do que a soma das filhas guarda a diferenca: sao
// arquivos soltos nela.
TEST_CASE("pai guarda o que as filhas nao explicam", "[growth_report]") {
    const auto report = build_growth_report(diff_with({
        grew("C:\\raiz", 1000),
        grew("C:\\raiz\\a", 400),
    }));

    const auto* raiz = find(report.items, "C:\\raiz");
    const auto* filha = find(report.items, "C:\\raiz\\a");

    REQUIRE(raiz != nullptr);
    REQUIRE(filha != nullptr);
    CHECK(raiz->exclusive_bytes == 600 * kMegabyte);
    CHECK(filha->exclusive_bytes == 400 * kMegabyte);
}

TEST_CASE("as maiores aparecem primeiro", "[growth_report]") {
    const auto report = build_growth_report(diff_with({
        grew("C:\\pequena", 10),
        grew("C:\\enorme", 5000),
        grew("C:\\media", 500),
    }));

    REQUIRE(report.items.size() >= 3);
    CHECK(report.items[0].path == "C:\\enorme");
    CHECK(report.items[1].path == "C:\\media");
}

// Pasta que encolheu explica espaco recuperado e nao pode virar crescimento.
TEST_CASE("reducao aparece com sinal proprio", "[growth_report]") {
    SnapshotDiff diff = diff_with({});
    diff.changes.push_back(FolderChange{.path = "C:\\encolheu",
                                        .before_bytes = 1000 * static_cast<std::uint64_t>(kMegabyte),
                                        .after_bytes = 0,
                                        .delta_bytes = -1000 * kMegabyte,
                                        .disappeared = true});

    const auto report = build_growth_report(diff);

    REQUIRE(report.shrunk.size() == 1);
    CHECK(report.shrunk.front().exclusive_bytes == -1000 * kMegabyte);
    CHECK(report.items.empty());
}

TEST_CASE("ruido pequeno fica de fora", "[growth_report]") {
    const auto report = build_growth_report(diff_with({
        grew("C:\\relevante", 500),
        grew("C:\\ruido", 1),
    }));

    CHECK(find(report.items, "C:\\relevante") != nullptr);
    CHECK(find(report.items, "C:\\ruido") == nullptr);
}
