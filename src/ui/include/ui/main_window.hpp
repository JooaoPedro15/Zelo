#pragma once

#include <core/usecases/quick_analysis.hpp>
#include <monitor/folder_watcher.hpp>

#include <memory>

#include <QMainWindow>

class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QTextBrowser;

namespace zelo::ui {

/// Painel principal. Apresenta o resultado da analise e conduz a limpeza.
///
/// Nao coleta, nao classifica risco e nao decide o que remover: pede tudo isso
/// as camadas de baixo. O que ela faz e mostrar, perguntar e relatar.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

public slots:
    /// Dispara a analise. Publico para permitir `zelo --analisar`, util em
    /// verificacao automatizada e para quem quer abrir ja com o resultado.
    void start_analysis();

public:
    [[nodiscard]] const core::AnalysisResult& result() const;

private:
    void show_result(const core::AnalysisResult& result);
    void show_details(int index);

    /// Monta o plano, mostra exatamente o que sairia e so executa se o usuario
    /// confirmar. A confirmacao e sempre explicita: limpar apaga arquivo, ainda
    /// que a quarentena permita voltar atras.
    void clean_selected_finding();

    void update_action_button(int index);

    /// O achado selecionado, ou nulo quando nao ha selecao.
    [[nodiscard]] const core::Recommendation* selected_recommendation() const;

    QWidget* build_growth_tab();

    /// Percorre o disco e guarda o retrato. Leva minutos, entao e sempre uma
    /// acao pedida pelo usuario, com progresso a vista.
    void take_snapshot();

    /// Mostra o que mudou entre os dois retratos mais recentes.
    void show_growth();

    QWidget* build_history_tab();

    /// Lista o que o Zelo ja fez no computador, filtrado pela busca.
    void show_history();

    /// Passa a observar as pastas que mais recebem escrita, enquanto a janela
    /// estiver aberta. E o que da hora ao crescimento: o retrato diz que uma
    /// pasta cresceu, a observacao diz quando.
    void start_watching();

    /// Resumo do que foi escrito enquanto a janela esteve aberta.
    [[nodiscard]] QString describe_recent_activity() const;

    QLabel* score_label_ = nullptr;
    QLabel* summary_label_ = nullptr;
    QProgressBar* score_bar_ = nullptr;
    QLabel* categories_label_ = nullptr;
    QListWidget* findings_ = nullptr;
    QTextBrowser* details_ = nullptr;
    QPushButton* analyze_button_ = nullptr;
    QPushButton* action_button_ = nullptr;

    QTabWidget* tabs_ = nullptr;
    QLabel* growth_summary_ = nullptr;
    QTextBrowser* growth_ = nullptr;
    QPushButton* snapshot_button_ = nullptr;
    QTextBrowser* history_ = nullptr;
    QLineEdit* history_search_ = nullptr;
    QLabel* snapshot_progress_ = nullptr;

    core::AnalysisResult result_;

    /// Vive enquanto a janela existe: fechar o aplicativo encerra a observacao.
    std::unique_ptr<monitor::FolderWatcher> watcher_;
};

}
