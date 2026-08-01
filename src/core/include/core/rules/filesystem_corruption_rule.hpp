#pragma once

#include "core/rules/analysis_rule.hpp"

#include <cstddef>

namespace cleaner::core {

/// Aponta corrupcao na estrutura do sistema de arquivos, registrada pelo
/// proprio NTFS.
///
/// Nao confundir com componente do Windows danificado: la a ferramenta e o sfc,
/// aqui e o chkdsk. Sao problemas diferentes, com causas e riscos diferentes.
///
/// A regra recomenda apenas a verificacao somente leitura. O reparo altera o
/// disco, pode exigir agendamento no proximo inicio e, num disco em falha,
/// pode piorar a situacao — por isso fica de fora desta versao.
class FilesystemCorruptionRule final : public AnalysisRule {
public:
    /// Corrupcao de indice acontece de forma isolada e o Windows costuma se
    /// recuperar sozinho. Repeticao e que indica volume precisando de atencao.
    static constexpr std::size_t kMinimumEvents = 3;

    /// Acima disso o volume vem reclamando com frequencia, e a chance de haver
    /// problema fisico no disco cresce.
    static constexpr std::size_t kSevereEvents = 20;

    [[nodiscard]] std::string id() const override;
    [[nodiscard]] int version() const override;

    [[nodiscard]] std::vector<Recommendation> evaluate(const SystemSnapshot& snapshot) const override;
};

}
