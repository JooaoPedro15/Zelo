#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cleaner::monitor {

/// Quanto uma pasta ocupa, contando tudo que esta dentro dela.
///
/// E o total da subarvore, nao apenas dos arquivos soltos na pasta: a pergunta
/// do usuario e "quanto AppData\Local\Google esta ocupando", e a resposta
/// precisa incluir o que ha nas subpastas.
struct FolderSize {
    std::string path;
    std::uint64_t logical_bytes = 0;
    std::uint64_t allocated_bytes = 0;
    std::size_t file_count = 0;

    /// Distancia da raiz da varredura. Serve para a interface mostrar os niveis
    /// de cima primeiro em vez de despejar tudo.
    int depth = 0;
};

/// Um arquivo grande o suficiente para valer a pena guardar individualmente.
struct TrackedFile {
    std::string path;
    std::uint64_t logical_bytes = 0;
    std::uint64_t allocated_bytes = 0;
    std::string modified_at;
};

/// Uma fotografia do armazenamento num instante.
struct Snapshot {
    std::int64_t id = 0;
    std::string taken_at;
    std::string volume;

    std::uint64_t total_bytes = 0;
    std::uint64_t free_bytes = 0;

    /// De onde veio: aberto pelo usuario, apos limpeza, automatico.
    std::string kind;
    std::string app_version;

    /// Falso quando a varredura foi interrompida. Um retrato parcial nao pode
    /// ser comparado com outro completo: o diff acusaria uma queda enorme que
    /// nunca aconteceu.
    bool complete = false;

    std::vector<FolderSize> folders;
    std::vector<TrackedFile> files;
};

/// Uma pasta que mudou de tamanho entre dois retratos.
struct FolderChange {
    std::string path;

    std::uint64_t before_bytes = 0;
    std::uint64_t after_bytes = 0;

    /// Positivo quando cresceu. Guardado com sinal para a interface nao ter que
    /// recalcular e arriscar inverter.
    std::int64_t delta_bytes = 0;

    bool appeared = false;
    bool disappeared = false;
};

/// O que mudou entre dois retratos.
struct SnapshotDiff {
    std::string from_taken_at;
    std::string to_taken_at;

    std::int64_t free_space_delta = 0;

    /// Ordenadas do maior crescimento para o maior encolhimento.
    std::vector<FolderChange> changes;
};

}
