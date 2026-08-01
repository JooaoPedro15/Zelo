#pragma once

#include "monitor/snapshot.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace cleaner::monitor {

/// Guarda os retratos do armazenamento e responde o que mudou entre eles.
///
/// Usa SQLite porque a pergunta central — "quais pastas cresceram entre estes
/// dois instantes" — e uma juncao sobre dezenas de milhares de linhas. Em
/// arquivo solto isso viraria carregar tudo na memoria a cada comparacao.
///
/// O banco fica no diretorio de dados do aplicativo e tem politica de retencao
/// propria: um monitor de espaco que engorda sem limite seria mais um causador
/// do problema que ele existe para resolver.
class SnapshotStore {
public:
    /// Abre ou cria o banco. `ok()` diz se deu certo — um banco indisponivel
    /// nao pode derrubar o aplicativo, apenas desligar o monitoramento.
    explicit SnapshotStore(std::filesystem::path database_path);
    ~SnapshotStore();

    SnapshotStore(const SnapshotStore&) = delete;
    SnapshotStore& operator=(const SnapshotStore&) = delete;
    SnapshotStore(SnapshotStore&&) = delete;
    SnapshotStore& operator=(SnapshotStore&&) = delete;

    [[nodiscard]] bool ok() const;

    /// Grava o retrato e devolve o identificador. Zero em caso de falha.
    std::int64_t save(const Snapshot& snapshot);

    /// Os retratos ja guardados, do mais recente para o mais antigo, sem as
    /// pastas — a lista existe para escolher o que comparar.
    [[nodiscard]] std::vector<Snapshot> list(int limit = 50) const;

    [[nodiscard]] std::optional<Snapshot> latest(const std::string& volume) const;

    /// O retrato completo mais proximo do instante pedido, para comparar com
    /// "ontem" ou "sete dias atras" sem exigir que exista um exatamente ali.
    [[nodiscard]] std::optional<Snapshot> closest_to(const std::string& volume,
                                                     const std::string& taken_at) const;

    /// O que mudou entre dois retratos.
    ///
    /// So compara retratos completos: cruzar um parcial com um inteiro
    /// mostraria como reducao aquilo que apenas nao foi visitado.
    [[nodiscard]] std::optional<SnapshotDiff> compare(std::int64_t from_id,
                                                      std::int64_t to_id) const;

    /// Aplica a retencao: mantem todos do ultimo dia, um por dia no ultimo mes
    /// e um por semana antes disso. Devolve quantos foram removidos.
    std::size_t apply_retention();

    /// Tamanho do proprio banco. A interface mostra este numero: o usuario
    /// precisa saber quanto custa manter o historico.
    [[nodiscard]] std::uint64_t database_size_bytes() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
