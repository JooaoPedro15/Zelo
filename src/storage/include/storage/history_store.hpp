#pragma once

#include "storage/json_codec.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace cleaner::storage {

/// Onde o aplicativo guarda os proprios dados, sob o perfil do usuario. Nada
/// sai da maquina.
[[nodiscard]] std::filesystem::path default_data_directory();

/// Adota a pasta de dados de um nome anterior do programa.
///
/// O projeto se chamou Zelo antes de se chamar Cleaner. Trocar o nome sem mover
/// os dados faria o historico, os retratos e a quarentena desaparecerem da vista
/// do usuario — presentes no disco, invisiveis no programa, e sem nenhuma
/// mensagem explicando.
///
/// Nao mescla: se a pasta nova ja existe, a antiga fica onde esta. Juntar dois
/// historicos produziria um terceiro que nunca aconteceu.
///
/// Devolve verdadeiro quando moveu alguma coisa.
bool adopt_previous_data_directory(const std::string& previous_name);

/// A mesma adocao, sobre uma pasta-base qualquer. Existe para o teste poder
/// exercitar a regra sem mexer na pasta de dados real de quem roda a suite.
bool adopt_previous_data_directory(const std::filesystem::path& base,
                                   const std::string& previous_name,
                                   const std::string& current_name);

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
