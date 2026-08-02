#include "cleaners/cleaner_engine.hpp"

#include <windows.h>

#include <array>
#include <filesystem>

namespace cleaner::cleaners {

namespace {

using core::CleanerSpec;
using core::ContentClass;
using core::RiskLevel;

/// Resolve variaveis de ambiente do jeito largo.
///
/// A versao estreita quebra em perfil com acento — "C:\Users\João" — e esse
/// defeito ja apareceu varias vezes neste projeto.
std::string expand(const wchar_t* pattern) {
    std::array<wchar_t, 32767> buffer{};
    const DWORD written =
        ExpandEnvironmentStringsW(pattern, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (written == 0 || written > buffer.size()) {
        return {};
    }

    const std::wstring wide(buffer.data(), written - 1);

    const int length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                           nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), length,
                        nullptr, nullptr);
    return utf8;
}

/// Todas as pastas de perfil de um navegador baseado em Chromium que contenham
/// a subpasta pedida.
///
/// Enumera o que existe em vez de assumir "Default": quem usa mais de um perfil
/// teria a maior parte do cache ignorada.
std::vector<std::string> chromium_profile_folders(const wchar_t* user_data, const char* subfolder) {
    const auto base = expand(user_data);
    if (base.empty()) {
        return {};
    }

    std::vector<std::string> folders;
    std::error_code error;

    for (const auto& entry : std::filesystem::directory_iterator(base, error)) {
        if (!entry.is_directory(error)) {
            continue;
        }

        const auto name = entry.path().filename().string();
        const bool is_profile = name == "Default" || name.rfind("Profile ", 0) == 0;
        if (!is_profile) {
            continue;
        }

        auto candidate = entry.path() / subfolder;
        if (std::filesystem::is_directory(candidate, error)) {
            folders.push_back(candidate.string());
        }
    }

    return folders;
}

std::vector<std::string> existing(std::vector<std::string> candidates) {
    std::error_code error;
    std::erase_if(candidates, [&error](const std::string& path) {
        return path.empty() || !std::filesystem::is_directory(path, error);
    });
    return candidates;
}

}

std::vector<CleanerSpec> available_cleaners() {
    std::vector<CleanerSpec> cleaners;

    const auto add = [&cleaners](CleanerSpec spec) {
        if (!spec.allowed_roots.empty()) {
            cleaners.push_back(std::move(spec));
        }
    };

    add(CleanerSpec{
        .id = "user.temp",
        .display_name = "Arquivos temporarios dos seus programas",
        .description = "A pasta onde os programas guardam arquivos de trabalho passageiros.",
        .application = "Windows",
        .allowed_roots = existing({expand(L"%TEMP%")}),
        .content_class = ContentClass::SafeToClean,
        .risk = RiskLevel::Green,
        .confidence = 0.9,
        .consequence = "Nada. Programas recriam o que precisarem.",
        .preserved_description = "Arquivos em uso por programa aberto ficam onde estao.",
        .rule_version = 1,
    });

    add(CleanerSpec{
        .id = "windows.temp",
        .display_name = "Arquivos temporarios do Windows",
        .description = "A mesma pasta que a Limpeza de Disco do Windows esvazia.",
        .application = "Windows",
        .allowed_roots = existing({expand(L"%SystemRoot%\\Temp")}),
        .content_class = ContentClass::SafeToClean,
        .risk = RiskLevel::Green,
        .confidence = 0.9,
        .consequence = "Nada. O sistema recria o que precisar.",
        .preserved_description = "Arquivos em uso ficam onde estao.",
        .needs_admin = true,
        .rule_version = 1,
    });

    add(CleanerSpec{
        .id = "windows.thumbnails",
        .display_name = "Cache de miniaturas",
        .description = "As miniaturas que o Explorador mostra para fotos e videos.",
        .application = "Windows",
        .allowed_roots = existing({expand(L"%LOCALAPPDATA%\\Microsoft\\Windows\\Explorer")}),
        .content_class = ContentClass::SafeToClean,
        .risk = RiskLevel::Green,
        .confidence = 0.9,
        .consequence = "As miniaturas somem e sao refeitas conforme voce abre as pastas. Na "
                       "primeira vez cada pasta demora um pouco mais.",
        .preserved_description = "Nenhuma foto ou video e tocado: aqui so ha as miniaturas.",
        .rule_version = 1,
    });

    add(CleanerSpec{
        .id = "windows.crash-dumps",
        .display_name = "Relatorios de travamento",
        .description = "Despejos de memoria de programas que travaram.",
        .application = "Windows",
        .allowed_roots = existing({expand(L"%LOCALAPPDATA%\\CrashDumps")}),
        .content_class = ContentClass::SafeToClean,
        .risk = RiskLevel::Green,
        .confidence = 0.9,
        .consequence = "Perde o material para investigar travamentos passados.",
        .preserved_description = "Nada alem dos despejos.",
        .regenerates_itself = false,
        .rule_version = 1,
    });

    add(CleanerSpec{
        .id = "windows.wer",
        .display_name = "Relatorios de erro antigos",
        .description = "Relatorios que o Windows guardou quando algum programa travou.",
        .application = "Windows",
        .allowed_roots =
            existing({expand(L"%ProgramData%\\Microsoft\\Windows\\WER\\ReportArchive"),
                      expand(L"%LOCALAPPDATA%\\Microsoft\\Windows\\WER\\ReportArchive")}),
        .content_class = ContentClass::SafeToClean,
        .risk = RiskLevel::Green,
        .confidence = 0.9,
        .consequence = "Perde o historico de travamentos antigos.",
        .preserved_description = "Nada alem dos relatorios.",
        .regenerates_itself = false,
        .rule_version = 1,
    });

    add(CleanerSpec{
        .id = "windows.shader-cache",
        .display_name = "Cache de shaders",
        .description = "Shaders ja compilados pelo DirectX e pelo driver de video.",
        .application = "Windows e NVIDIA",
        .allowed_roots = existing({expand(L"%LOCALAPPDATA%\\D3DSCache"),
                                   expand(L"%LOCALAPPDATA%\\NVIDIA\\DXCache"),
                                   expand(L"%LOCALAPPDATA%\\NVIDIA\\GLCache")}),
        .content_class = ContentClass::SafeToClean,
        .risk = RiskLevel::Green,
        .confidence = 0.9,
        .consequence = "Os primeiros minutos de cada jogo podem ficar mais lentos, ate os shaders "
                       "serem compilados de novo.",
        .preserved_description = "Nenhuma configuracao de jogo e tocada.",
        .rule_version = 1,
    });

    // Navegadores: so as pastas de cache reconhecidas, dentro de cada perfil.
    // O perfil inteiro continua fora do alcance — senha, favorito, historico e
    // extensao moram nele.
    for (const auto& [id, name, user_data] :
         std::array<std::tuple<const char*, const char*, const wchar_t*>, 2>{{
             {"chrome.cache", "Google Chrome",
              L"%LOCALAPPDATA%\\Google\\Chrome\\User Data"},
             {"edge.cache", "Microsoft Edge", L"%LOCALAPPDATA%\\Microsoft\\Edge\\User Data"},
         }}) {
        std::vector<std::string> roots;
        for (const char* subfolder : {"Cache", "Code Cache", "GPUCache", "ShaderCache"}) {
            for (auto& folder : chromium_profile_folders(user_data, subfolder)) {
                roots.push_back(std::move(folder));
            }
        }

        add(CleanerSpec{
            .id = id,
            .display_name = std::string{"Cache do "} + name,
            .description = "Paginas, imagens e codigo guardados para os sites abrirem mais rapido.",
            .application = name,
            .allowed_roots = std::move(roots),
            .content_class = ContentClass::SafeToClean,
            .risk = RiskLevel::Green,
            .confidence = 0.9,
            .consequence = "Os sites carregam um pouco mais devagar na primeira visita depois da "
                           "limpeza.",
            .preserved_description = "Senhas, favoritos, historico, cookies e extensoes ficam. "
                                     "Este limpador so alcanca as pastas de cache de cada perfil.",
            .needs_app_closed = true,
            .rule_version = 1,
        });
    }

    add(CleanerSpec{
        .id = "discord.cache",
        .display_name = "Cache do Discord",
        .description = "Arquivos que o Discord guardou para abrir mais rapido.",
        .application = "Discord",
        .allowed_roots = existing({expand(L"%APPDATA%\\discord\\Cache"),
                                   expand(L"%APPDATA%\\discord\\Code Cache"),
                                   expand(L"%APPDATA%\\discord\\GPUCache")}),
        .content_class = ContentClass::SafeToClean,
        .risk = RiskLevel::Green,
        .confidence = 0.9,
        .consequence = "Nada. O programa recria conforme usa.",
        .preserved_description = "Nao perde conversa nem configuracao.",
        .needs_app_closed = true,
        .rule_version = 1,
    });

    add(CleanerSpec{
        .id = "python.uv-cache",
        .display_name = "Cache de pacotes Python (uv)",
        .description = "Pacotes Python que o uv guardou para nao baixar de novo.",
        .application = "Python (uv)",
        .allowed_roots = existing({expand(L"%LOCALAPPDATA%\\uv\\cache")}),
        .content_class = ContentClass::CleanWithConsequence,
        .risk = RiskLevel::Yellow,
        .confidence = 0.9,
        .consequence = "Nada permanente. O proximo comando que precisar de um pacote o baixa outra "
                       "vez, o que leva mais tempo e consome internet.",
        .preserved_description = "Nenhum ambiente virtual e nenhum projeto sao tocados.",
        .rule_version = 1,
    });

    add(CleanerSpec{
        .id = "python.pip-cache",
        .display_name = "Cache do pip",
        .description = "Pacotes Python que o pip guardou para nao baixar de novo.",
        .application = "Python (pip)",
        .allowed_roots = existing({expand(L"%LOCALAPPDATA%\\pip\\cache")}),
        .content_class = ContentClass::CleanWithConsequence,
        .risk = RiskLevel::Yellow,
        .confidence = 0.9,
        .consequence = "Voltam a ser baixados quando preciso.",
        .preserved_description = "Nenhum ambiente virtual e nenhum projeto sao tocados.",
        .rule_version = 1,
    });

    add(CleanerSpec{
        .id = "node.npm-cache",
        .display_name = "Cache do npm",
        .description = "Pacotes JavaScript que o npm guardou para nao baixar de novo.",
        .application = "npm",
        .allowed_roots = existing({expand(L"%LOCALAPPDATA%\\npm-cache")}),
        .content_class = ContentClass::CleanWithConsequence,
        .risk = RiskLevel::Yellow,
        .confidence = 0.9,
        .consequence = "Voltam a ser baixados quando preciso.",
        .preserved_description = "Nenhum node_modules de projeto e tocado.",
        .rule_version = 1,
    });

    add(CleanerSpec{
        .id = "vscode.cached-data",
        .display_name = "Cache do Visual Studio Code",
        .description = "Codigo do editor ja compilado e registros de funcionamento.",
        .application = "Visual Studio Code",
        .allowed_roots = existing({expand(L"%APPDATA%\\Code\\CachedData"),
                                   expand(L"%APPDATA%\\Code\\logs")}),
        .content_class = ContentClass::SafeToClean,
        .risk = RiskLevel::Green,
        .confidence = 0.9,
        .consequence = "Recriado na proxima abertura do editor.",
        .preserved_description = "Configuracoes, atalhos, trechos salvos e extensoes ficam.",
        .needs_app_closed = true,
        .rule_version = 1,
    });

    add(CleanerSpec{
        .id = "codex.temporary",
        .display_name = "Arquivos de trabalho temporarios do Codex",
        .description = "A pasta de temporarios que a propria ferramenta declara.",
        .application = "Codex",
        .allowed_roots = existing({expand(L"%USERPROFILE%\\.codex\\.tmp")}),
        .content_class = ContentClass::SafeToClean,
        .risk = RiskLevel::Green,
        .confidence = 0.85,
        .consequence = "Volta sozinho, sem custo perceptivel.",
        .preserved_description = "Nada do seu historico nem das suas configuracoes, que ficam em "
                                 "outras pastas.",
        .rule_version = 1,
    });

    add(CleanerSpec{
        .id = "cleaner.quarantine",
        .display_name = "Quarentena antiga do Cleaner",
        .description = "Arquivos guardados por versoes anteriores, de um recurso que nao existe "
                       "mais.",
        .application = "Cleaner",
        .allowed_roots = existing({expand(L"%LOCALAPPDATA%\\Cleaner\\quarentena")}),
        .content_class = ContentClass::SafeToClean,
        .risk = RiskLevel::Green,
        .confidence = 0.95,
        .consequence = "Nada que o computador use.",
        .preserved_description = "O historico e os retratos ficam: estao em outras pastas.",
        .regenerates_itself = false,
        .rule_version = 1,
    });

    return cleaners;
}

}
