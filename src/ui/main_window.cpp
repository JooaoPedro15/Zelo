#include "ui/main_window.hpp"

#include "ui/presentation.hpp"

#include <collectors/snapshot_collector.hpp>
#include <storage/history_store.hpp>
#include <storage/logging.hpp>

#include <spdlog/spdlog.h>

#include <QApplication>
#include <QDateTime>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace zelo::ui {

namespace {

QString escaped(const std::string& text) {
    return QString::fromStdString(text).toHtmlEscaped();
}

QString section(const QString& title, const QString& body) {
    if (body.trimmed().isEmpty()) {
        return {};
    }
    return QStringLiteral("<p><b>%1</b><br>%2</p>").arg(title, body);
}

}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Zelo"));
    resize(1040, 680);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);

    auto* header = new QHBoxLayout;

    auto* health_box = new QVBoxLayout;
    score_label_ = new QLabel(QStringLiteral("Saude geral: —"), central);
    QFont score_font = score_label_->font();
    score_font.setPointSize(score_font.pointSize() + 6);
    score_font.setBold(true);
    score_label_->setFont(score_font);

    summary_label_ = new QLabel(
        QStringLiteral("Clique em Analisar para examinar o computador. Nada e alterado."), central);
    summary_label_->setWordWrap(true);

    score_bar_ = new QProgressBar(central);
    score_bar_->setRange(0, 100);
    score_bar_->setValue(0);
    score_bar_->setTextVisible(false);
    score_bar_->setFixedHeight(10);

    health_box->addWidget(score_label_);
    health_box->addWidget(score_bar_);
    health_box->addWidget(summary_label_);

    analyze_button_ = new QPushButton(QStringLiteral("Analisar"), central);
    analyze_button_->setMinimumWidth(140);
    connect(analyze_button_, &QPushButton::clicked, this, &MainWindow::start_analysis);

    header->addLayout(health_box, 1);
    header->addWidget(analyze_button_, 0, Qt::AlignTop);
    layout->addLayout(header);

    categories_label_ = new QLabel(central);
    categories_label_->setWordWrap(true);
    categories_label_->setTextFormat(Qt::RichText);
    layout->addWidget(categories_label_);

    auto* splitter = new QSplitter(Qt::Horizontal, central);

    auto* findings_box = new QGroupBox(QStringLiteral("O que foi encontrado"), splitter);
    auto* findings_layout = new QVBoxLayout(findings_box);
    findings_ = new QListWidget(findings_box);
    connect(findings_, &QListWidget::currentRowChanged, this, &MainWindow::show_details);
    findings_layout->addWidget(findings_);

    auto* details_box = new QGroupBox(QStringLiteral("Detalhes"), splitter);
    auto* details_layout = new QVBoxLayout(details_box);
    details_ = new QTextBrowser(details_box);
    details_->setOpenExternalLinks(false);
    details_layout->addWidget(details_);

    splitter->addWidget(findings_box);
    splitter->addWidget(details_box);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    layout->addWidget(splitter, 1);

    setCentralWidget(central);
}

const core::AnalysisResult& MainWindow::result() const {
    return result_;
}

void MainWindow::start_analysis() {
    analyze_button_->setEnabled(false);
    analyze_button_->setText(QStringLiteral("Analisando..."));
    summary_label_->setText(QStringLiteral("Lendo discos, inicializacao e temporarios..."));
    QApplication::processEvents();

    spdlog::info("analise iniciada");

    const auto snapshot = collectors::collect_snapshot();
    result_ = core::QuickAnalysis::with_default_rules().run(snapshot);

    // O que nao pode ser observado vira registro. Sem isso, uma coleta que
    // falha silenciosamente e indistinguivel de uma que nao achou nada.
    for (const auto& area : result_.unavailable) {
        spdlog::warn("nao foi possivel analisar: {}", area);
    }
    spdlog::info("analise concluida: saude {}, {} achados", result_.health.overall(),
                 result_.recommendations.size());

    show_result(result_);

    storage::StoredSession session;
    session.id = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HHmmss_quick"))
                     .toStdString();
    session.started_at = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();
    session.app_version = QApplication::applicationVersion().toStdString();
    session.result = result_;

    const storage::HistoryStore history{storage::default_data_directory() / "history"};
    history.save(session);
    history.apply_retention(50);

    if (const auto quarantined = history.quarantine_unreadable(); quarantined > 0) {
        spdlog::warn("{} arquivos de historico ilegiveis foram postos de quarentena", quarantined);
    }
    spdlog::info("analise gravada em {}", session.id);

    analyze_button_->setEnabled(true);
    analyze_button_->setText(QStringLiteral("Analisar de novo"));
}

void MainWindow::show_result(const core::AnalysisResult& result) {
    const int overall = result.health.overall();
    score_label_->setText(QStringLiteral("Saude geral: %1 de 100").arg(overall));
    score_bar_->setValue(overall);

    QString summary = health_summary(result);
    if (!result.unavailable.empty()) {
        QStringList areas;
        for (const auto& area : result.unavailable) {
            areas << QString::fromStdString(area);
        }
        // Dizer o que nao foi analisado evita que o silencio seja lido como
        // "esta tudo bem" numa area que sequer foi observada.
        summary += QStringLiteral(" Nao foi possivel analisar: %1.").arg(areas.join(", "));
    }
    summary_label_->setText(summary);

    QStringList categories;
    for (const auto category :
         {core::HealthCategory::Storage, core::HealthCategory::WindowsIntegrity,
          core::HealthCategory::Startup, core::HealthCategory::Disks,
          core::HealthCategory::Updates, core::HealthCategory::Security,
          core::HealthCategory::Performance, core::HealthCategory::Stability}) {
        categories << QStringLiteral("%1: <b>%2</b>")
                          .arg(health_category_label(category))
                          .arg(result.health.of(category));
    }
    categories_label_->setText(categories.join(QStringLiteral(" &nbsp;·&nbsp; ")));

    findings_->clear();
    for (const auto& recommendation : result.recommendations) {
        auto* item = new QListWidgetItem(QString::fromStdString(recommendation.title), findings_);
        item->setForeground(risk_color(recommendation.risk));
        item->setToolTip(QStringLiteral("%1 · confianca %2")
                             .arg(risk_label(recommendation.risk),
                                  confidence_label(recommendation.confidence)));
    }

    if (findings_->count() > 0) {
        findings_->setCurrentRow(0);
    } else {
        details_->setHtml(QStringLiteral(
            "<p>Nenhum ponto de atencao nas areas analisadas.</p>"
            "<p>Isto nao significa que o computador esta perfeito: o Zelo ainda analisa apenas "
            "espaco em disco, arquivos temporarios e programas de inicializacao.</p>"));
    }
}

void MainWindow::show_details(int index) {
    if (index < 0 || index >= static_cast<int>(result_.recommendations.size())) {
        return;
    }

    const auto& recommendation = result_.recommendations.at(static_cast<std::size_t>(index));

    QString evidence;
    for (const auto& item : recommendation.evidence) {
        evidence += QStringLiteral("<li>%1 — %2 <i>(fonte: %3)</i></li>")
                        .arg(escaped(item.description), escaped(item.value), escaped(item.source));
    }

    QString reasons;
    for (const auto& reason : recommendation.confidence.reasons()) {
        reasons += QStringLiteral("<li>%1</li>").arg(escaped(reason));
    }

    QString html = QStringLiteral("<h2>%1</h2>").arg(escaped(recommendation.title));

    html += QStringLiteral("<p><span style='color:%1'><b>%2</b></span> &nbsp;·&nbsp; "
                           "Gravidade: %3 &nbsp;·&nbsp; Confianca da analise: %4</p>")
                .arg(risk_color(recommendation.risk).name(), risk_label(recommendation.risk),
                     severity_label(recommendation.severity),
                     confidence_label(recommendation.confidence));

    html += section(QStringLiteral("O que foi encontrado"), escaped(recommendation.description));

    if (!evidence.isEmpty()) {
        html += QStringLiteral("<p><b>Como foi identificado</b><ul>%1</ul></p>").arg(evidence);
    }
    if (!reasons.isEmpty()) {
        html += QStringLiteral("<p><b>No que a conclusao se apoia</b><ul>%1</ul></p>").arg(reasons);
    }

    if (recommendation.reclaimable_bytes > 0) {
        html += section(QStringLiteral("Espaco que pode ser liberado"),
                        format_bytes(recommendation.reclaimable_bytes));
    }

    html += section(QStringLiteral("O que fazer"), escaped(recommendation.recommended_action));
    html += section(QStringLiteral("Alternativa"), escaped(recommendation.alternative_action));
    html += section(QStringLiteral("Resultado esperado"), escaped(recommendation.expected_result));

    // Limitacoes nunca sao omitidas: sao o compromisso do produto de nao
    // prometer mais do que consegue entregar.
    html += section(QStringLiteral("O que isto nao garante"), escaped(recommendation.limitations));

    if (!recommendation.affected_paths.empty()) {
        QString paths;
        const auto shown = std::min<std::size_t>(recommendation.affected_paths.size(), 15);
        for (std::size_t item = 0; item < shown; ++item) {
            paths += QStringLiteral("<li>%1</li>").arg(escaped(recommendation.affected_paths.at(item)));
        }
        if (recommendation.affected_paths.size() > shown) {
            paths += QStringLiteral("<li><i>e mais %1</i></li>")
                         .arg(recommendation.affected_paths.size() - shown);
        }
        html += QStringLiteral("<p><b>Itens envolvidos</b><ul>%1</ul></p>").arg(paths);
    }

    html += QStringLiteral(
        "<p style='color:#777'><i>Esta versao do Zelo apenas analisa e explica. "
        "Nenhuma alteracao e feita no computador.</i></p>");

    details_->setHtml(html);
}

}
