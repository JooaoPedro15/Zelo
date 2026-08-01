#pragma once

#include <functional>
#include <memory>
#include <utility>

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QRunnable>
#include <QThreadPool>

namespace cleaner::ui {

/// Roda um trabalho demorado fora da thread da interface e entrega o resultado
/// de volta nela.
///
/// A janela ficava parada enquanto varria disco, apagava arquivo ou esperava um
/// comando do Windows. `processEvents` no meio do laco nao resolve isso: ele
/// redesenha a tela mas deixa o clique seguinte entrar no meio da operacao em
/// curso, que e como um botao acaba disparando duas vezes a mesma limpeza.
///
/// `work` roda na thread de trabalho e nao pode tocar em widget nenhum.
/// `done` roda na thread da interface e e onde a tela e atualizada.
///
/// Se a janela for destruida antes do fim, `done` simplesmente nao acontece — o
/// trabalho termina sozinho e o resultado e descartado.
template <typename Result>
void run_in_background(QObject* owner, std::function<Result()> work,
                       std::function<void(Result)> done) {
    class Task : public QRunnable {
    public:
        Task(QObject* owner, std::function<Result()> work, std::function<void(Result)> done)
            : owner_(owner), work_(std::move(work)), done_(std::move(done)) {
            setAutoDelete(true);
        }

        void run() override {
            auto result = std::make_shared<Result>(work_());

            if (owner_.isNull()) {
                return;
            }

            // A entrega vai enfileirada para o dono: quem executa `done` e a
            // thread dele, que e a da interface.
            QMetaObject::invokeMethod(
                owner_, [done = done_, result] { done(*result); }, Qt::QueuedConnection);
        }

    private:
        QPointer<QObject> owner_;
        std::function<Result()> work_;
        std::function<void(Result)> done_;
    };

    QThreadPool::globalInstance()->start(new Task(owner, std::move(work), std::move(done)));
}

}
