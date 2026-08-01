#pragma once

#include "core/rules/analysis_rule.hpp"

#include <cstddef>

namespace cleaner::core {

/// Aponta indicios de que arquivos ou componentes do Windows podem estar
/// danificados, e explica as ferramentas oficiais que verificam isso.
///
/// Esta versao do Cleaner nao executa sfc nem DISM: eles exigem administrador,
/// demoram e um deles altera o sistema. O que a regra faz e o passo que o
/// planejamento exige antes de qualquer reparo — reunir evidencia de que o
/// reparo faz sentido, em vez de sugerir comando so porque ele existe.
class WindowsIntegrityRule final : public AnalysisRule {
public:
    /// Um evento isolado costuma ser ruido de uma atualizacao que se resolveu
    /// sozinha. Repeticao e que sustenta a suspeita.
    static constexpr std::size_t kMinimumEvents = 2;

    [[nodiscard]] std::string id() const override;
    [[nodiscard]] int version() const override;

    [[nodiscard]] std::vector<Recommendation> evaluate(const SystemSnapshot& snapshot) const override;
};

}
