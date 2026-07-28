#pragma once

#include <core/usecases/quick_analysis.hpp>

#include <QMainWindow>

class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QTextBrowser;

namespace zelo::ui {

/// Painel principal. Apresenta o resultado da analise e nada mais: nao coleta,
/// nao classifica risco e nao executa nenhuma acao sobre o sistema.
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

    QLabel* score_label_ = nullptr;
    QLabel* summary_label_ = nullptr;
    QProgressBar* score_bar_ = nullptr;
    QLabel* categories_label_ = nullptr;
    QListWidget* findings_ = nullptr;
    QTextBrowser* details_ = nullptr;
    QPushButton* analyze_button_ = nullptr;

    core::AnalysisResult result_;
};

}
