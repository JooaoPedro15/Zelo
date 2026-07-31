#pragma once

#include <core/usecases/quick_analysis.hpp>

#include <QMainWindow>

class QLabel;
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

    QLabel* score_label_ = nullptr;
    QLabel* summary_label_ = nullptr;
    QProgressBar* score_bar_ = nullptr;
    QLabel* categories_label_ = nullptr;
    QListWidget* findings_ = nullptr;
    QTextBrowser* details_ = nullptr;
    QPushButton* analyze_button_ = nullptr;
    QPushButton* action_button_ = nullptr;

    core::AnalysisResult result_;
};

}
