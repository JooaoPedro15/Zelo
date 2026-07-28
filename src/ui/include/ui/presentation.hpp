#pragma once

#include <core/usecases/quick_analysis.hpp>

#include <QColor>
#include <QString>

namespace zelo::ui {

/// Traducoes e formatacoes para a tela. Ficam separadas dos widgets para poder
/// ser testadas sem abrir janela.
///
/// A interface so apresenta o que a analise decidiu: nao classifica risco, nao
/// calcula confianca e nao monta comando.
[[nodiscard]] QString risk_label(core::RiskLevel risk);
[[nodiscard]] QColor risk_color(core::RiskLevel risk);

[[nodiscard]] QString severity_label(core::Severity severity);
[[nodiscard]] QString health_category_label(core::HealthCategory category);

/// Confianca como porcentagem inteira, do jeito que o planejamento pede.
[[nodiscard]] QString confidence_label(const core::Confidence& confidence);

[[nodiscard]] QString format_bytes(std::uint64_t bytes);

/// Frase de abertura do painel. Descreve o estado sem alarmar e sem prometer.
[[nodiscard]] QString health_summary(const core::AnalysisResult& result);

}
