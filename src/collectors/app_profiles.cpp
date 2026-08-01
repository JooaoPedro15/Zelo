#include "collectors/app_profiles.hpp"

#include "collectors/detail/text.hpp"

#include <scanner/storage_scanner.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <system_error>
#include <utility>

#include <windows.h>

namespace zelo::collectors {

namespace {

using core::AppProfile;
using core::ProfileItem;
using core::RegenerationCost;
using core::RiskLevel;

std::string expand(const std::string& text) {
    const auto wide = detail::to_wide(text);

    std::array<wchar_t, 32767> buffer{};
    const DWORD written =
        ::ExpandEnvironmentStringsW(wide.c_str(), buffer.data(), static_cast<DWORD>(buffer.size()));
    if (written == 0 || written > buffer.size()) {
        return {};
    }

    return detail::to_utf8(std::wstring_view(buffer.data(), written - 1));
}

/// Cache de aplicativo Electron. Aparece igual no Claude, no VS Code e em
/// qualquer outro construido sobre a mesma base.
std::vector<ProfileItem> electron_caches() {
    return {
        {"Cache", "Cache da interface", "Paginas e recursos que o programa guardou para abrir mais rapido.",
         "Nada permanente; e refeito conforme o programa e usado.", RiskLevel::Green,
         RegenerationCost::NeedsDownload},
        {"Code Cache", "Cache de codigo compilado",
         "Resultado da compilacao do proprio programa, guardado para iniciar mais rapido.",
         "A primeira abertura depois da limpeza fica um pouco mais lenta.", RiskLevel::Green,
         RegenerationCost::NeedsRework},
        {"GPUCache", "Cache da placa de video",
         "Resultado de trabalho grafico ja feito, guardado para nao repetir.",
         "E refeito no proximo uso.", RiskLevel::Green, RegenerationCost::NeedsRework},
        {"DawnGraphiteCache", "Cache grafico", "Mesma coisa, de outro subsistema grafico.",
         "E refeito no proximo uso.", RiskLevel::Green, RegenerationCost::NeedsRework},
        {"DawnWebGPUCache", "Cache grafico", "Mesma coisa, de outro subsistema grafico.",
         "E refeito no proximo uso.", RiskLevel::Green, RegenerationCost::NeedsRework},
        {"logs", "Registros de funcionamento",
         "Anotacoes que o programa faz enquanto roda, uteis para investigar problema.",
         "O historico de funcionamento. Nada deixa de funcionar.", RiskLevel::Green,
         RegenerationCost::Permanent},
    };
}

/// Dados de sessao e estado de aplicativo Electron. Sao os que nunca podem ser
/// tratados como cache, ainda que fiquem lado a lado com ele.
std::vector<ProfileItem> electron_state() {
    return {
        {"Local Storage", "Dados guardados pelo programa",
         "Preferencias e informacoes que o programa gravou sobre o seu uso.",
         "Configuracoes e, dependendo do programa, o login.", RiskLevel::Red,
         RegenerationCost::Permanent},
        {"Session Storage", "Dados da sessao atual", "Estado do que esta aberto agora.",
         "O que estava em andamento.", RiskLevel::Red, RegenerationCost::Permanent},
        {"IndexedDB", "Banco de dados local",
         "Onde o programa guarda dados estruturados — incluindo, em muitos casos, o seu conteudo.",
         "Dados que so existem ali.", RiskLevel::Red, RegenerationCost::Permanent},
        {"Local State", "Estado do programa", "Arquivo de configuracao principal.",
         "Configuracoes e, possivelmente, credenciais.", RiskLevel::Red,
         RegenerationCost::Permanent},
        {"Network", "Dados de rede", "Cookies e informacoes de conexao, incluindo autenticacao.",
         "O login. Voce precisaria entrar de novo.", RiskLevel::Red, RegenerationCost::Permanent},
        {"Partitions", "Areas isoladas do programa",
         "Dados de partes do programa que rodam separadas.",
         "Depende do programa; pode incluir login e conteudo.", RiskLevel::Red,
         RegenerationCost::Permanent},
    };
}

void append(std::vector<ProfileItem>& target, std::vector<ProfileItem> extra) {
    target.insert(target.end(), std::make_move_iterator(extra.begin()),
                  std::make_move_iterator(extra.end()));
}

AppProfile vscode_profile() {
    AppProfile profile;
    profile.id = "app.vscode";
    profile.application = "Visual Studio Code";
    profile.root = expand("%APPDATA%\\Code");

    profile.items = {
        {"CachedExtensionVSIXs", "Instaladores de extensao ja usados",
         "Os pacotes de instalacao das extensoes que voce ja instalou. Ficam guardados depois da "
         "instalacao terminar.",
         "Nada. As extensoes continuam instaladas e funcionando; so os instaladores somem.",
         RiskLevel::Green, RegenerationCost::NeedsDownload},
        {"CachedData", "Dados temporarios do editor",
         "Resultado de trabalho que o editor guardou para repetir menos.",
         "Nada permanente; e refeito conforme voce usa.", RiskLevel::Green,
         RegenerationCost::NeedsRework},
        {"CachedProfilesData", "Dados temporarios de perfis",
         "Mesma coisa, para os perfis do editor.", "Nada permanente.", RiskLevel::Green,
         RegenerationCost::NeedsRework},
        {"Crashpad", "Relatorios de travamento",
         "O que o editor gravou quando fechou sozinho, para analise tecnica.",
         "O material para investigar travamentos passados.", RiskLevel::Green,
         RegenerationCost::Permanent},

        // A pasta que nunca pode ser oferecida. Fica explicita no perfil, com
        // risco vermelho, em vez de simplesmente ausente: assim ela aparece na
        // interface com a explicacao de por que nao ha acao.
        {"User", "Suas configuracoes do editor",
         "Onde ficam configuracoes, atalhos de teclado, snippets, estado dos projetos abertos e "
         "os dados das extensoes — inclusive conversas de extensoes de IA.",
         "Tudo que voce personalizou no editor, e o historico das extensoes.", RiskLevel::Red,
         RegenerationCost::Permanent},
    };

    append(profile.items, electron_caches());
    append(profile.items, electron_state());
    return profile;
}

AppProfile codex_profile() {
    AppProfile profile;
    profile.id = "app.codex";
    profile.application = "Codex";
    profile.root = expand("%USERPROFILE%\\.codex");

    profile.items = {
        {".tmp", "Arquivos de trabalho temporarios",
         "Arquivos que o Codex cria durante o uso e nao precisa depois.",
         "Nada do seu historico nem das suas configuracoes, que ficam em outras pastas.",
         RiskLevel::Green, RegenerationCost::Free},
        {"cache", "Cache do Codex",
         "Resultado de trabalho ja feito, guardado para nao repetir.",
         "Nada permanente.", RiskLevel::Green, RegenerationCost::NeedsRework},

        // Estes tres sao a razao de este perfil existir. Um catalogo que
        // oferecesse ".codex" inteiro levaria o historico junto com o lixo.
        {"sessions", "Suas sessoes de trabalho",
         "O registro das suas conversas e do que foi feito em cada sessao.",
         "O historico completo do seu trabalho com a ferramenta. Nao ha como recuperar.",
         RiskLevel::Red, RegenerationCost::Permanent},
        {"sqlite", "Banco de dados local",
         "Onde a ferramenta guarda o que aprendeu sobre os seus projetos.",
         "Contexto acumulado que so existe ali.", RiskLevel::Red, RegenerationCost::Permanent},
        {"skills", "Suas habilidades configuradas",
         "Instrucoes e comportamentos que voce configurou.", "O que voce personalizou.",
         RiskLevel::Red, RegenerationCost::Permanent},
    };

    return profile;
}

AppProfile claude_profile() {
    AppProfile profile;
    profile.id = "app.claude";
    profile.application = "Claude";
    profile.root = expand("%APPDATA%\\Claude");

    profile.items = {
        // Sessao e conversa nunca sao cache, por mais que fiquem ao lado dele.
        {"local-agent-mode-sessions", "Suas sessoes de trabalho",
         "O registro das sessoes de trabalho com o agente.",
         "O historico dessas sessoes. Nao ha como recuperar.", RiskLevel::Red,
         RegenerationCost::Permanent},
        {"claude-code-sessions", "Suas sessoes do Claude Code",
         "O registro das sessoes de linha de comando.", "O historico dessas sessoes.",
         RiskLevel::Red, RegenerationCost::Permanent},
        {"ChromeNativeHost", "Ligacao com o navegador",
         "Componente que permite ao programa conversar com a extensao do navegador.",
         "A integracao com o navegador ate ser reinstalada.", RiskLevel::Red,
         RegenerationCost::Permanent},
    };

    append(profile.items, electron_caches());
    append(profile.items, electron_state());
    return profile;
}

}

std::vector<core::AppProfile> app_profiles_catalog() {
    std::vector<core::AppProfile> profiles{vscode_profile(), codex_profile(), claude_profile()};

    std::erase_if(profiles, [](const AppProfile& profile) { return profile.root.empty(); });
    return profiles;
}

AppProfileCollector::AppProfileCollector(core::ProtectedPaths protected_paths)
    : protected_paths_(std::move(protected_paths)) {}

std::vector<core::ProfileFinding> AppProfileCollector::collect(std::stop_token token) const {
    const scanner::StorageScanner storage_scanner{
        scanner::ScanOptions{.largest_files_kept = 1, .rollup_depth = 0}};

    std::vector<core::ProfileFinding> findings;

    for (const auto& profile : app_profiles_catalog()) {
        std::error_code error;
        if (!std::filesystem::is_directory(profile.root, error)) {
            continue;
        }

        // Enumera o que existe de fato, em vez de procurar o que a tabela
        // espera. E o que faz o desconhecido aparecer: uma pasta nova de uma
        // versao nova do programa nao passa despercebida.
        for (const auto& entry : std::filesystem::directory_iterator(profile.root, error)) {
            if (token.stop_requested()) {
                return findings;
            }

            const auto name = entry.path().filename().string();

            const auto known = std::find_if(
                profile.items.begin(), profile.items.end(), [&name](const core::ProfileItem& item) {
                    return detail::find_ignoring_case(item.relative_path, name) == 0 &&
                           item.relative_path.size() == name.size();
                });

            core::ProfileFinding finding;
            finding.path = entry.path().string();
            finding.application = profile.application;

            if (known != profile.items.end()) {
                finding.item = *known;
            } else {
                // Nome nao reconhecido. Nao ha palpite por semelhanca: uma
                // pasta chamada "cache" pode guardar qualquer coisa, e apagar
                // pelo nome e como limpadores destroem dados.
                finding.item = core::ProfileItem{
                    .relative_path = name,
                    .display_name = name,
                    .what_it_is = "O Zelo nao reconhece este item. Ele pode ter surgido numa versao "
                                  "nova do programa.",
                    .what_you_lose = "Desconhecido — e justamente por isso nao ha acao disponivel.",
                    .risk = RiskLevel::Unknown,
                    .regeneration = RegenerationCost::Permanent,
                };
            }

            if (entry.is_directory(error)) {
                const auto result = storage_scanner.scan(entry.path(), token);
                if (!result.completed) {
                    continue;
                }
                finding.size_bytes = result.allocated_bytes;
            } else {
                finding.size_bytes = std::filesystem::file_size(entry.path(), error);
                if (error) {
                    continue;
                }
            }

            findings.push_back(std::move(finding));
        }
    }

    return findings;
}

bool AppProfileCollector::collect_into(core::SystemSnapshot& snapshot,
                                       std::stop_token token) const {
    snapshot.profile_findings = collect(token);
    snapshot.profiles_available = true;
    return true;
}

}
