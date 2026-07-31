#include "ui/main_window.hpp"

#include "ui/presentation.hpp"

#include <collectors/snapshot_collector.hpp>
#include <monitor/growth_report.hpp>
#include <monitor/snapshot_store.hpp>
#include <monitor/snapshot_taker.hpp>
#include <collectors/system_paths.hpp>
#include <collectors/temporary_files_collector.hpp>
#include <core/rules/format.hpp>
#include <storage/cleanup_service.hpp>
#include <storage/history_store.hpp>
#include <storage/logging.hpp>
#include <storage/quarantine_store.hpp>

#include <spdlog/spdlog.h>

#include <QApplication>
#include <QDateTime>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QIcon>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QTabWidget>
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

/// Um circulo colorido para indicar o risco ao lado do titulo do achado.
QIcon risk_badge(const QColor& color) {
    constexpr int kSize = 12;

    QPixmap pixmap(kSize, kSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(1, 1, kSize - 2, kSize - 2);

    return QIcon(pixmap);
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
    connect(findings_, &QListWidget::currentRowChanged, this, &MainWindow::update_action_button);
    findings_layout->addWidget(findings_);

    // O botao de acao so aparece para achados que o Zelo sabe resolver com
    // seguranca. Nos demais ele fica escondido, em vez de desabilitado: um
    // botao cinza sugere que existe uma acao, e para a maioria dos achados nao
    // existe nenhuma que o aplicativo deva tomar sozinho.
    action_button_ = new QPushButton(findings_box);
    action_button_->setVisible(false);
    connect(action_button_, &QPushButton::clicked, this, &MainWindow::clean_selected_finding);
    findings_layout->addWidget(action_button_);

    auto* details_box = new QGroupBox(QStringLiteral("Detalhes"), splitter);
    auto* details_layout = new QVBoxLayout(details_box);
    details_ = new QTextBrowser(details_box);
    details_->setOpenExternalLinks(false);
    details_layout->addWidget(details_);

    splitter->addWidget(findings_box);
    splitter->addWidget(details_box);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);

    tabs_ = new QTabWidget(central);
    tabs_->addTab(splitter, QStringLiteral("Analise"));
    tabs_->addTab(build_growth_tab(), QStringLiteral("O que cresceu"));
    layout->addWidget(tabs_, 1);

    setCentralWidget(central);

    show_growth();
}

QWidget* MainWindow::build_growth_tab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    growth_summary_ = new QLabel(page);
    growth_summary_->setWordWrap(true);
    growth_summary_->setTextFormat(Qt::RichText);
    layout->addWidget(growth_summary_);

    growth_ = new QTextBrowser(page);
    layout->addWidget(growth_, 1);

    auto* actions = new QHBoxLayout;

    // Um retrato do disco leva minutos. Ele nao pode acontecer sozinho ao abrir
    // o programa: seria o aplicativo travando sem o usuario ter pedido nada.
    snapshot_button_ = new QPushButton(QStringLiteral("Tirar retrato do disco"), page);
    connect(snapshot_button_, &QPushButton::clicked, this, &MainWindow::take_snapshot);
    actions->addWidget(snapshot_button_);

    snapshot_progress_ = new QLabel(page);
    actions->addWidget(snapshot_progress_, 1);

    layout->addLayout(actions);
    return page;
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

const core::Recommendation* MainWindow::selected_recommendation() const {
    const int index = findings_->currentRow();
    if (index < 0 || index >= static_cast<int>(result_.recommendations.size())) {
        return nullptr;
    }
    return &result_.recommendations.at(static_cast<std::size_t>(index));
}

void MainWindow::update_action_button(int index) {
    const core::Recommendation* recommendation =
        index >= 0 && index < static_cast<int>(result_.recommendations.size())
            ? &result_.recommendations.at(static_cast<std::size_t>(index))
            : nullptr;

    // O botao aparece nos achados cuja acao o Zelo sabe executar com seguranca
    // e desfazer. Nos demais fica escondido: botao cinza sugere que existe uma
    // acao quando, para a maioria dos achados, nao existe nenhuma que o
    // aplicativo deva tomar sozinho.
    const bool cleanable = recommendation != nullptr &&
                           (recommendation->rule_id == "storage.excessive-temporary-files" ||
                            recommendation->rule_id == "storage.reclaimable-location");

    action_button_->setVisible(cleanable);
    if (cleanable) {
        action_button_->setText(
            QStringLiteral("Limpar — libera %1")
                .arg(QString::fromStdString(core::format_bytes(recommendation->reclaimable_bytes))));
    }
}

void MainWindow::clean_selected_finding() {
    const core::Recommendation* recommendation = selected_recommendation();
    if (recommendation == nullptr) {
        return;
    }

    const std::string rule_id = recommendation->rule_id;
    const std::string title = recommendation->title;
    const std::string limitations = recommendation->limitations;
    const std::vector<std::string> affected = recommendation->affected_paths;

    action_button_->setEnabled(false);
    action_button_->setText(QStringLiteral("Verificando o que pode ser removido..."));
    QApplication::processEvents();

    const auto protected_paths =
        collectors::build_protected_paths(collectors::collect_system_paths());

    const storage::QuarantineStore quarantine{storage::default_data_directory() / "quarentena",
                                              protected_paths};
    const storage::CleanupService cleanup{quarantine, protected_paths};

    core::CleanupPlan plan;
    if (rule_id == "storage.excessive-temporary-files") {
        const collectors::TemporaryFilesCollector collector{protected_paths};

        std::vector<std::string> paths;
        for (const auto& file : collector.list_files()) {
            paths.push_back(file.string());
        }
        plan = cleanup.plan(paths, rule_id);
    } else {
        // Locais do catalogo sao pastas: o conteudo e levantado agora, para
        // refletir o disco no momento de agir.
        for (const auto& folder : affected) {
            core::CleanupPlan folder_plan = cleanup.plan_folder(folder, rule_id);
            plan.items.insert(plan.items.end(), folder_plan.items.begin(), folder_plan.items.end());
            plan.rejected.insert(plan.rejected.end(), folder_plan.rejected.begin(),
                                 folder_plan.rejected.end());
        }
    }

    action_button_->setEnabled(true);
    update_action_button(findings_->currentRow());

    if (plan.empty()) {
        QMessageBox::information(this, QStringLiteral("Limpeza"),
                                 QStringLiteral("Nao ha arquivos temporarios que possam ser "
                                                "removidos com seguranca agora."));
        return;
    }

    // A confirmacao mostra numero e tamanho de verdade e repete o que se perde
    // naquele local especifico. Aprovar "limpar" sem saber quanto sai e o que
    // fica para tras nao e consentimento informado.
    QMessageBox confirmation(this);
    confirmation.setWindowTitle(QStringLiteral("Confirmar limpeza"));
    confirmation.setIcon(QMessageBox::Question);
    confirmation.setText(QStringLiteral("Remover %1 arquivos, liberando cerca de %2?")
                             .arg(plan.items.size())
                             .arg(QString::fromStdString(core::format_bytes(plan.total_bytes()))));

    QString details = QString::fromStdString(title) + QStringLiteral("\n\n");
    if (!limitations.empty()) {
        details += QString::fromStdString(limitations) + QStringLiteral("\n\n");
    }
    details += QStringLiteral(
        "Os arquivos sao apagados de vez e o espaco e liberado na hora.\n\n"
        "Programas abertos podem estar usando parte deles; esses ficam onde estao e o espaco "
        "liberado sera menor que o estimado.");
    confirmation.setInformativeText(details);

    auto* confirm = confirmation.addButton(QStringLiteral("Limpar"), QMessageBox::AcceptRole);
    confirmation.addButton(QStringLiteral("Cancelar"), QMessageBox::RejectRole);
    confirmation.setDefaultButton(qobject_cast<QPushButton*>(confirmation.buttons().last()));
    confirmation.exec();

    if (confirmation.clickedButton() != confirm) {
        spdlog::info("limpeza cancelada pelo usuario");
        return;
    }

    action_button_->setEnabled(false);
    action_button_->setText(QStringLiteral("Limpando..."));
    QApplication::processEvents();

    // Conteudo do catalogo e recriado pelo programa dono, entao guardar copia
    // ocuparia o mesmo espaco que se queria liberar.
    const core::CleanupOutcome outcome = cleanup.execute(plan, storage::RemovalMode::Delete);

    QString report = QStringLiteral("%1 arquivos removidos, %2 liberados.")
                         .arg(outcome.removed_count)
                         .arg(QString::fromStdString(core::format_bytes(outcome.freed_bytes)));
    if (!outcome.skipped.empty()) {
        report += QStringLiteral("\n\n%1 arquivos ficaram onde estavam, em geral por estarem "
                                 "em uso por algum programa aberto. Fechar o programa e limpar "
                                 "de novo costuma resolver.")
                      .arg(outcome.skipped.size());
    }

    QMessageBox::information(this, QStringLiteral("Limpeza concluida"), report);

    // O botao volta a funcionar antes da reanalise. Sem isto, limpar um achado
    // deixava todos os outros sem acao.
    action_button_->setEnabled(true);

    // Reanalisa: a pontuacao e o espaco livre mudaram, e mostrar numero velho
    // depois de agir seria enganoso.
    start_analysis();
}

namespace {

/// Onde o monitor guarda os retratos. Fica junto dos demais dados do Zelo, e a
/// varredura exclui essa pasta de proposito.
std::filesystem::path snapshot_database() {
    return storage::default_data_directory() / "monitor" / "retratos.sqlite";
}

QString attribution_label(monitor::AttributionConfidence confidence) {
    switch (confidence) {
    case monitor::AttributionConfidence::Confirmed:
        return QStringLiteral("confirmado");
    case monitor::AttributionConfidence::HighlyLikely:
        return QStringLiteral("altamente provavel");
    case monitor::AttributionConfidence::PossiblyRelated:
        return QStringLiteral("possivelmente relacionado");
    case monitor::AttributionConfidence::Unknown:
        break;
    }
    return QStringLiteral("origem desconhecida");
}

}

void MainWindow::take_snapshot() {
    snapshot_button_->setEnabled(false);
    snapshot_progress_->setText(QStringLiteral("Percorrendo o disco... isto leva alguns minutos."));
    QApplication::processEvents();

    monitor::SnapshotStore store{snapshot_database()};
    if (!store.ok()) {
        snapshot_progress_->setText(QStringLiteral("Nao foi possivel abrir o banco de retratos."));
        snapshot_button_->setEnabled(true);
        return;
    }

    const monitor::SnapshotTaker taker{monitor::SnapshotOptions{
        // Sem isto o proprio banco de retratos apareceria como consumo
        // misterioso na proxima comparacao.
        .excluded_paths = {storage::default_data_directory().string()},
    }};

    spdlog::info("retrato iniciado");
    auto snapshot = taker.take("C:\\", "C:");

    if (!snapshot.complete) {
        snapshot_progress_->setText(
            QStringLiteral("A varredura nao terminou; o retrato nao foi guardado."));
        snapshot_button_->setEnabled(true);
        return;
    }

    if (store.save(snapshot) == 0) {
        snapshot_progress_->setText(QStringLiteral("Nao foi possivel guardar o retrato."));
        snapshot_button_->setEnabled(true);
        return;
    }

    store.apply_retention();
    spdlog::info("retrato guardado: {} pastas", snapshot.folders.size());

    snapshot_progress_->setText(
        QStringLiteral("Retrato guardado (%1 pastas). Banco: %2.")
            .arg(snapshot.folders.size())
            .arg(QString::fromStdString(core::format_bytes(store.database_size_bytes()))));

    snapshot_button_->setEnabled(true);
    show_growth();
}

void MainWindow::show_growth() {
    const monitor::SnapshotStore store{snapshot_database()};
    if (!store.ok()) {
        growth_summary_->setText(QStringLiteral("<b>O monitoramento nao pode ser aberto.</b>"));
        return;
    }

    const auto snapshots = store.list();

    if (snapshots.size() < 2) {
        // Ser claro sobre isso importa: sem um passado registrado nao ha como
        // dizer o que mudou, e prometer o contrario frustraria o usuario.
        growth_summary_->setText(
            snapshots.empty()
                ? QStringLiteral("<b>Nenhum retrato ainda.</b> O primeiro serve de referencia; a "
                                 "comparacao aparece a partir do segundo.")
                : QStringLiteral("<b>Um retrato guardado.</b> Tire outro depois de usar o "
                                 "computador para ver o que mudou no periodo."));
        growth_->clear();
        return;
    }

    const auto diff = store.compare(snapshots[1].id, snapshots[0].id);
    if (!diff) {
        growth_summary_->setText(
            QStringLiteral("<b>Os dois retratos mais recentes nao podem ser comparados.</b>"));
        return;
    }

    const auto report = monitor::build_growth_report(*diff);

    const bool lost = report.free_space_delta < 0;
    const auto amount = static_cast<std::uint64_t>(std::abs(report.free_space_delta));

    growth_summary_->setText(
        QStringLiteral("<b>Desde %1, o disco C: %2 %3 de espaco livre.</b>")
            .arg(QString::fromStdString(report.from_taken_at),
                 lost ? QStringLiteral("perdeu") : QStringLiteral("recuperou"),
                 QString::fromStdString(core::format_bytes(amount))));

    QString body;

    if (report.items.empty()) {
        body += QStringLiteral("<p>Nenhuma pasta cresceu de forma relevante no periodo.</p>");
    } else {
        body += QStringLiteral("<p><b>O que cresceu</b></p><ul>");
        for (std::size_t index = 0; index < std::min<std::size_t>(15, report.items.size());
             ++index) {
            const auto& item = report.items[index];
            body += QStringLiteral("<li><b>%1</b> — %2 <i>(%3)</i></li>")
                        .arg(QString::fromStdString(item.path).toHtmlEscaped(),
                             QString::fromStdString(core::format_bytes(
                                 static_cast<std::uint64_t>(item.exclusive_bytes))),
                             attribution_label(item.attribution));
        }
        body += QStringLiteral("</ul>");
    }

    if (!report.shrunk.empty()) {
        body += QStringLiteral("<p><b>O que encolheu</b></p><ul>");
        for (std::size_t index = 0; index < std::min<std::size_t>(5, report.shrunk.size());
             ++index) {
            const auto& item = report.shrunk[index];
            body += QStringLiteral("<li>%1 — %2 a menos</li>")
                        .arg(QString::fromStdString(item.path).toHtmlEscaped(),
                             QString::fromStdString(core::format_bytes(
                                 static_cast<std::uint64_t>(-item.exclusive_bytes))));
        }
        body += QStringLiteral("</ul>");
    }

    body += QStringLiteral(
        "<p style='color:gray'><i>Cada pasta mostra o quanto ela cresceu por conta propria: o "
        "crescimento que vem de uma subpasta aparece na subpasta, para o mesmo espaco nao ser "
        "contado duas vezes.</i></p>");

    growth_->setHtml(body);
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

        // O risco vira marcador, nao cor do texto. Pintar o texto o deixava
        // ilegivel sobre o fundo da selecao, e cor sozinha nao comunica nada a
        // quem nao distingue verde de vermelho — o rotulo do risco continua
        // escrito no painel de detalhes e na dica.
        item->setIcon(risk_badge(risk_color(recommendation.risk)));
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
