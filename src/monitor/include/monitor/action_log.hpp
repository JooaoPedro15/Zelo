#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace zelo::monitor {

/// O que o Zelo fez, e se aquilo pode ser revertido.
enum class ActionKind {
    /// Arquivos apagados de vez. Nao ha volta.
    Deleted,

    /// Arquivos movidos para a quarentena, com origem registrada.
    Quarantined,

    /// Itens devolvidos da quarentena ao lugar de origem.
    Restored,
};

/// Uma acao registrada.
struct ActionRecord {
    std::int64_t id = 0;
    std::string at;

    ActionKind kind = ActionKind::Deleted;

    /// Que recomendacao motivou. Liga a acao ao motivo que o usuario leu antes
    /// de aprovar.
    std::string reason;

    /// De onde saiu. Para uma limpeza de pasta, a pasta.
    std::string target;

    std::size_t item_count = 0;
    std::uint64_t bytes = 0;

    std::size_t skipped_count = 0;

    /// Falso quando a acao e definitiva. O historico nunca promete devolver o
    /// que foi apagado de vez.
    bool reversible = false;
};

/// Guarda o que foi feito no computador.
///
/// Sem quarentena, este registro e a unica rastreabilidade que sobra: ele nao
/// desfaz nada, mas responde o que saiu, quando, por qual motivo e quanto
/// espaco aquilo liberou. Registrar e obrigatorio justamente porque a acao e
/// irreversivel.
class ActionLog {
public:
    explicit ActionLog(std::filesystem::path database_path);
    ~ActionLog();

    ActionLog(const ActionLog&) = delete;
    ActionLog& operator=(const ActionLog&) = delete;
    ActionLog(ActionLog&&) = delete;
    ActionLog& operator=(ActionLog&&) = delete;

    [[nodiscard]] bool ok() const;

    std::int64_t record(const ActionRecord& action);

    /// As acoes mais recentes primeiro.
    [[nodiscard]] std::vector<ActionRecord> recent(int limit = 100) const;

    /// Acoes cujo alvo ou motivo contenha o termo. Busca simples, sem
    /// diferenciar maiusculas.
    [[nodiscard]] std::vector<ActionRecord> search(const std::string& term, int limit = 100) const;

    /// Quanto espaco as acoes registradas liberaram no total.
    [[nodiscard]] std::uint64_t total_freed_bytes() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
