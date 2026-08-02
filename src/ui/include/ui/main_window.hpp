#pragma once

#include <commands/installer_cache.hpp>
#include <core/models/cleanup_plan.hpp>
#include <core/models/space_tree.hpp>
#include <core/usecases/quick_analysis.hpp>
#include <monitor/folder_watcher.hpp>

#include <memory>
#include <stop_token>

#include <QMainWindow>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QTextBrowser;
class QTreeWidget;

namespace cleaner::ui {

/// Painel principal. Apresenta o resultado da analise e conduz a limpeza.
///
/// Nao coleta, nao classifica risco e nao decide o que remover: pede tudo isso
/// as camadas de baixo. O que ela faz e mostrar, perguntar e relatar.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

public slots:
    /// Dispara a analise. Publico para permitir `cleaner --analisar`, util em
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

    /// Segunda metade da limpeza: com o plano ja levantado, mostra o que sairia,
    /// pergunta e so entao executa. Separada porque o levantamento acontece fora
    /// da thread da interface e volta para ca.
    void confirm_and_clean(core::CleanupPlan plan, const std::string& rule_id,
                           const std::string& title, const std::string& limitations,
                           const std::vector<std::string>& affected);

    void update_action_button(int index);

    /// Abre a lista de programas que iniciam com o Windows para o usuario ligar
    /// e desligar cada um. Desativar aqui nao desinstala nada e se desfaz
    /// marcando a caixa de novo.
    void manage_startup();

    /// O achado selecionado, ou nulo quando nao ha selecao.
    [[nodiscard]] const core::Recommendation* selected_recommendation() const;

    QWidget* build_growth_tab();

    /// Percorre o disco e guarda o retrato. Leva minutos, entao e sempre uma
    /// acao pedida pelo usuario, com progresso a vista.
    void take_snapshot();

    /// Mostra o que mudou entre os dois retratos mais recentes.
    void show_growth();

    QWidget* build_space_tab();

    /// Varre a pasta escolhida e monta a arvore, com a conta fechada contra o
    /// que o Windows informa sobre o volume.
    void survey_space();

    /// Cancela a varredura em curso.
    void cancel_survey();

    void show_survey(const core::SpaceSurvey& survey);

    QWidget* build_cloud_tab();

    /// Mede as pastas sincronizadas, separando o que ocupa disco do que so
    /// existe na nuvem.
    void show_cloud();

    /// Marca os arquivos baixados como "liberar espaco". Nao apaga nada: o
    /// arquivo continua na nuvem e nos outros dispositivos.
    void release_cloud_space();

    QWidget* build_windows_tab();

    /// Aciona a limpeza oficial escolhida. O Cleaner nao apaga nada aqui: quem
    /// remove e o proprio Windows, pelo caminho que a Microsoft mantem.
    void run_selected_windows_command();

    void show_windows_command_details();

    /// Confere quais instaladores guardados pelo Windows nenhum programa
    /// instalado ainda reivindica, e oferece removê-los.
    void review_installer_cache();

    /// A remocao propriamente dita. Nao esta ligada a nenhum botao: apagar um
    /// instalador que ainda tem dono quebra reparar e desinstalar aquele
    /// programa, e sem quarentena nao ha como voltar atras.
    void remove_orphan_installers_after_confirmation(
        const commands::InstallerCacheReport& report);

    QWidget* build_history_tab();

    /// Lista o que o Cleaner ja fez no computador, filtrado pela busca.
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
    QComboBox* space_root_ = nullptr;
    QTreeWidget* space_tree_ = nullptr;
    QLabel* space_summary_ = nullptr;
    QPushButton* space_button_ = nullptr;
    QPushButton* space_cancel_ = nullptr;
    QLabel* space_progress_ = nullptr;

    /// Vive enquanto a varredura de espaco roda. Existe para o botao de cancelar
    /// ter o que pedir.
    std::unique_ptr<std::stop_source> survey_stop_;

    QListWidget* cloud_folders_ = nullptr;
    QTextBrowser* cloud_ = nullptr;
    QPushButton* cloud_release_button_ = nullptr;

    QListWidget* windows_commands_ = nullptr;
    QTextBrowser* windows_output_ = nullptr;
    QPushButton* windows_run_button_ = nullptr;
    QPushButton* installer_cache_button_ = nullptr;

    QTextBrowser* history_ = nullptr;
    QLineEdit* history_search_ = nullptr;
    QLabel* snapshot_progress_ = nullptr;

    core::AnalysisResult result_;

    /// Ha um trabalho demorado em andamento.
    ///
    /// Enquanto ele roda a janela continua respondendo, e e justamente por isso
    /// que o segundo clique precisa ser barrado: antes, a tela travada fazia
    /// esse papel por acidente.
    bool busy_ = false;

    /// Vive enquanto a janela existe: fechar o aplicativo encerra a observacao.
    std::unique_ptr<monitor::FolderWatcher> watcher_;
};

}
