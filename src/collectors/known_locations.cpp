#include "collectors/known_locations.hpp"

#include "collectors/detail/text.hpp"

#include <scanner/storage_scanner.hpp>

#include <filesystem>

#include <array>
#include <system_error>
#include <utility>

#include <windows.h>

namespace cleaner::collectors {

namespace {

using core::KnownLocation;
using core::RegenerationCost;
using core::RiskLevel;

/// Os caminhos do catalogo sao ASCII, mas a expansao devolve o perfil do
/// usuario, que frequentemente nao e — "C:\Users\João". Converter byte a byte
/// produziria UTF-8 invalido, e construir um `path` com isso lanca excecao.
std::string expand(const char* text) {
    const std::wstring pattern = detail::to_wide(text);

    std::array<wchar_t, 32767> buffer{};
    const DWORD written = ::ExpandEnvironmentStringsW(pattern.c_str(), buffer.data(),
                                                      static_cast<DWORD>(buffer.size()));
    if (written == 0 || written > buffer.size()) {
        return {};
    }

    return detail::to_utf8(std::wstring_view(buffer.data(), written - 1));
}

/// Uma entrada do catalogo, antes de resolver o caminho.
struct CatalogEntry {
    const char* id;
    const char* display_name;
    const char* path;
    const char* owner;
    const char* what_it_is;
    const char* what_you_lose;
    RegenerationCost regeneration;
    RiskLevel risk;
};

/// Cada entrada foi escrita olhando o que aquela pasta realmente guarda. Onde
/// nao houve certeza sobre o conteudo, a entrada simplesmente nao existe: e
/// melhor deixar espaco na mesa do que sugerir apagar algo sem saber o que e.
/// As pastas de temporarios do sistema nao entram aqui: elas tem coletor
/// proprio, que sabe descobrir onde o Windows as coloca. Repeti-las produziria
/// dois achados para os mesmos arquivos e um total somado em dobro.
const std::array<CatalogEntry, 29>& catalog() {
    static const std::array<CatalogEntry, 29> entries{{
        // --- O proprio Cleaner ---
        //
        // A quarentena foi removida do produto: limpar passou a apagar de vez.
        // O que ficou guardado antes disso continua ocupando disco sem servir
        // para nada, e sem aparecer em lugar nenhum — a varredura exclui a
        // pasta do proprio programa. Um limpador que esconde os proprios restos
        // e o pior caso possivel aqui.
        {"cleaner.quarantine", "Quarentena antiga do Cleaner",
         "%LOCALAPPDATA%\\Cleaner\\quarentena", "Cleaner",
         "Arquivos que versoes anteriores do Cleaner moveram para uma quarentena que nao existe "
         "mais. Eles nao voltam sozinhos para lugar nenhum.",
         "Nada que o computador use. Sao sobras que ja tinham sido aprovadas para remocao.",
         RegenerationCost::Permanent, RiskLevel::Green},

        {"cleaner.quarantine.legacy", "Quarentena do nome anterior",
         "%LOCALAPPDATA%\\Zelo\\quarentena", "Cleaner",
         "A mesma quarentena, guardada quando o programa ainda se chamava Zelo.",
         "Nada que o computador use. Sao sobras que ja tinham sido aprovadas para remocao.",
         RegenerationCost::Permanent, RiskLevel::Green},

        // --- Windows ---
        {"windows.update.download", "Instaladores de atualizacao ja aplicados",
         "%SystemRoot%\\SoftwareDistribution\\Download", "Windows Update",
         "Os pacotes que o Windows baixou para se atualizar. Depois de instalados, ficam ali sem "
         "utilidade.",
         "Nada. Se o Windows precisar de novo, ele baixa.", RegenerationCost::NeedsDownload,
         RiskLevel::Green},

        {"windows.thumbnails", "Cache de miniaturas",
         "%LOCALAPPDATA%\\Microsoft\\Windows\\Explorer", "Windows",
         "As miniaturas que o Explorador mostra para fotos e videos.",
         "As miniaturas somem e sao refeitas conforme voce abre as pastas. Na primeira vez cada "
         "pasta demora um pouco mais.",
         RegenerationCost::Free, RiskLevel::Green},

        {"windows.wer", "Relatorios de erro antigos",
         "%ProgramData%\\Microsoft\\Windows\\WER\\ReportArchive", "Windows",
         "Relatorios que o Windows guardou quando algum programa travou.",
         "O historico de travamentos antigos. O Cleaner usa o registro de eventos, nao estes "
         "arquivos, entao a analise nao perde nada.",
         RegenerationCost::Permanent, RiskLevel::Green},

        // --- Caches de desenvolvimento ---
        {"dev.uv", "Cache de pacotes Python (uv)", "%LOCALAPPDATA%\\uv\\cache", "uv",
         "Copias dos pacotes Python que o uv ja baixou, guardadas para nao precisar baixar de "
         "novo em cada projeto.",
         "Nada permanente. O proximo comando que precisar de um pacote o baixa outra vez, o que "
         "leva mais tempo e consome internet.",
         RegenerationCost::NeedsDownload, RiskLevel::Green},

        {"dev.pip", "Cache do pip", "%LOCALAPPDATA%\\pip\\cache", "pip",
         "O mesmo, para pacotes instalados pelo pip.",
         "Nada permanente; volta a ser baixado quando preciso.", RegenerationCost::NeedsDownload,
         RiskLevel::Green},

        {"dev.npm", "Cache do npm", "%LOCALAPPDATA%\\npm-cache", "npm",
         "Copias dos pacotes JavaScript ja baixados.",
         "Nada permanente; volta a ser baixado quando preciso.", RegenerationCost::NeedsDownload,
         RiskLevel::Green},

        {"dev.huggingface", "Modelos de IA baixados", "%USERPROFILE%\\.cache\\huggingface",
         "Hugging Face",
         "Modelos de inteligencia artificial que algum programa ou script baixou. Costumam ser "
         "arquivos grandes, de centenas de megabytes cada.",
         "Os modelos em si. Nada quebra, mas na proxima vez que um deles for usado ele sera "
         "baixado de novo — e pode ser um download demorado.",
         RegenerationCost::NeedsDownload, RiskLevel::Yellow},

        // A area do Codex saiu daqui: o perfil do aplicativo classifica cada
        // pasta separadamente, e manter a entrada no catalogo produzia dois
        // achados para o mesmo caminho, com riscos diferentes. Sobra o cache de
        // runtimes, que fica fora da area do programa.
        {"dev.codex.runtimes", "Runtimes baixados pelo Codex",
         "%USERPROFILE%\\.cache\\codex-runtimes", "Codex",
         "Ambientes de execucao que o Codex baixou para rodar codigo.",
         "Serao baixados de novo quando forem necessarios.", RegenerationCost::NeedsDownload,
         RiskLevel::Yellow},

        // --- Caches de aplicativos ---
        {"app.nvidia.dx", "Cache de shaders (DirectX)",
         "%LOCALAPPDATA%\\NVIDIA\\DXCache", "NVIDIA",
         "Shaders ja compilados dos jogos que voce jogou, guardados para o jogo carregar mais "
         "rapido.",
         "Os primeiros minutos de cada jogo podem ficar mais lentos, ate os shaders serem "
         "compilados de novo.",
         RegenerationCost::NeedsRework, RiskLevel::Green},

        {"app.nvidia.gl", "Cache de shaders (OpenGL)", "%LOCALAPPDATA%\\NVIDIA\\GLCache", "NVIDIA",
         "O mesmo, para jogos e programas que usam OpenGL.",
         "Mesma coisa: os shaders sao recompilados no proximo uso.", RegenerationCost::NeedsRework,
         RiskLevel::Green},

        {"app.chrome.cache", "Cache do Chrome",
         "%LOCALAPPDATA%\\Google\\Chrome\\User Data\\Default\\Cache", "Google Chrome",
         "Paginas e imagens guardadas para os sites abrirem mais rapido.",
         "Nao perde senha, favorito nem historico. Os sites so carregam um pouco mais devagar na "
         "primeira visita depois da limpeza.",
         RegenerationCost::NeedsDownload, RiskLevel::Green},

        {"app.edge.cache", "Cache do Edge",
         "%LOCALAPPDATA%\\Microsoft\\Edge\\User Data\\Default\\Cache", "Microsoft Edge",
         "O mesmo, para o Edge.",
         "Nao perde senha, favorito nem historico.", RegenerationCost::NeedsDownload,
         RiskLevel::Green},

        {"app.directx.shader", "Cache de shaders do Windows",
         "%LOCALAPPDATA%\\D3DSCache", "Windows",
         "Shaders compilados que o proprio Windows guarda.",
         "Sao recompilados no proximo uso.", RegenerationCost::NeedsRework, RiskLevel::Green},

        // --- Mais locais do Windows ---
        {"windows.delivery", "Cache de compartilhamento de atualizacoes",
         "%SystemRoot%\\SoftwareDistribution\\DeliveryOptimization", "Windows Update",
         "Pedacos de atualizacao que o Windows guardou para compartilhar com outros computadores "
         "da rede.",
         "Nada. Se precisar, o Windows baixa de novo.", RegenerationCost::NeedsDownload,
         RiskLevel::Green},

        {"windows.logs.cbs", "Registros de manutencao do Windows",
         "%SystemRoot%\\Logs\\CBS", "Windows",
         "Anotacoes que o Windows faz enquanto instala e repara os proprios componentes.",
         "O historico dessas operacoes. Ferramentas de reparo escrevem um registro novo quando "
         "rodam, entao nada deixa de funcionar.",
         RegenerationCost::Permanent, RiskLevel::Green},

        {"windows.panther", "Registros de instalacao do Windows",
         "%SystemRoot%\\Panther", "Windows",
         "Anotacoes da instalacao ou da ultima grande atualizacao do Windows.",
         "O historico da instalacao. Util para investigar um problema de atualizacao; sem "
         "utilidade no dia a dia.",
         RegenerationCost::Permanent, RiskLevel::Yellow},

        {"windows.downloaded.installations", "Instaladores antigos do Windows",
         "%SystemRoot%\\Downloaded Installations", "Windows",
         "Instaladores que programas deixaram para tras depois de instalados.",
         "A possibilidade de reparar alguns programas sem baixar o instalador de novo.",
         RegenerationCost::NeedsDownload, RiskLevel::Yellow},

        {"windows.fontcache", "Cache de fontes",
         "%SystemRoot%\\ServiceProfiles\\LocalService\\AppData\\Local\\FontCache", "Windows",
         "Fontes ja processadas, guardadas para os textos aparecerem mais rapido.",
         "As fontes em si continuam instaladas. O sistema refaz o cache sozinho.",
         RegenerationCost::Free, RiskLevel::Green},

        {"windows.crashdumps", "Relatorios de travamento",
         "%LOCALAPPDATA%\\CrashDumps", "Windows",
         "Arquivos que o Windows grava quando um programa fecha sozinho, para analise tecnica.",
         "O material para investigar travamentos passados. Costumam ser grandes e so servem a "
         "quem for depurar o programa.",
         RegenerationCost::Permanent, RiskLevel::Green},

        {"windows.inetcache.low", "Arquivos temporarios de internet",
         "%LOCALAPPDATA%\\Microsoft\\Windows\\INetCache", "Windows",
         "Paginas e imagens guardadas por componentes do Windows que abrem conteudo da internet.",
         "Nada permanente; sao baixados de novo quando preciso.", RegenerationCost::NeedsDownload,
         RiskLevel::Green},

        // --- Aplicativos comuns ---
        {"app.discord.cache", "Cache do Discord", "%APPDATA%\\discord\\Cache", "Discord",
         "Imagens e arquivos que o Discord guardou para carregar mais rapido.",
         "Nao perde conversa nem configuracao.", RegenerationCost::NeedsDownload, RiskLevel::Green},

        {"app.spotify.cache", "Cache do Spotify", "%LOCALAPPDATA%\\Spotify\\Data", "Spotify",
         "Musicas guardadas para tocar sem baixar de novo.",
         "Nao perde playlist nem login. As musicas sao baixadas outra vez ao tocar.",
         RegenerationCost::NeedsDownload, RiskLevel::Green},

        {"app.teams.cache", "Cache do Teams",
         "%APPDATA%\\Microsoft\\Teams\\Cache", "Microsoft Teams",
         "Arquivos que o Teams guardou para abrir mais rapido.",
         "Nao perde conversa nem configuracao.", RegenerationCost::NeedsDownload, RiskLevel::Green},

        {"dev.gradle", "Cache do Gradle", "%USERPROFILE%\\.gradle\\caches", "Gradle",
         "Bibliotecas e resultados de compilacao que o Gradle guardou entre projetos.",
         "Nada permanente. A proxima compilacao baixa e refaz o que precisar, e demora mais.",
         RegenerationCost::NeedsRework, RiskLevel::Yellow},

        {"dev.nuget", "Cache do NuGet", "%USERPROFILE%\\.nuget\\packages", "NuGet",
         "Pacotes .NET ja baixados, guardados para nao repetir o download em cada projeto.",
         "Nada permanente; voltam a ser baixados quando um projeto precisar.",
         RegenerationCost::NeedsDownload, RiskLevel::Yellow},

        {"dev.cargo", "Cache do Cargo (Rust)", "%USERPROFILE%\\.cargo\\registry", "Cargo",
         "Copias das bibliotecas Rust ja baixadas.",
         "Nada permanente; voltam a ser baixadas quando preciso.", RegenerationCost::NeedsDownload,
         RiskLevel::Yellow},

        {"dev.yarn", "Cache do Yarn", "%LOCALAPPDATA%\\Yarn\\Cache", "Yarn",
         "Pacotes JavaScript ja baixados pelo Yarn.",
         "Nada permanente; voltam a ser baixados quando preciso.", RegenerationCost::NeedsDownload,
         RiskLevel::Green},
    }};
    return entries;
}

}

std::vector<core::KnownLocation> known_locations_catalog() {
    std::vector<core::KnownLocation> locations;
    locations.reserve(catalog().size());

    for (const auto& entry : catalog()) {
        core::KnownLocation location;
        location.id = entry.id;
        location.display_name = entry.display_name;
        location.path = expand(entry.path);
        location.owner = entry.owner;
        location.what_it_is = entry.what_it_is;
        location.what_you_lose = entry.what_you_lose;
        location.regeneration = entry.regeneration;
        location.risk = entry.risk;

        std::error_code error;
        location.present =
            !location.path.empty() && std::filesystem::is_directory(location.path, error);

        locations.push_back(std::move(location));
    }

    return locations;
}

ReclaimableCollector::ReclaimableCollector(core::ProtectedPaths protected_paths)
    : protected_paths_(std::move(protected_paths)) {}

core::ReclaimableInfo ReclaimableCollector::collect(std::stop_token token) const {
    const scanner::StorageScanner storage_scanner{
        scanner::ScanOptions{.largest_files_kept = 1, .rollup_depth = 0}};

    core::ReclaimableInfo info;

    for (auto& location : known_locations_catalog()) {
        if (!location.present) {
            info.locations.push_back(location);
            continue;
        }

        // Um local do catalogo que caia sob a deny-list nao e oferecido. O
        // catalogo e revisado, mas a deny-list e quem decide, sempre.
        if (protected_paths_.is_protected(location.path)) {
            location.present = false;
            info.locations.push_back(location);
            continue;
        }

        const auto result = storage_scanner.scan(location.path, token);

        // Cancelamento e a unica razao para parar: o usuario pediu. Antes,
        // qualquer local incompleto abortava o laco e levava junto todos os
        // seguintes — uma pasta ilegivel apagava o resto do relatorio, e a area
        // inteira aparecia como indisponivel.
        if (token.stop_requested()) {
            return info;
        }

        // O numero que importa e o que ocupa disco, nao o que os arquivos
        // declaram. Os perfis de aplicativo ja usavam o alocado; usar o logico
        // aqui fazia dois numeros de naturezas diferentes somarem na mesma
        // tela.
        location.size_bytes = result.allocated_bytes;
        location.measured_completely = result.completed;
        info.locations.push_back(location);
    }

    info.available = true;
    return info;
}

bool ReclaimableCollector::collect_into(core::SystemSnapshot& snapshot,
                                        std::stop_token token) const {
    snapshot.reclaimable = collect(token);
    return snapshot.reclaimable.available;
}

}
