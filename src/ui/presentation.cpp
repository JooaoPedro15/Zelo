#include "ui/presentation.hpp"

#include <core/rules/format.hpp>

namespace zelo::ui {

QString risk_label(core::RiskLevel risk) {
    switch (risk) {
    case core::RiskLevel::Green:
        return QStringLiteral("Normalmente seguro");
    case core::RiskLevel::Yellow:
        return QStringLiteral("Revise antes");
    case core::RiskLevel::Red:
        return QStringLiteral("Apenas informativo");
    }
    return QStringLiteral("Apenas informativo");
}

QColor risk_color(core::RiskLevel risk) {
    switch (risk) {
    case core::RiskLevel::Green:
        return QColor{0x2E, 0x7D, 0x32};
    case core::RiskLevel::Yellow:
        return QColor{0xE6, 0x8A, 0x00};
    case core::RiskLevel::Red:
        return QColor{0xC6, 0x28, 0x28};
    }
    return QColor{0xC6, 0x28, 0x28};
}

QString severity_label(core::Severity severity) {
    switch (severity) {
    case core::Severity::Info:
        return QStringLiteral("Informacao");
    case core::Severity::Attention:
        return QStringLiteral("Atencao");
    case core::Severity::Serious:
        return QStringLiteral("Importante");
    }
    return QStringLiteral("Informacao");
}

QString health_category_label(core::HealthCategory category) {
    switch (category) {
    case core::HealthCategory::Storage:
        return QStringLiteral("Armazenamento");
    case core::HealthCategory::WindowsIntegrity:
        return QStringLiteral("Integridade do Windows");
    case core::HealthCategory::Startup:
        return QStringLiteral("Inicializacao");
    case core::HealthCategory::Disks:
        return QStringLiteral("Discos");
    case core::HealthCategory::Updates:
        return QStringLiteral("Atualizacoes");
    case core::HealthCategory::Security:
        return QStringLiteral("Seguranca");
    case core::HealthCategory::Performance:
        return QStringLiteral("Desempenho");
    case core::HealthCategory::Stability:
        return QStringLiteral("Estabilidade");
    }
    return QStringLiteral("Categoria");
}

QString confidence_label(const core::Confidence& confidence) {
    return QStringLiteral("%1%").arg(static_cast<int>(confidence.value() * 100.0 + 0.5));
}

QString format_bytes(std::uint64_t bytes) {
    return QString::fromStdString(core::format_bytes(bytes));
}

QString health_summary(const core::AnalysisResult& result) {
    const auto count = static_cast<int>(result.recommendations.size());

    if (count == 0) {
        return QStringLiteral("Nenhum problema encontrado nas areas analisadas.");
    }
    if (count == 1) {
        return QStringLiteral("1 ponto de atencao encontrado.");
    }
    return QStringLiteral("%1 pontos de atencao encontrados.").arg(count);
}

}
