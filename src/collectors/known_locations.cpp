#include "collectors/known_locations.hpp"

#include "collectors/detail/text.hpp"

#include <scanner/storage_scanner.hpp>

#include <filesystem>

#include <array>
#include <system_error>
#include <utility>

#include <windows.h>

namespace zelo::collectors {

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
const std::array<CatalogEntry, 16>& catalog() {
    static const std::array<CatalogEntry, 16> entries{{
        // --- Temporarios do sistema ---
        {"temp.user", "Arquivos temporarios", "%TEMP%", "Windows",
         "Arquivos que programas criam para uso momentaneo e nem sempre apagam depois. "
         "Instaladores ja usados, logs e sobras de operacoes concluidas.",
         "Nada que voce use. Programas abertos podem estar usando alguns deles, e esses ficam "
         "onde estao.",
         RegenerationCost::Free, RiskLevel::Green},

        {"temp.windows", "Temporarios do Windows", "%SystemRoot%\\Temp", "Windows",
         "A mesma coisa, na pasta que o proprio Windows usa.",
         "Nada que voce use.", RegenerationCost::Free, RiskLevel::Green},

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
         "O historico de travamentos antigos. O Zelo usa o registro de eventos, nao estes "
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

        {"dev.codex.tmp", "Temporarios do Codex", "%USERPROFILE%\\.codex\\.tmp", "Codex",
         "Arquivos de trabalho que o Codex cria durante o uso.",
         "Nada do seu historico ou das suas configuracoes, que ficam em outras pastas.",
         RegenerationCost::Free, RiskLevel::Yellow},

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
        if (!result.completed) {
            return info;
        }

        location.size_bytes = result.total_bytes;
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
