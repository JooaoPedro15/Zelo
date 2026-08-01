#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cleaner::commands {

/// Um instalador guardado em `C:\Windows\Installer` que nenhum programa
/// instalado reivindica.
struct OrphanInstaller {
    std::string path;
    std::uint64_t bytes = 0;
};

/// O que ha na pasta de instaladores do Windows.
///
/// O Windows guarda ali uma copia do .msi de cada programa e do .msp de cada
/// correcao, para poder reparar e desinstalar depois. Ele nunca remove essas
/// copias sozinho — nem quando o programa dono e desinstalado. Em maquina com
/// anos de uso a pasta passa facil de 20 GB, quase toda de programa que nao
/// existe mais.
struct InstallerCacheReport {
    /// Instaladores sem dono. Sao estes que podem sair.
    std::vector<OrphanInstaller> orphans;
    std::uint64_t orphan_bytes = 0;

    /// Instaladores que algum programa instalado ainda usa. Ficam.
    std::size_t referenced_count = 0;
    std::uint64_t referenced_bytes = 0;

    /// Falso quando nao deu para perguntar ao Windows quais instaladores estao
    /// em uso.
    ///
    /// Sem essa resposta, todo arquivo da pasta pareceria orfao — e apagar um
    /// instalador que ainda tem dono quebra reparar e desinstalar aquele
    /// programa. Entao a lista sai vazia em vez de sair errada.
    bool products_readable = false;

    /// O que impediu a leitura, quando ela falhou.
    std::string obstacle;
};

/// Pergunta ao Windows quais instaladores ainda tem dono e separa o resto.
/// Somente leitura.
[[nodiscard]] InstallerCacheReport scan_installer_cache();

struct InstallerCleanupOutcome {
    std::size_t removed = 0;
    std::size_t failed = 0;
    std::uint64_t bytes = 0;

    /// Verdadeiro quando a remocao parou por falta de privilegio.
    bool needs_elevation = false;
};

/// Remove os instaladores sem dono informados.
///
/// Apaga de vez: nao ha copia guardada. O que se perde e a possibilidade de
/// reparar ou desinstalar pelo painel do Windows um programa que ja nao esta
/// instalado — e por isso a lista so inclui os que nenhum programa reivindica.
InstallerCleanupOutcome remove_orphan_installers(const std::vector<OrphanInstaller>& orphans);

}
