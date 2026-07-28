#pragma once

#include "storage/json_codec.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace zelo::storage {

/// Onde o aplicativo guarda os proprios dados, sob o perfil do usuario. Nada
/// sai da maquina.
[[nodiscard]] std::filesystem::path default_data_directory();

/// Historico de analises, uma sessao por arquivo JSON legivel.
///
/// Sem banco de dados: o volume e pequeno, nao ha consulta relacional, e o
/// planejamento pede que o usuario consiga ler o proprio historico.
class HistoryStore {
public:
    explicit HistoryStore(std::filesystem::path directory);

    /// Grava a sessao. A escrita e atomica — o arquivo final so aparece
    /// completo, entao queda de energia no meio nao deixa historico pela
    /// metade.
    void save(const StoredSession& session) const;

    /// Sessoes legiveis, da mais recente para a mais antiga. Arquivo estragado
    /// e ignorado aqui e tratado por `quarantine_unreadable`.
    [[nodiscard]] std::vector<StoredSession> load_all() const;

    [[nodiscard]] std::optional<StoredSession> load(const std::string& id) const;

    /// Move arquivos ilegiveis para `.corrupt`. Um historico estragado nao pode
    /// impedir o aplicativo de abrir nem de gravar a proxima analise.
    [[nodiscard]] std::size_t quarantine_unreadable() const;

    /// Mantem apenas as sessoes mais recentes. Historico e util, mas nao ao
    /// ponto de crescer sem limite no disco de quem ja esta sem espaco.
    void apply_retention(std::size_t keep) const;

    [[nodiscard]] const std::filesystem::path& directory() const;

private:
    std::filesystem::path directory_;
};

}
