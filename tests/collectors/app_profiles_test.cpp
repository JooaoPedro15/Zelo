#include <catch2/catch_test_macros.hpp>
#include <collectors/app_profiles.hpp>
#include <core/rules/app_profile_rule.hpp>

#include <algorithm>
#include <string>

using zelo::collectors::app_profiles_catalog;
using zelo::core::AppProfile;
using zelo::core::AppProfileRule;
using zelo::core::ProfileFinding;
using zelo::core::ProfileItem;
using zelo::core::RiskLevel;
using zelo::core::SystemSnapshot;

namespace {

const ProfileItem* item_of(const AppProfile& profile, const std::string& relative) {
    const auto found = std::find_if(
        profile.items.begin(), profile.items.end(),
        [&relative](const ProfileItem& item) { return item.relative_path == relative; });
    return found == profile.items.end() ? nullptr : &*found;
}

const AppProfile* profile_of(const std::vector<AppProfile>& profiles, const std::string& id) {
    const auto found = std::find_if(profiles.begin(), profiles.end(),
                                    [&id](const AppProfile& profile) { return profile.id == id; });
    return found == profiles.end() ? nullptr : &*found;
}

SystemSnapshot snapshot_with(std::vector<ProfileFinding> findings) {
    SystemSnapshot snapshot;
    snapshot.profiles_available = true;
    snapshot.profile_findings = std::move(findings);
    return snapshot;
}

ProfileFinding finding_with(RiskLevel risk, std::uint64_t bytes) {
    ProfileFinding finding;
    finding.path = "C:\\app\\item";
    finding.application = "Programa";
    finding.size_bytes = bytes;
    finding.item.display_name = "Item";
    finding.item.risk = risk;
    return finding;
}

}

// O compromisso mais importante desta etapa. Sessao, conversa e contexto ficam
// na mesma pasta que o cache descartavel, e um catalogo que ofereca a pasta
// inteira levaria o historico junto com o lixo.
TEST_CASE("sessoes e contexto de ferramentas de IA sao sempre vermelhos", "[app_profiles]") {
    const auto profiles = app_profiles_catalog();

    const auto* codex = profile_of(profiles, "app.codex");
    REQUIRE(codex != nullptr);

    for (const auto& protegido : {"sessions", "sqlite", "skills"}) {
        const auto* item = item_of(*codex, protegido);
        INFO("item: " << protegido);
        REQUIRE(item != nullptr);
        CHECK(item->risk == RiskLevel::Red);
    }

    const auto* claude = profile_of(profiles, "app.claude");
    REQUIRE(claude != nullptr);

    for (const auto& protegido : {"local-agent-mode-sessions", "claude-code-sessions"}) {
        const auto* item = item_of(*claude, protegido);
        INFO("item: " << protegido);
        REQUIRE(item != nullptr);
        CHECK(item->risk == RiskLevel::Red);
    }
}

TEST_CASE("configuracoes do editor nunca sao oferecidas", "[app_profiles]") {
    const auto profiles = app_profiles_catalog();

    const auto* vscode = profile_of(profiles, "app.vscode");
    REQUIRE(vscode != nullptr);

    const auto* user = item_of(*vscode, "User");
    REQUIRE(user != nullptr);

    // Configuracoes, atalhos, snippets e estado dos projetos moram todos aqui.
    CHECK(user->risk == RiskLevel::Red);
}

// Login e credencial ficam nesses lugares em programas Electron. Trata-los como
// cache deslogaria o usuario sem aviso.
TEST_CASE("estado e credenciais de aplicativo sao vermelhos", "[app_profiles]") {
    const auto profiles = app_profiles_catalog();

    const auto* claude = profile_of(profiles, "app.claude");
    REQUIRE(claude != nullptr);

    for (const auto& protegido : {"Local Storage", "IndexedDB", "Local State", "Network"}) {
        const auto* item = item_of(*claude, protegido);
        INFO("item: " << protegido);
        REQUIRE(item != nullptr);
        CHECK(item->risk == RiskLevel::Red);
    }
}

// O erro classico de limpador: apagar a pasta do navegador inteira e levar
// senha, favorito e extensao junto com o cache.
TEST_CASE("senhas, favoritos e extensoes do navegador sao vermelhos", "[app_profiles]") {
    const auto profiles = app_profiles_catalog();

    const auto* chrome = profile_of(profiles, "app.chrome");
    REQUIRE(chrome != nullptr);

    for (const auto& protegido : {"Login Data", "Bookmarks", "History", "Web Data", "Extensions",
                                  "Local Extension Settings", "Network", "IndexedDB",
                                  "Local Storage", "Preferences"}) {
        const auto* item = item_of(*chrome, protegido);
        INFO("item: " << protegido);
        REQUIRE(item != nullptr);
        CHECK(item->risk == RiskLevel::Red);
    }
}

TEST_CASE("cache de navegacao e verde", "[app_profiles]") {
    const auto profiles = app_profiles_catalog();

    const auto* chrome = profile_of(profiles, "app.chrome");
    REQUIRE(chrome != nullptr);

    for (const auto& seguro : {"Cache", "Code Cache", "GPUCache"}) {
        const auto* item = item_of(*chrome, seguro);
        INFO("item: " << seguro);
        REQUIRE(item != nullptr);
        CHECK(item->risk == RiskLevel::Green);
    }
}

// Dentro de Service Worker convivem cache descartavel e o conteudo que faz um
// site funcionar offline. Nao separar os dois com seguranca e motivo para nao
// oferecer.
TEST_CASE("dados de site offline nao sao tratados como cache", "[app_profiles]") {
    const auto profiles = app_profiles_catalog();

    const auto* chrome = profile_of(profiles, "app.chrome");
    REQUIRE(chrome != nullptr);

    const auto* worker = item_of(*chrome, "Service Worker");
    REQUIRE(worker != nullptr);
    CHECK(worker->risk == RiskLevel::Unknown);
}

// Cache de midia volta sozinho; template e trabalho do usuario. Os dois moram
// na mesma pasta da Adobe.
TEST_CASE("cache da Adobe e verde, mas templates nao", "[app_profiles]") {
    const auto profiles = app_profiles_catalog();

    const auto* adobe = profile_of(profiles, "app.adobe");
    REQUIRE(adobe != nullptr);

    for (const auto& seguro : {"Media Cache Files", "Peak Files", "Metadata Cache"}) {
        const auto* item = item_of(*adobe, seguro);
        INFO("item: " << seguro);
        REQUIRE(item != nullptr);
        CHECK(item->risk == RiskLevel::Green);
    }

    const auto* templates = item_of(*adobe, "Motion Graphics Templates");
    REQUIRE(templates != nullptr);
    CHECK(templates->risk == RiskLevel::Red);
}

TEST_CASE("caches reconhecidos sao verdes", "[app_profiles]") {
    const auto profiles = app_profiles_catalog();

    const auto* vscode = profile_of(profiles, "app.vscode");
    REQUIRE(vscode != nullptr);

    for (const auto& seguro : {"CachedExtensionVSIXs", "GPUCache", "Crashpad"}) {
        const auto* item = item_of(*vscode, seguro);
        INFO("item: " << seguro);
        REQUIRE(item != nullptr);
        CHECK(item->risk == RiskLevel::Green);
    }
}

// Todo item do perfil precisa explicar o que e e o que se perde. Sem isso a
// interface exibiria um nome de pasta e pediria uma decisao as cegas.
TEST_CASE("todo item do perfil se explica", "[app_profiles]") {
    for (const auto& profile : app_profiles_catalog()) {
        for (const auto& item : profile.items) {
            INFO(profile.application << " / " << item.relative_path);
            CHECK_FALSE(item.display_name.empty());
            CHECK_FALSE(item.what_it_is.empty());
            CHECK_FALSE(item.what_you_lose.empty());
        }
    }
}

TEST_CASE("item vermelho ou desconhecido nao promete espaco recuperavel", "[app_profiles]") {
    const AppProfileRule rule;

    for (const auto risco : {RiskLevel::Red, RiskLevel::Unknown}) {
        const auto recommendations =
            rule.evaluate(snapshot_with({finding_with(risco, 500ULL * 1024 * 1024)}));

        REQUIRE(recommendations.size() == 1);

        const auto& recommendation = recommendations.front();
        CHECK(recommendation.reclaimable_bytes == 0);
        CHECK_FALSE(app_may_execute(recommendation));
    }
}

// Some-los da lista esconderia do usuario onde o espaco esta.
TEST_CASE("item sem acao ainda aparece na lista", "[app_profiles]") {
    const AppProfileRule rule;

    const auto recommendations =
        rule.evaluate(snapshot_with({finding_with(RiskLevel::Unknown, 900ULL * 1024 * 1024)}));

    REQUIRE(recommendations.size() == 1);
    CHECK_FALSE(recommendations.front().recommended_action.empty());
    CHECK_FALSE(recommendations.front().limitations.empty());
}

TEST_CASE("espaco recuperavel de perfil nao desconta da pontuacao", "[app_profiles]") {
    const AppProfileRule rule;

    const auto recommendations =
        rule.evaluate(snapshot_with({finding_with(RiskLevel::Green, 800ULL * 1024 * 1024)}));

    REQUIRE(recommendations.size() == 1);
    CHECK_FALSE(recommendations.front().counts_against_health);
}

TEST_CASE("sem coleta de perfis a regra nao conclui nada", "[app_profiles]") {
    const AppProfileRule rule;

    SystemSnapshot snapshot;
    snapshot.profiles_available = false;

    CHECK(rule.evaluate(snapshot).empty());
}
