#pragma once

#include <string>
#include <vector>

namespace cleaner::core {

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

    /// Reconstroi uma confianca ja calculada, para leitura de historico. Nao
    /// recalcula nada: os sinais originais nao sao guardados, apenas o
    /// resultado e os motivos que foram mostrados ao usuario na epoca.
    static Confidence restored(double value, std::vector<std::string> reasons);

    [[nodiscard]] double value() const;

    /// Somente os sinais que pontuaram, na ordem em que foram informados.
    [[nodiscard]] const std::vector<std::string>& reasons() const;

private:
    double value_ = 0.0;
    std::vector<std::string> reasons_;
};

}
