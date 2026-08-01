#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cleaner::monitor {

/// Atividade observada numa pasta, ja agrupada.
///
/// Guarda onde e quando houve escrita, nao quanto. O aviso do sistema informa
/// qual arquivo mudou, nunca o tamanho — e consultar o disco a cada evento
/// transformaria o monitor no proprio peso que ele deveria evitar. O quanto vem
/// dos retratos, que medem de verdade.
struct FolderActivity {
    std::string folder;

    std::size_t event_count = 0;

    std::string first_seen;
    std::string last_seen;

    /// Arquivos distintos tocados no periodo. Um programa gravando o mesmo log
    /// mil vezes e diferente de um que criou mil arquivos.
    std::size_t distinct_files = 0;
};

/// Observa pastas enquanto o aplicativo esta aberto e diz onde houve escrita.
///
/// Preenche a lacuna que o retrato nao cobre: comparar dois retratos revela que
/// uma pasta cresceu, mas nao quando. Esta classe da a hora.
///
/// Fica deliberadamente barata. Os avisos do sistema chegam agrupados, sao
/// consolidados por pasta antes de virar linha no banco, e nada e consultado no
/// disco durante a observacao.
class FolderWatcher {
public:
    /// Eventos do mesmo lugar dentro desta janela viram uma linha so. Sem isso,
    /// um compilador escrevendo em rajada geraria milhares de registros que
    /// dizem a mesma coisa.
    static constexpr std::chrono::seconds kConsolidationWindow{60};

    FolderWatcher();
    ~FolderWatcher();

    FolderWatcher(const FolderWatcher&) = delete;
    FolderWatcher& operator=(const FolderWatcher&) = delete;
    FolderWatcher(FolderWatcher&&) = delete;
    FolderWatcher& operator=(FolderWatcher&&) = delete;

    /// Passa a observar a pasta e tudo abaixo dela. Devolve falso quando o
    /// sistema recusa — pasta inexistente ou sem permissao.
    bool watch(const std::filesystem::path& folder);

    /// Encerra a observacao. Chamado tambem pelo destrutor.
    void stop();

    /// A atividade acumulada ate agora, consolidada por pasta.
    [[nodiscard]] std::vector<FolderActivity> collect() const;

    /// Esvazia o acumulado. Usado depois de gravar no banco, para a memoria nao
    /// crescer enquanto o aplicativo fica aberto.
    std::vector<FolderActivity> drain();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
