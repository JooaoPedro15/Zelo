#pragma once

#include <string>
#include <vector>

namespace zelo::core {

/// Um sinal verificavel que sustenta uma conclusao. O texto e exibido ao
/// usuario como motivo, entao descreve o que foi observado, nao o que se supoe.
struct ConfidenceSignal {
    std::string reason;
    double weight;
};

/// O quanto as evidencias sustentam uma conclusao — nao mede o risco da acao.
/// O sistema pode ter alta confianca de que um programa nao e usado e ainda
/// assim classificar a desinstalacao como amarela.
class Confidence {
public:
    /// Teto deliberado: a analise trabalha com indicios, entao exibir 100%
    /// prometeria uma certeza que ela nao tem.
    static constexpr double kMaximumValue = 0.95;

    static Confidence from_signals(std::vector<ConfidenceSignal> signals);

    [[nodiscard]] double value() const;

    /// Somente os sinais que pontuaram, na ordem em que foram informados.
    [[nodiscard]] const std::vector<std::string>& reasons() const;

private:
    double value_ = 0.0;
    std::vector<std::string> reasons_;
};

}
