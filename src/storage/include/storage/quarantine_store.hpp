#pragma once

#include <core/risk/protected_paths.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace zelo::storage {

/// Um arquivo guardado na quarentena, com o que e preciso para devolve-lo.
struct QuarantineEntry {
    std::string id;
    std::string original_path;
    std::uint64_t size_bytes = 0;
    std::string quarantined_at;

    /// Qual recomendacao motivou a remocao. Aparece no historico para o usuario
    /// entender por que aquele arquivo saiu do lugar.
    std::string reason;
};

/// Guarda arquivos removidos pela limpeza em vez de apaga-los.
///
/// E a rede de seguranca do modo de limpeza: enquanto o arquivo esta aqui, a
/// remocao pode ser desfeita. Nada e apagado de verdade ate a purga, que tem
/// prazo e confirmacao propria.
///
/// A quarentena consulta a deny-list no momento de agir, nao apenas quando a
/// recomendacao foi criada. Se uma regra errar e apontar para um caminho
/// protegido, a recusa acontece aqui — defesa em profundidade.
class QuarantineStore {
public:
    QuarantineStore(std::filesystem::path root, core::ProtectedPaths protected_paths);

    /// Move o arquivo para a quarentena. Nao apaga nada.
    ///
    /// Devolve vazio quando o arquivo nao existe, esta protegido ou nao pode
    /// ser movido — um arquivo em uso, por exemplo.
    [[nodiscard]] std::optional<QuarantineEntry> take(const std::filesystem::path& file,
                                                      const std::string& reason) const;

    /// Devolve o arquivo ao lugar de origem.
    ///
    /// Recusa quando ja existe algo naquele caminho: o programa dono pode ter
    /// recriado o arquivo, e sobrescrever destruiria a versao nova.
    [[nodiscard]] bool restore(const std::string& id) const;

    [[nodiscard]] std::vector<QuarantineEntry> entries() const;

    /// Apaga em definitivo o que passou do prazo. Este e o unico ponto do
    /// modulo em que um arquivo deixa de existir.
    std::size_t purge_older_than(int days) const;

private:
    [[nodiscard]] std::filesystem::path manifest_path() const;
    [[nodiscard]] std::filesystem::path storage_path(const std::string& id) const;

    /// Devolve falso quando o registro nao pode ser gravado. Quem chama precisa
    /// tratar: sem registro, um arquivo movido vira orfao e o desfazer se perde.
    [[nodiscard]] bool write_entries(const std::vector<QuarantineEntry>& entries) const;

    std::filesystem::path root_;
    core::ProtectedPaths protected_paths_;
};

}
