#include "ui/main_window.hpp"

#include "ui/presentation.hpp"

#include <collectors/running_apps.hpp>
#include <collectors/snapshot_collector.hpp>
#include <collectors/cloud_folders.hpp>
#include <ui/background.hpp>

#include <collectors/startup_collector.hpp>
#include <scanner/space_survey.hpp>
#include <commands/command_catalog.hpp>
#include <commands/installer_cache.hpp>
#include <commands/startup_control.hpp>
#include <commands/command_runner.hpp>
#include <monitor/action_log.hpp>
#include <monitor/growth_alerts.hpp>
#include <monitor/growth_report.hpp>
#include <monitor/snapshot_store.hpp>
#include <monitor/snapshot_taker.hpp>
#include <collectors/system_paths.hpp>
#include <collectors/temporary_files_collector.hpp>
#include <core/rules/format.hpp>
#include <storage/cleanup_service.hpp>
#include <storage/cloud_release.hpp>
#include <storage/history_store.hpp>
#include <storage/logging.hpp>
#include <storage/quarantine_store.hpp>

#include <spdlog/spdlog.h>

#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QTreeWidget>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QIcon>
#include <QLineEdit>
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

namespace cleaner::ui {

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

/// Onde o monitor guarda os retratos. Fica junto dos demais dados do Cleaner, e a
/// varredura exclui essa pasta de proposito.
std::filesystem::path snapshot_database() {
    return storage::default_data_directory() / "monitor" / "retratos.sqlite";
}

std::filesystem::path action_database() {
    return storage::default_data_directory() / "monitor" / "acoes.sqlite";
}

/// Quais programas conhecidos estao abertos e mexem nos caminhos afetados.
///
/// O mapa e curto e por caminho, nao por adivinhacao: so nomeia o programa
/// quando ha ligacao clara entre a pasta e o executavel.
QString running_apps_holding(const std::vector<std::string>& paths) {
    struct Owner {
        const char* path_marker;
        const char* executable;
        const char* display_name;
    };

    static constexpr std::array kOwners{
        Owner{"\\Google\\Chrome", "chrome.exe", "O Google Chrome"},
        Owner{"\\Microsoft\\Edge", "msedge.exe", "O Microsoft Edge"},
        Owner{"\\Code", "Code.exe", "O Visual Studio Code"},
        Owner{"\\Adobe", "Adobe Desktop Service.exe", "O Creative Cloud"},
        Owner{"\\Claude", "Claude.exe", "O Claude"},
        Owner{"\\Discord", "Discord.exe", "O Discord"},
        Owner{"\\Spotify", "Spotify.exe", "O Spotify"},
    };

    QStringList found;
    for (const auto& path : paths) {
        for (const auto& owner : kOwners) {
            if (path.find(owner.path_marker) == std::string::npos) {
                continue;
            }
            if (collectors::is_running(owner.executable)) {
                const auto name = QString::fromLatin1(owner.display_name);
                if (!found.contains(name)) {
                    found.append(name);
                }
            }
        }
    }

    return found.join(QStringLiteral(", "));
}

QString action_kind_label(monitor::ActionKind kind) {
    switch (kind) {
    case monitor::ActionKind::Deleted:
        return QStringLiteral("apagado");
    case monitor::ActionKind::Quarantined:
        return QStringLiteral("em quarentena");
    case monitor::ActionKind::Restored:
        return QStringLiteral("restaurado");
    case monitor::ActionKind::CommandRun:
        return QStringLiteral("comando do Windows");
    }
    return QStringLiteral("acao");
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
    setWindowTitle(QStringLiteral("Cleaner"));
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

    // O botao de acao so aparece para achados que o Cleaner sabe resolver com
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

    auto* growth_page = build_growth_tab();
    auto* cloud_page = build_cloud_tab();
    auto* history_page = build_history_tab();

    tabs_ = new QTabWidget(central);
    tabs_->addTab(splitter, QStringLiteral("Analise"));
    tabs_->addTab(build_space_tab(), QStringLiteral("Onde o espaco esta"));
    tabs_->addTab(growth_page, QStringLiteral("O que cresceu"));
    tabs_->addTab(cloud_page, QStringLiteral("Nuvem"));
    tabs_->addTab(build_windows_tab(), QStringLiteral("Limpezas do Windows"));
    tabs_->addTab(history_page, QStringLiteral("Historico"));

    // As abas sao remontadas ao serem abertas. A atividade observada muda
    // enquanto a janela fica aberta, e mostrar o estado do momento em que o
    // programa iniciou seria informacao velha.
    //
    // A comparacao e pelo widget, nao pela posicao: inserir uma aba no meio nao
    // pode fazer a janela reconstruir a pagina errada.
    connect(tabs_, &QTabWidget::currentChanged, this,
            [this, growth_page, cloud_page, history_page](int index) {
                const QWidget* page = tabs_->widget(index);
                if (page == growth_page) {
                    show_growth();
                } else if (page == cloud_page) {
                    show_cloud();
                } else if (page == history_page) {
                    show_history();
                }
            });

    layout->addWidget(tabs_, 1);

    setCentralWidget(central);

    show_growth();
    show_history();
    start_watching();
}

void MainWindow::start_watching() {
    watcher_ = std::make_unique<monitor::FolderWatcher>();

    // As raizes que concentram escrita de programa: e onde o espaco some sem o
    // usuario perceber. O disco inteiro seria caro demais para observar, e o
    // retrato ja cobre o resto.
    const auto profile = collectors::collect_system_paths().user_profile;
    if (profile.empty()) {
        return;
    }

    const std::filesystem::path home(profile);

    for (const auto& folder : {home / "AppData" / "Local", home / "AppData" / "Roaming"}) {
        if (watcher_->watch(folder)) {
            spdlog::info("observando {}", folder.string());
        }
    }
}

void MainWindow::manage_startup() {
    const auto items = collectors::StartupCollector{}.collect();
    if (items.empty()) {
        QMessageBox::information(this, QStringLiteral("Inicializacao"),
                                 QStringLiteral("Nada foi encontrado na inicializacao."));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("O que inicia com o Windows"));
    dialog.resize(680, 520);

    auto* layout = new QVBoxLayout(&dialog);

    auto* intro = new QLabel(
        QStringLiteral(
            "<p>Desmarque o que nao precisa abrir junto com o computador. "
            "<b>Nada e desinstalado</b>: o programa continua no lugar e volta a iniciar "
            "quando voce marcar de novo.</p>"
            "<p style='color:gray'>Itens essenciais — protecao, audio, video e drivers — "
            "aparecem bloqueados de proposito.</p>"),
        &dialog);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* list = new QListWidget(&dialog);
    for (const auto& item : items) {
        auto* row = new QListWidgetItem(
            QStringLiteral("%1%2\n%3")
                .arg(QString::fromStdString(item.name),
                     item.essential ? QStringLiteral("  (essencial)") : QString(),
                     QString::fromStdString(item.path)),
            list);

        row->setCheckState(item.enabled ? Qt::Checked : Qt::Unchecked);

        if (item.essential) {
            // Bloqueado em vez de escondido: o usuario precisa ver que o item
            // existe e por que o Cleaner nao mexe nele.
            row->setFlags(row->flags() & ~Qt::ItemIsEnabled);
        }
    }
    layout->addWidget(list, 1);

    auto* buttons = new QHBoxLayout;
    auto* apply = new QPushButton(QStringLiteral("Aplicar"), &dialog);
    auto* cancel = new QPushButton(QStringLiteral("Cancelar"), &dialog);
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(apply);
    layout->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(apply, &QPushButton::clicked, &dialog, &QDialog::accept);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    std::size_t desativados = 0;
    std::size_t religados = 0;
    std::size_t negados = 0;

    for (int index = 0; index < list->count(); ++index) {
        const auto& item = items.at(static_cast<std::size_t>(index));
        if (item.essential) {
            continue;
        }

        const bool wanted = list->item(index)->checkState() == Qt::Checked;
        if (wanted == item.enabled) {
            continue;
        }

        // Atalho da pasta de inicializacao e identificado pelo nome do arquivo
        // com extensao; entrada de registro, pelo nome do valor.
        const bool folder = item.origin == core::StartupOrigin::UserFolder ||
                            item.origin == core::StartupOrigin::MachineFolder;
        const std::string key =
            folder ? std::filesystem::path(item.path).filename().string() : item.name;

        switch (commands::set_startup_enabled(key, item.origin, wanted)) {
        case commands::StartupChange::Disabled:
            ++desativados;
            break;
        case commands::StartupChange::Enabled:
            ++religados;
            break;
        case commands::StartupChange::Denied:
            ++negados;
            break;
        case commands::StartupChange::Unchanged:
        case commands::StartupChange::Failed:
            break;
        }
    }

    QString resumo;
    if (desativados > 0) {
        resumo += QStringLiteral("%1 programas deixaram de iniciar com o Windows.\n").arg(desativados);
    }
    if (religados > 0) {
        resumo += QStringLiteral("%1 voltaram a iniciar.\n").arg(religados);
    }
    if (negados > 0) {
        resumo += QStringLiteral(
                      "%1 valem para todos os usuarios e exigem o Cleaner aberto como "
                      "administrador.\n")
                      .arg(negados);
    }
    if (resumo.isEmpty()) {
        resumo = QStringLiteral("Nada mudou.");
    } else {
        resumo += QStringLiteral("\nO efeito aparece na proxima vez que o computador ligar.");
    }

    QMessageBox::information(this, QStringLiteral("Inicializacao"), resumo);

    if (desativados > 0 || religados > 0) {
        monitor::ActionLog log{action_database()};
        if (log.ok()) {
            log.record(monitor::ActionRecord{
                .kind = monitor::ActionKind::CommandRun,
                .reason = "Mudou o que inicia com o Windows",
                .target = "inicializacao",
                .item_count = desativados + religados,
                // A unica acao do Cleaner que se desfaz por completo: basta
                // marcar a caixa de novo.
                .reversible = true,
            });
        }

        start_analysis();
    }
}

QWidget* MainWindow::build_space_tab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    auto* row = new QHBoxLayout;
    row->addWidget(new QLabel(QStringLiteral("Analisar:"), page));

    space_root_ = new QComboBox(page);

    // As tres perguntas mais comuns, na ordem em que costumam ser feitas: onde
    // esta o espaco do meu perfil, do disco inteiro, e do que o Windows guarda.
    const auto profile = collectors::collect_system_paths().user_profile;
    if (!profile.empty()) {
        const std::filesystem::path home(profile);

        space_root_->addItem(QStringLiteral("Meus dados (%1)").arg(QString::fromStdString(profile)),
                             QString::fromStdWString(home.wstring()));
        space_root_->addItem(QStringLiteral("AppData"),
                             QString::fromStdWString((home / L"AppData").wstring()));
    }
    space_root_->addItem(QStringLiteral("Disco C: inteiro"), QStringLiteral("C:\\"));
    space_root_->addItem(QStringLiteral("Windows"), QStringLiteral("C:\\Windows"));
    space_root_->addItem(QStringLiteral("Programas"), QStringLiteral("C:\\Program Files"));
    row->addWidget(space_root_, 1);

    space_button_ = new QPushButton(QStringLiteral("Analisar"), page);
    connect(space_button_, &QPushButton::clicked, this, &MainWindow::survey_space);
    row->addWidget(space_button_);

    space_cancel_ = new QPushButton(QStringLiteral("Cancelar"), page);
    space_cancel_->setEnabled(false);
    connect(space_cancel_, &QPushButton::clicked, this, &MainWindow::cancel_survey);
    row->addWidget(space_cancel_);

    layout->addLayout(row);

    space_progress_ = new QLabel(page);
    space_progress_->setWordWrap(true);
    layout->addWidget(space_progress_);

    space_summary_ = new QLabel(page);
    space_summary_->setWordWrap(true);
    space_summary_->setTextFormat(Qt::RichText);
    layout->addWidget(space_summary_);

    space_tree_ = new QTreeWidget(page);
    space_tree_->setColumnCount(3);
    space_tree_->setHeaderLabels(
        {QStringLiteral("Pasta"), QStringLiteral("Ocupa"), QStringLiteral("Arquivos")});
    space_tree_->setColumnWidth(0, 420);
    layout->addWidget(space_tree_, 1);

    return page;
}

void MainWindow::cancel_survey() {
    if (survey_stop_ != nullptr) {
        survey_stop_->request_stop();
        space_progress_->setText(QStringLiteral("Cancelando..."));
    }
}

namespace {

/// Monta a linha da arvore. Recursiva porque a arvore tambem e.
QTreeWidgetItem* build_tree_item(const core::SpaceNode& node) {
    auto* item = new QTreeWidgetItem;
    item->setText(0, QString::fromStdString(node.display_name));
    item->setText(1, QString::fromStdString(core::format_bytes(node.allocated_bytes)));
    item->setText(2, QString::number(node.file_count));
    item->setToolTip(0, QString::fromStdString(node.path));

    if (!node.complete()) {
        // Parte da pasta nao pode ser lida: o numero e um piso. Dizer isso na
        // propria linha evita que o usuario persiga uma diferenca que o
        // programa ja sabe explicar.
        item->setText(0, QStringLiteral("%1  (leitura parcial)")
                             .arg(QString::fromStdString(node.display_name)));
    }

    for (const auto& child : node.children) {
        item->addChild(build_tree_item(child));
    }
    return item;
}

}

void MainWindow::show_survey(const core::SpaceSurvey& survey) {
    space_tree_->clear();

    auto* root = build_tree_item(survey.root);
    root->setText(0, QString::fromStdString(survey.root.path));
    space_tree_->addTopLevelItem(root);
    root->setExpanded(true);

    QString body =
        QStringLiteral(
            "<table cellspacing='0' cellpadding='4'>"
            "<tr><td><b>Disco %1</b></td><td>%2 no total</td><td>%3 livres</td></tr>"
            "<tr><td><b>Em uso</b></td><td>%4</td><td>segundo o Windows</td></tr>"
            "<tr><td><b>Localizado aqui</b></td><td>%5</td><td>%6% do que esta em uso</td></tr>"
            "</table>")
            .arg(QString::fromStdString(survey.volume),
                 QString::fromStdString(core::format_bytes(survey.volume_total_bytes)),
                 QString::fromStdString(core::format_bytes(survey.volume_free_bytes)),
                 QString::fromStdString(core::format_bytes(survey.used_bytes())),
                 QString::fromStdString(core::format_bytes(survey.identified_bytes)))
            .arg(static_cast<int>(survey.coverage() * 100));

    // O numero que faltava. Sem ele, uma lista de pastas nao diz se explica o
    // disco ou so um pedaco dele — e o usuario fica achando que o espaco sumiu
    // sozinho.
    if (const auto missing = survey.unexplained_bytes(); missing > 0) {
        body += QStringLiteral(
                    "<p><b>Fora desta pasta: %1.</b> Isso nao sumiu: esta em outros lugares do "
                    "disco que esta analise nao percorreu. Escolha o disco inteiro acima para "
                    "procurar.</p>")
                    .arg(QString::fromStdString(
                        core::format_bytes(static_cast<std::uint64_t>(missing))));
    }

    if (survey.unreadable_count > 0) {
        QString examples;
        for (std::size_t index = 0;
             index < std::min<std::size_t>(3, survey.unreadable_examples.size()); ++index) {
            examples += QStringLiteral("<br>%1")
                            .arg(QString::fromStdString(survey.unreadable_examples.at(index))
                                     .toHtmlEscaped());
        }

        body += QStringLiteral(
                    "<p style='color:#E68A00'><b>%1 pastas nao puderam ser lidas</b> (permissao "
                    "negada, ou em uso). O que ha nelas nao entrou na conta.%2</p>")
                    .arg(survey.unreadable_count)
                    .arg(examples);
    }

    if (!survey.complete) {
        body += QStringLiteral(
            "<p style='color:#E68A00'>A analise foi interrompida. Os numeros valem para o que "
            "deu tempo de percorrer.</p>");
    }

    if (survey.root.online_only_bytes > 0) {
        body += QStringLiteral(
                    "<p style='color:gray'>Mais %1 em arquivos que moram so na nuvem. Nao "
                    "ocupam espaco aqui e nao entram na conta acima.</p>")
                    .arg(QString::fromStdString(
                        core::format_bytes(survey.root.online_only_bytes)));
    }

    space_summary_->setText(body);
}

void MainWindow::survey_space() {
    if (busy_) {
        return;
    }
    busy_ = true;

    const auto root = space_root_->currentData().toString().toStdWString();

    space_button_->setEnabled(false);
    space_cancel_->setEnabled(true);
    space_progress_->setText(
        QStringLiteral("Percorrendo... o disco inteiro leva alguns minutos."));

    survey_stop_ = std::make_unique<std::stop_source>();

    run_in_background<core::SpaceSurvey>(
        this,
        [root, token = survey_stop_->get_token()] {
            return scanner::survey_space(root, "C:", scanner::SurveyOptions{}, token);
        },
        [this](core::SpaceSurvey survey) {
            show_survey(survey);

            space_progress_->setText(
                survey.complete ? QStringLiteral("Analise concluida.")
                                : QStringLiteral("Analise interrompida."));
            space_button_->setEnabled(true);
            space_cancel_->setEnabled(false);
            survey_stop_.reset();
            busy_ = false;
        });
}

QWidget* MainWindow::build_cloud_tab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    cloud_folders_ = new QListWidget(page);
    cloud_folders_->setMaximumHeight(90);
    connect(cloud_folders_, &QListWidget::currentRowChanged, this, &MainWindow::show_cloud);
    layout->addWidget(cloud_folders_);

    cloud_ = new QTextBrowser(page);
    layout->addWidget(cloud_, 1);

    cloud_release_button_ = new QPushButton(QStringLiteral("Liberar espaco local"), page);
    cloud_release_button_->setEnabled(false);
    connect(cloud_release_button_, &QPushButton::clicked, this, &MainWindow::release_cloud_space);
    layout->addWidget(cloud_release_button_);

    return page;
}

void MainWindow::show_cloud() {
    if (cloud_folders_->count() == 0) {
        for (const auto& folder : collectors::cloud_folders()) {
            auto* item = new QListWidgetItem(
                QStringLiteral("%1 (%2)").arg(QString::fromStdString(folder.path),
                                              QString::fromStdString(folder.account)),
                cloud_folders_);
            item->setData(Qt::UserRole, QString::fromStdString(folder.path));
        }

        if (cloud_folders_->count() > 0) {
            cloud_folders_->setCurrentRow(0);
            return;
        }
    }

    const auto* item = cloud_folders_->currentItem();
    if (item == nullptr) {
        cloud_->setHtml(QStringLiteral(
            "<p>Nenhuma pasta sincronizada com nuvem foi encontrada neste usuario.</p>"));
        cloud_release_button_->setEnabled(false);
        return;
    }

    const auto path = item->data(Qt::UserRole).toString();

    cloud_->setHtml(QStringLiteral("<p>Medindo %1...</p>").arg(path.toHtmlEscaped()));
    cloud_release_button_->setEnabled(false);
    QApplication::processEvents();

    const auto space = collectors::measure_cloud_folder(path.toStdWString());
    const auto plan = storage::plan_cloud_release(path.toStdWString());

    QString body = QStringLiteral("<h3>%1</h3>").arg(path.toHtmlEscaped());

    body += QStringLiteral(
                "<table cellspacing='0' cellpadding='6'>"
                "<tr><td><b>No disco</b></td><td>%1</td><td>%2 arquivos</td></tr>"
                "<tr><td><b>So na nuvem</b></td><td>%3</td><td>%4 arquivos</td></tr>"
                "</table>")
                .arg(QString::fromStdString(core::format_bytes(space.local_bytes)))
                .arg(space.local_files)
                .arg(QString::fromStdString(core::format_bytes(space.online_only_bytes)))
                .arg(space.online_only_files);

    // A distincao mais importante da aba inteira. Sem ela, o usuario apagaria um
    // arquivo achando que libera espaco e perderia o arquivo em todos os
    // dispositivos, sem liberar nada.
    body += QStringLiteral(
        "<p>O que esta <b>so na nuvem</b> nao ocupa espaco aqui. Apagar esses arquivos nao "
        "libera nada e remove o arquivo da nuvem e dos seus outros dispositivos — por isso o "
        "Cleaner nunca apaga nada dentro desta pasta.</p>");

    if (!space.complete) {
        body += QStringLiteral(
            "<p style='color:#E68A00'>A leitura nao terminou por completo. Os numeros acima "
            "sao um piso, nao o total.</p>");
    }

    if (plan.file_count > 0) {
        body += QStringLiteral(
                    "<p><b>Liberar espaco local</b> devolveria cerca de %1 (%2 arquivos). Os "
                    "arquivos continuam na nuvem e voltam sozinhos quando voce abrir cada um.</p>")
                    .arg(QString::fromStdString(core::format_bytes(plan.bytes)))
                    .arg(plan.file_count);
        cloud_release_button_->setEnabled(true);
    } else {
        body += QStringLiteral("<p>Nao ha nada baixado para liberar nesta pasta.</p>");
    }

    if (plan.pinned_kept > 0) {
        body += QStringLiteral(
                    "<p style='color:gray'>%1 arquivos estao marcados por voce como "
                    "\"sempre manter neste dispositivo\" e ficam de fora.</p>")
                    .arg(plan.pinned_kept);
    }

    cloud_->setHtml(body);
}

void MainWindow::release_cloud_space() {
    const auto* item = cloud_folders_->currentItem();
    if (item == nullptr) {
        return;
    }

    const auto path = item->data(Qt::UserRole).toString();
    const auto plan = storage::plan_cloud_release(path.toStdWString());

    QMessageBox confirmation(this);
    confirmation.setWindowTitle(QStringLiteral("Liberar espaco local"));
    confirmation.setTextFormat(Qt::RichText);
    confirmation.setText(
        QStringLiteral(
            "<p>Isto marca %1 arquivos (%2) para ficarem apenas na nuvem.</p>"
            "<p><b>Nenhum arquivo e apagado.</b> Eles continuam na nuvem e nos seus outros "
            "dispositivos, e voltam para o disco quando voce abrir cada um.</p>"
            "<p>O espaco nao aparece livre na hora: quem esvazia os arquivos e o servico de "
            "nuvem, no ritmo dele.</p>"
            "<p>Continuar?</p>")
            .arg(plan.file_count)
            .arg(QString::fromStdString(core::format_bytes(plan.bytes))));
    confirmation.addButton(QStringLiteral("Liberar espaco"), QMessageBox::AcceptRole);
    confirmation.addButton(QStringLiteral("Cancelar"), QMessageBox::RejectRole);
    confirmation.setDefaultButton(qobject_cast<QPushButton*>(confirmation.buttons().last()));
    confirmation.exec();

    if (confirmation.buttonRole(confirmation.clickedButton()) != QMessageBox::AcceptRole) {
        return;
    }

    cloud_release_button_->setEnabled(false);
    QApplication::processEvents();

    const auto outcome = storage::release_cloud_space(path.toStdWString());

    QString body = QStringLiteral(
                       "<p>%1 arquivos marcados para ficar so na nuvem, somando %2.</p>"
                       "<p>O espaco vai aparecer livre conforme o servico de nuvem esvazia cada "
                       "arquivo. Isso leva alguns minutos.</p>")
                       .arg(outcome.released)
                       .arg(QString::fromStdString(core::format_bytes(outcome.bytes)));

    if (outcome.failed > 0) {
        body += QStringLiteral(
                    "<p style='color:#E68A00'>%1 arquivos nao aceitaram a marca — normalmente "
                    "porque estao abertos em algum programa.</p>")
                    .arg(outcome.failed);
    }

    cloud_->setHtml(body);

    // Voltar a habilitar so se ainda houver o que liberar. Deixar o botao morto
    // apos um uso foi defeito relatado na limpeza, e a causa era exatamente
    // esta: desabilitar antes da acao e nunca reavaliar depois.
    cloud_release_button_->setEnabled(storage::plan_cloud_release(path.toStdWString()).file_count >
                                      0);

    monitor::ActionLog log{action_database()};
    if (log.ok()) {
        // Reversivel de verdade, e o unico caso ate agora: abrir o arquivo o traz
        // de volta. O historico pode dizer isso sem prometer o impossivel.
        log.record(monitor::ActionRecord{
            .kind = monitor::ActionKind::CommandRun,
            .reason = "Liberou espaco local na pasta de nuvem",
            .target = path.toStdString(),
            .item_count = outcome.released,
            .bytes = outcome.bytes,
            .reversible = true,
        });
    }

    spdlog::info("liberou espaco local em {}: {} arquivos", path.toStdString(), outcome.released);
}

QWidget* MainWindow::build_windows_tab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    auto* intro = new QLabel(
        QStringLiteral(
            "<p>Estas sao as limpezas que a propria Microsoft mantem. O Cleaner nao apaga nada "
            "aqui: ele chama o programa do Windows e mostra a resposta.</p>"),
        page);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    windows_commands_ = new QListWidget(page);
    for (const auto& command : commands::command_catalog()) {
        auto* item = new QListWidgetItem(QString::fromStdString(command.name), windows_commands_);
        item->setData(Qt::UserRole, static_cast<int>(command.id));
    }
    connect(windows_commands_, &QListWidget::currentRowChanged, this,
            &MainWindow::show_windows_command_details);
    layout->addWidget(windows_commands_);

    windows_output_ = new QTextBrowser(page);
    layout->addWidget(windows_output_, 1);

    auto* actions = new QHBoxLayout;

    windows_run_button_ = new QPushButton(QStringLiteral("Executar"), page);
    windows_run_button_->setEnabled(false);
    connect(windows_run_button_, &QPushButton::clicked, this,
            &MainWindow::run_selected_windows_command);
    actions->addWidget(windows_run_button_);

    // Fica fora da lista de comandos porque nao e um comando do Windows: e uma
    // conferencia que o Cleaner faz perguntando ao Windows Installer quem e
    // dono de que. Nenhum programa da Microsoft limpa essa pasta.
    installer_cache_button_ =
        new QPushButton(QStringLiteral("Conferir instaladores guardados"), page);
    connect(installer_cache_button_, &QPushButton::clicked, this,
            &MainWindow::review_installer_cache);
    actions->addWidget(installer_cache_button_);

    layout->addLayout(actions);

    return page;
}

void MainWindow::review_installer_cache() {
    installer_cache_button_->setEnabled(false);
    windows_output_->setHtml(QStringLiteral(
        "<p>Perguntando ao Windows quais instaladores ainda tem dono...</p>"));
    QApplication::processEvents();

    const auto report = commands::scan_installer_cache();
    installer_cache_button_->setEnabled(true);

    if (!report.products_readable) {
        windows_output_->setHtml(
            QStringLiteral("<p style='color:#E68A00'><b>Conferencia nao concluida.</b><br>%1</p>")
                .arg(QString::fromStdString(report.obstacle).toHtmlEscaped()));
        return;
    }

    QString body = QStringLiteral(
                       "<h3>Instaladores guardados pelo Windows</h3>"
                       "<table cellspacing='0' cellpadding='6'>"
                       "<tr><td><b>Ainda em uso</b></td><td>%1</td><td>%2 arquivos</td></tr>"
                       "<tr><td><b>Sem dono</b></td><td>%3</td><td>%4 arquivos</td></tr>"
                       "</table>")
                       .arg(QString::fromStdString(core::format_bytes(report.referenced_bytes)))
                       .arg(report.referenced_count)
                       .arg(QString::fromStdString(core::format_bytes(report.orphan_bytes)))
                       .arg(report.orphans.size());

    body += QStringLiteral(
        "<p>O Windows guarda uma copia do instalador de cada programa para poder reparar e "
        "desinstalar depois, e nunca remove essa copia sozinho — nem quando o programa e "
        "desinstalado. Os <b>sem dono</b> sao os que nenhum programa instalado reivindica.</p>");

    if (report.orphans.empty()) {
        body += QStringLiteral("<p>Nada a remover aqui.</p>");
        windows_output_->setHtml(body);
        return;
    }

    body += QStringLiteral("<p><b>Os maiores:</b></p><ul>");
    for (std::size_t index = 0; index < std::min<std::size_t>(10, report.orphans.size()); ++index) {
        const auto& orphan = report.orphans.at(index);
        body += QStringLiteral("<li>%1 — %2</li>")
                    .arg(QString::fromStdString(std::filesystem::path(orphan.path)
                                                    .filename()
                                                    .string())
                             .toHtmlEscaped())
                    .arg(QString::fromStdString(core::format_bytes(orphan.bytes)));
    }
    body += QStringLiteral("</ul>");

    // Analise sim, botao nao. Apagar um instalador que ainda tem dono quebra
    // reparar e desinstalar aquele programa, e nao ha volta: nem quarentena,
    // nem restauracao. Enquanto essas duas pecas nao existirem, esta pasta
    // continua como protegida e o numero serve so para o usuario saber onde
    // esta o espaco.
    body += QStringLiteral(
        "<p style='color:#777'><i>O Cleaner nao remove nada aqui. A separacao acima veio do "
        "proprio Windows Installer, mas apagar por engano um instalador em uso quebraria "
        "reparar e desinstalar aquele programa, sem volta. A remocao so sera oferecida quando "
        "houver quarentena e restauracao.</i></p>");

    windows_output_->setHtml(body);
}

/// Mantida fora do fluxo enquanto nao houver quarentena e restauracao. O codigo
/// fica: a decisao foi adiar a oferta, nao descartar o trabalho.
void MainWindow::remove_orphan_installers_after_confirmation(
    const commands::InstallerCacheReport& report) {
    QMessageBox confirmation(this);
    confirmation.setWindowTitle(QStringLiteral("Remover instaladores sem dono"));
    confirmation.setTextFormat(Qt::RichText);
    confirmation.setText(
        QStringLiteral(
            "<p>Remover %1 instaladores, liberando %2?</p>"
            "<p><b>Sem volta.</b> Sao copias de instaladores de programas que nao estao mais "
            "instalados. O que se perde e a possibilidade de reparar ou desinstalar pelo painel "
            "do Windows um programa que ja saiu do computador.</p>"
            "<p>Nenhum instalador em uso entra nesta lista: a separacao veio do proprio "
            "Windows Installer, nao de palpite sobre o nome do arquivo.</p>")
            .arg(report.orphans.size())
            .arg(QString::fromStdString(core::format_bytes(report.orphan_bytes))));
    confirmation.addButton(QStringLiteral("Remover"), QMessageBox::AcceptRole);
    confirmation.addButton(QStringLiteral("Cancelar"), QMessageBox::RejectRole);
    confirmation.setDefaultButton(qobject_cast<QPushButton*>(confirmation.buttons().last()));
    confirmation.exec();

    if (confirmation.buttonRole(confirmation.clickedButton()) != QMessageBox::AcceptRole) {
        return;
    }

    installer_cache_button_->setEnabled(false);
    QApplication::processEvents();

    const auto outcome = commands::remove_orphan_installers(report.orphans);
    installer_cache_button_->setEnabled(true);

    QString resultado = QStringLiteral("<p><b>%1 instaladores removidos, %2 liberados.</b></p>")
                            .arg(outcome.removed)
                            .arg(QString::fromStdString(core::format_bytes(outcome.bytes)));

    if (outcome.needs_elevation) {
        resultado += QStringLiteral(
            "<p style='color:#E68A00'>Parte deles exige o Cleaner aberto como administrador.</p>");
    } else if (outcome.failed > 0) {
        resultado += QStringLiteral("<p style='color:#E68A00'>%1 nao puderam ser removidos.</p>")
                         .arg(outcome.failed);
    }

    windows_output_->setHtml(resultado);

    monitor::ActionLog log{action_database()};
    if (log.ok()) {
        log.record(monitor::ActionRecord{
            .kind = monitor::ActionKind::Deleted,
            .reason = "Instaladores guardados sem programa dono",
            .target = "Windows\\Installer",
            .item_count = outcome.removed,
            .bytes = outcome.bytes,
            .skipped_count = outcome.failed,
            .reversible = false,
        });
    }

    spdlog::info("instaladores orfaos removidos: {} arquivos, {} bytes", outcome.removed,
                 outcome.bytes);
}

namespace {

/// O comando selecionado, ou nulo quando nao ha selecao.
const commands::OfficialCommand* selected_command(const QListWidget* list) {
    const auto* item = list->currentItem();
    if (item == nullptr) {
        return nullptr;
    }

    const auto id = static_cast<commands::CommandId>(item->data(Qt::UserRole).toInt());
    return &commands::command_by_id(id);
}

}

void MainWindow::show_windows_command_details() {
    const auto* command = selected_command(windows_commands_);
    if (command == nullptr) {
        windows_run_button_->setEnabled(false);
        return;
    }

    windows_run_button_->setEnabled(true);

    QString body = QStringLiteral("<p>%1</p>")
                       .arg(QString::fromStdString(command->purpose).toHtmlEscaped());

    if (command->requires_elevation && !commands::running_elevated()) {
        body += QStringLiteral(
            "<p style='color:#E68A00'>Este comando so funciona com o Cleaner aberto como "
            "administrador. Feche e abra de novo com o botao direito, em "
            "\"Executar como administrador\".</p>");
    }

    if (!command->irreversible_effect.empty()) {
        body += QStringLiteral("<p style='color:#C0392B'><b>Sem volta:</b> %1</p>")
                    .arg(QString::fromStdString(command->irreversible_effect).toHtmlEscaped());
    }

    // Mostrar a linha exata tira o misterio: da para conferir o comando fora do
    // Cleaner antes de deixar rodar.
    QString line = QString::fromStdString(command->executable);
    for (const auto& argument : command->arguments) {
        line += QLatin1Char(' ') + QString::fromStdString(argument);
    }
    body += QStringLiteral("<p style='color:gray'>Linha executada: <code>%1</code></p>")
                .arg(line.toHtmlEscaped());

    windows_output_->setHtml(body);
}

void MainWindow::run_selected_windows_command() {
    const auto* command = selected_command(windows_commands_);
    if (command == nullptr) {
        return;
    }

    if (command->modifies_system) {
        QMessageBox confirmation(this);
        confirmation.setWindowTitle(QStringLiteral("Confirmar limpeza do Windows"));
        confirmation.setTextFormat(Qt::RichText);
        confirmation.setText(
            QStringLiteral("<p>%1</p><p><b>Sem volta:</b> %2</p><p>Executar agora?</p>")
                .arg(QString::fromStdString(command->purpose).toHtmlEscaped(),
                     QString::fromStdString(command->irreversible_effect).toHtmlEscaped()));
        confirmation.addButton(QStringLiteral("Executar"), QMessageBox::AcceptRole);
        confirmation.addButton(QStringLiteral("Cancelar"), QMessageBox::RejectRole);
        // O botao de partida e o de cancelar: apertar Enter sem ler nao pode
        // acionar uma mudanca definitiva.
        confirmation.setDefaultButton(qobject_cast<QPushButton*>(confirmation.buttons().last()));
        confirmation.exec();

        if (confirmation.buttonRole(confirmation.clickedButton()) != QMessageBox::AcceptRole) {
            return;
        }
    }

    windows_run_button_->setEnabled(false);
    windows_output_->setHtml(QStringLiteral(
        "<p>Executando. Uma limpeza do Windows pode levar varios minutos.</p>"));
    QApplication::processEvents();

    const auto result = commands::run_official_command(
        commands::CommandRequest{.id = command->id, .confirmed = true});

    QString body = QStringLiteral("<p><b>%1</b></p><p>%2</p>")
                       .arg(QString::fromStdString(command->name).toHtmlEscaped(),
                            QString::fromStdString(result.explanation).toHtmlEscaped());

    if (!result.output.empty()) {
        body += QStringLiteral("<pre style='white-space:pre-wrap'>%1</pre>")
                    .arg(QString::fromStdString(result.output).toHtmlEscaped());
    }

    windows_output_->setHtml(body);
    windows_run_button_->setEnabled(true);

    // Registrar mesmo o que nao alterou nada: quem le o historico depois quer
    // saber o que foi acionado, nao so o que deu certo.
    monitor::ActionLog log{action_database()};
    if (log.ok()) {
        log.record(monitor::ActionRecord{
            .kind = monitor::ActionKind::CommandRun,
            .reason = command->name + " — " + result.explanation,
            .target = command->executable,
            .reversible = false,
        });
    }

    spdlog::info("comando oficial {} terminou com codigo {}", command->executable,
                 result.exit_code);
}

QWidget* MainWindow::build_history_tab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    auto* search_row = new QHBoxLayout;
    search_row->addWidget(new QLabel(QStringLiteral("Procurar:"), page));

    history_search_ = new QLineEdit(page);
    history_search_->setPlaceholderText(QStringLiteral("pasta ou motivo"));
    connect(history_search_, &QLineEdit::textChanged, this, &MainWindow::show_history);
    search_row->addWidget(history_search_, 1);

    layout->addLayout(search_row);

    history_ = new QTextBrowser(page);
    layout->addWidget(history_, 1);

    return page;
}

void MainWindow::show_history() {
    const monitor::ActionLog log{action_database()};
    if (!log.ok()) {
        history_->setHtml(QStringLiteral("<p>O historico nao pode ser aberto.</p>"));
        return;
    }

    const auto term = history_search_->text().trimmed().toStdString();
    const auto actions = term.empty() ? log.recent() : log.search(term);

    if (actions.empty()) {
        history_->setHtml(
            term.empty()
                ? QStringLiteral("<p>Nenhuma limpeza foi feita ainda.</p>")
                : QStringLiteral("<p>Nada encontrado para esse termo.</p>"));
        return;
    }

    QString body = QStringLiteral("<p><b>Total liberado ate agora: %1</b></p>")
                       .arg(QString::fromStdString(core::format_bytes(log.total_freed_bytes())));

    body += QStringLiteral("<table cellspacing='0' cellpadding='6' width='100%'>");
    for (const auto& action : actions) {
        body += QStringLiteral(
                    "<tr><td valign='top'><b>%1</b><br><span style='color:gray'>%2</span></td>"
                    "<td valign='top'>%3<br><span style='color:gray'>%4</span></td></tr>")
                    .arg(QString::fromStdString(action.reason).toHtmlEscaped(),
                         QString::fromStdString(action.at),
                         QStringLiteral("%1 arquivos, %2 (%3)")
                             .arg(action.item_count)
                             .arg(QString::fromStdString(core::format_bytes(action.bytes)),
                                  action_kind_label(action.kind)),
                         QString::fromStdString(action.target).toHtmlEscaped());
    }
    body += QStringLiteral("</table>");

    // Ser explicito sobre isso e o compromisso do projeto: nunca prometer
    // devolver o que ja nao existe.
    body += QStringLiteral(
        "<p style='color:gray'><i>O historico registra o que foi feito. Arquivos apagados de vez "
        "nao podem ser devolvidos por aqui — quando eles voltam, e o proprio programa dono que os "
        "recria.</i></p>");

    history_->setHtml(body);
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
    if (busy_) {
        return;
    }
    busy_ = true;

    analyze_button_->setEnabled(false);
    analyze_button_->setText(QStringLiteral("Analisando..."));
    summary_label_->setText(QStringLiteral("Lendo discos, inicializacao e temporarios..."));

    spdlog::info("analise iniciada");

    const auto version = QApplication::applicationVersion().toStdString();

    // A coleta le disco, registro, WMI e log de eventos. Nada disso toca widget,
    // entao roda inteira fora da thread da interface.
    run_in_background<core::AnalysisResult>(
        this,
        [] { return core::QuickAnalysis::with_default_rules().run(collectors::collect_snapshot()); },
        [this, version](core::AnalysisResult analysis) {
            result_ = std::move(analysis);

            // O que nao pode ser observado vira registro. Sem isso, uma coleta
            // que falha silenciosamente e indistinguivel de uma que nao achou
            // nada.
            for (const auto& area : result_.unavailable) {
                spdlog::warn("nao foi possivel analisar: {}", area);
            }
            spdlog::info("analise concluida: saude {}, {} achados", result_.health.overall(),
                         result_.recommendations.size());

            show_result(result_);

            storage::StoredSession session;
            session.id =
                QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HHmmss_quick"))
                    .toStdString();
            session.started_at = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();
            session.app_version = version;
            session.result = result_;

            const storage::HistoryStore history{storage::default_data_directory() / "history"};
            history.save(session);
            history.apply_retention(50);

            if (const auto quarantined = history.quarantine_unreadable(); quarantined > 0) {
                spdlog::warn("{} arquivos de historico ilegiveis foram postos de quarentena",
                             quarantined);
            }
            spdlog::info("analise gravada em {}", session.id);

            analyze_button_->setEnabled(true);
            analyze_button_->setText(QStringLiteral("Analisar de novo"));
            busy_ = false;
        });
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

    // O botao aparece nos achados cuja acao o Cleaner sabe executar com seguranca
    // e desfazer. Nos demais fica escondido: botao cinza sugere que existe uma
    // acao quando, para a maioria dos achados, nao existe nenhuma que o
    // aplicativo deva tomar sozinho.
    // Vermelho e desconhecido nunca ganham botao. O primeiro porque o
    // aplicativo nao deve agir; o segundo porque ele nao sabe o que ha ali, e
    // nao saber e o motivo mais forte para nao mexer.
    // Achado de perfil de aplicativo entra aqui pelo mesmo criterio dos demais:
    // o que decide e o risco, nao a regra que produziu o achado. Ficar de fora
    // fazia o maior item da lista — gigabytes de sobra de ferramenta, marcados
    // como seguros — nao ter botao nenhum.
    const bool cleanable = recommendation != nullptr && core::app_may_execute(*recommendation) &&
                           !recommendation->affected_paths.empty() &&
                           (recommendation->rule_id == "storage.excessive-temporary-files" ||
                            recommendation->rule_id == "storage.reclaimable-location" ||
                            recommendation->rule_id == "apps.profile-item");

    // Inicializacao nao libera espaco, entao nao passa pelo criterio de limpeza.
    // Mas tem uma acao segura e que se desfaz por completo, e um achado sem
    // nenhum caminho de saida e so uma reclamacao.
    const bool startup = recommendation != nullptr &&
                         recommendation->rule_id == "startup.too-many-items";

    action_button_->setVisible(cleanable || startup);

    if (startup) {
        action_button_->setText(QStringLiteral("Escolher o que inicia com o Windows"));
    } else if (cleanable) {
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

    if (recommendation->rule_id == "startup.too-many-items") {
        manage_startup();
        return;
    }

    const std::string rule_id = recommendation->rule_id;
    const std::string title = recommendation->title;
    const std::string limitations = recommendation->limitations;
    const std::vector<std::string> affected = recommendation->affected_paths;

    if (busy_) {
        return;
    }
    busy_ = true;

    action_button_->setEnabled(false);
    action_button_->setText(QStringLiteral("Verificando o que pode ser removido..."));

    // Levantar o plano percorre as pastas envolvidas — centenas de milhares de
    // arquivos no caso do cache de pacotes. Fica fora da thread da interface.
    run_in_background<core::CleanupPlan>(
        this,
        [rule_id, affected] {
            const auto protected_paths =
                collectors::build_protected_paths(collectors::collect_system_paths());

            const storage::QuarantineStore quarantine{
                storage::default_data_directory() / "quarentena", protected_paths};
            const storage::CleanupService cleanup{quarantine, protected_paths};

            core::CleanupPlan plan;
            if (rule_id == "storage.excessive-temporary-files") {
                const collectors::TemporaryFilesCollector collector{protected_paths};

                std::vector<std::string> paths;
                for (const auto& file : collector.list_files()) {
                    paths.push_back(file.string());
                }
                return cleanup.plan(paths, rule_id);
            }

            // Locais do catalogo sao pastas: o conteudo e levantado agora, para
            // refletir o disco no momento de agir.
            for (const auto& folder : affected) {
                core::CleanupPlan folder_plan = cleanup.plan_folder(folder, rule_id);
                plan.items.insert(plan.items.end(), folder_plan.items.begin(),
                                  folder_plan.items.end());
                plan.rejected.insert(plan.rejected.end(), folder_plan.rejected.begin(),
                                     folder_plan.rejected.end());
            }
            return plan;
        },
        [this, rule_id, title, limitations, affected](core::CleanupPlan plan) {
            busy_ = false;
            confirm_and_clean(std::move(plan), rule_id, title, limitations, affected);
        });
}

void MainWindow::confirm_and_clean(core::CleanupPlan plan, const std::string& rule_id,
                                   const std::string& title, const std::string& limitations,
                                   const std::vector<std::string>& affected) {
    static_cast<void>(rule_id);

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
    details += QStringLiteral("Os arquivos sao apagados de vez e o espaco e liberado na hora.\n\n");

    // Avisa antes, e nomeando o programa. "Arquivos em uso nao serao removidos"
    // e vago; dizer que o Chrome esta aberto permite ao usuario decidir se
    // fecha antes ou aceita liberar menos. O Cleaner nunca encerra nada sozinho:
    // isso custaria o trabalho nao salvo.
    if (const auto abertos = running_apps_holding(affected); !abertos.isEmpty()) {
        details += QStringLiteral(
                       "%1 esta aberto agora. Os arquivos que ele estiver usando ficam onde "
                       "estao, e o espaco liberado sera menor que a estimativa. Fechar o "
                       "programa antes costuma liberar bem mais.\n\n")
                       .arg(abertos);
    } else {
        details += QStringLiteral(
            "Programas abertos podem estar usando parte deles; esses ficam onde estao e o "
            "espaco liberado sera menor que o estimado.");
    }

    confirmation.setInformativeText(details);

    auto* confirm = confirmation.addButton(QStringLiteral("Limpar"), QMessageBox::AcceptRole);
    confirmation.addButton(QStringLiteral("Cancelar"), QMessageBox::RejectRole);
    confirmation.setDefaultButton(qobject_cast<QPushButton*>(confirmation.buttons().last()));
    confirmation.exec();

    if (confirmation.clickedButton() != confirm) {
        spdlog::info("limpeza cancelada pelo usuario");
        return;
    }

    busy_ = true;
    action_button_->setEnabled(false);
    action_button_->setText(QStringLiteral("Limpando..."));

    const std::string target =
        affected.empty() ? std::string{"arquivos temporarios do sistema"} : affected.front();

    // Apagar centenas de milhares de arquivos leva minutos. Antes disso a janela
    // ficava branca e sem responder, e nao havia como distinguir "trabalhando"
    // de "travou".
    run_in_background<core::CleanupOutcome>(
        this,
        [plan = std::move(plan), title, target] {
            const auto protected_paths =
                collectors::build_protected_paths(collectors::collect_system_paths());
            const storage::QuarantineStore quarantine{
                storage::default_data_directory() / "quarentena", protected_paths};
            const storage::CleanupService cleanup{quarantine, protected_paths};

            // Conteudo do catalogo e recriado pelo programa dono, entao guardar
            // copia ocuparia o mesmo espaco que se queria liberar.
            const auto outcome = cleanup.execute(plan, storage::RemovalMode::Delete);

            // Sem quarentena, o registro e a unica rastreabilidade que sobra.
            // Ele nao desfaz nada — e justamente por isso a acao precisa ficar
            // anotada.
            monitor::ActionLog log{action_database()};
            log.record(monitor::ActionRecord{
                .kind = monitor::ActionKind::Deleted,
                .reason = title,
                .target = target,
                .item_count = outcome.removed_count,
                .bytes = outcome.freed_bytes,
                .skipped_count = outcome.skipped.size(),
                .reversible = false,
            });

            return outcome;
        },
        [this](core::CleanupOutcome outcome) {
            QString report = QStringLiteral("%1 arquivos removidos, %2 liberados.")
                                 .arg(outcome.removed_count)
                                 .arg(QString::fromStdString(
                                     core::format_bytes(outcome.freed_bytes)));
            if (!outcome.skipped.empty()) {
                report += QStringLiteral(
                              "\n\n%1 arquivos ficaram onde estavam, em geral por estarem "
                              "em uso por algum programa aberto. Fechar o programa e limpar "
                              "de novo costuma resolver.")
                              .arg(outcome.skipped.size());
            }

            QMessageBox::information(this, QStringLiteral("Limpeza concluida"), report);
            show_history();

            // O botao volta a funcionar antes da reanalise. Sem isto, limpar um
            // achado deixava todos os outros sem acao.
            action_button_->setEnabled(true);
            busy_ = false;

            // Reanalisa: a pontuacao e o espaco livre mudaram, e mostrar numero
            // velho depois de agir seria enganoso.
            start_analysis();
        });
}

namespace {

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
    if (busy_) {
        return;
    }
    busy_ = true;

    snapshot_button_->setEnabled(false);
    snapshot_progress_->setText(QStringLiteral("Percorrendo o disco... isto leva alguns minutos."));

    struct SnapshotOutcome {
        QString message;
        bool saved = false;
    };

    // Varrer o C: inteiro passa de tres minutos nesta maquina. Era o trecho que
    // mais tempo mantinha a janela sem responder — e o unico jeito de saber se
    // o programa tinha travado era esperar.
    //
    // O banco e aberto dentro da propria thread de trabalho: conexao do Qt
    // pertence a quem a criou, e usa-la de outra thread e defeito silencioso.
    run_in_background<SnapshotOutcome>(
        this,
        [] {
            monitor::SnapshotStore store{snapshot_database()};
            if (!store.ok()) {
                return SnapshotOutcome{
                    QStringLiteral("Nao foi possivel abrir o banco de retratos.")};
            }

            const monitor::SnapshotTaker taker{monitor::SnapshotOptions{
                // Sem isto o proprio banco de retratos apareceria como consumo
                // misterioso na proxima comparacao.
                .excluded_paths = {storage::default_data_directory().string()},
            }};

            spdlog::info("retrato iniciado");
            const auto snapshot = taker.take("C:\\", "C:");

            if (!snapshot.complete) {
                return SnapshotOutcome{
                    QStringLiteral("A varredura nao terminou; o retrato nao foi guardado.")};
            }

            if (store.save(snapshot) == 0) {
                return SnapshotOutcome{QStringLiteral("Nao foi possivel guardar o retrato.")};
            }

            store.apply_retention();
            spdlog::info("retrato guardado: {} pastas", snapshot.folders.size());

            return SnapshotOutcome{
                QStringLiteral("Retrato guardado (%1 pastas). Banco: %2.")
                    .arg(snapshot.folders.size())
                    .arg(QString::fromStdString(core::format_bytes(store.database_size_bytes()))),
                true};
        },
        [this](SnapshotOutcome outcome) {
            snapshot_progress_->setText(outcome.message);
            snapshot_button_->setEnabled(true);
            busy_ = false;

            if (outcome.saved) {
                show_growth();
            }
        });
}

QString MainWindow::describe_recent_activity() const {
    if (watcher_ == nullptr) {
        return {};
    }

    auto activity = watcher_->collect();
    if (activity.empty()) {
        return {};
    }

    std::sort(activity.begin(), activity.end(),
              [](const monitor::FolderActivity& left, const monitor::FolderActivity& right) {
                  return left.event_count > right.event_count;
              });

    QString body = QStringLiteral(
        "<hr><p><b>Atividade desde que o Cleaner foi aberto</b><br>"
        "<span style='color:gray'>Onde houve escrita e quando. O tamanho nao aparece aqui: o "
        "aviso do sistema informa qual arquivo mudou, nunca quanto ele ocupa — esse numero vem "
        "dos retratos, que medem de verdade.</span></p><ul>");

    for (std::size_t index = 0; index < std::min<std::size_t>(8, activity.size()); ++index) {
        const auto& entry = activity[index];

        body += QStringLiteral("<li><b>%1</b><br>%2 gravacoes em %3 arquivos, entre %4 e %5</li>")
                    .arg(QString::fromStdString(entry.folder).toHtmlEscaped())
                    .arg(entry.event_count)
                    .arg(entry.distinct_files)
                    .arg(QString::fromStdString(entry.first_seen),
                         QString::fromStdString(entry.last_seen));
    }

    body += QStringLiteral("</ul>");
    return body;
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

    QString warnings;
    // O volume vem do proprio retrato, nao fixo no codigo: assim continua certo
    // no dia em que houver retrato de outro disco.
    if (const auto atual = store.latest(snapshots.front().volume); atual.has_value()) {
        for (const auto& alert : monitor::evaluate_alerts(*atual, report)) {
            warnings += QStringLiteral(
                            "<p style='color:#E68A00'><b>%1</b><br>%2<br>"
                            "<span style='color:gray'>%3</span></p>")
                            .arg(QString::fromStdString(alert.title).toHtmlEscaped(),
                                 QString::fromStdString(alert.evidence).toHtmlEscaped(),
                                 QString::fromStdString(alert.suggested_action).toHtmlEscaped());
        }
    }

    const bool lost = report.free_space_delta < 0;
    const auto amount = static_cast<std::uint64_t>(std::abs(report.free_space_delta));

    growth_summary_->setText(
        QStringLiteral("<b>Desde %1, o disco C: %2 %3 de espaco livre.</b>")
            .arg(QString::fromStdString(report.from_taken_at),
                 lost ? QStringLiteral("perdeu") : QStringLiteral("recuperou"),
                 QString::fromStdString(core::format_bytes(amount))));

    QString body = warnings;

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

    body += describe_recent_activity();

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
            "<p>Isto nao significa que o computador esta perfeito: o Cleaner ainda analisa apenas "
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

    // O texto antigo dizia que nada era alterado. Isso deixou de ser verdade
    // quando a limpeza passou a apagar de fato, e um aviso desatualizado e pior
    // do que aviso nenhum: ele convida a aprovar sem ler.
    html += QStringLiteral(
        "<p style='color:#777'><i>Nada e removido sem voce apertar o botao e confirmar a lista. "
        "O que sai, sai de vez, e fica registrado no historico.</i></p>");

    details_->setHtml(html);
}

}
