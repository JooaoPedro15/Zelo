#include <catch2/catch_session.hpp>

#include <QCoreApplication>

/// Ponto de entrada proprio para os testes.
///
/// O Qt carrega o driver de banco como plugin, e a busca por plugins depende de
/// uma instancia de aplicativo existir. Sem ela, abrir o banco falha com uma
/// mensagem que nao explica nada — e o processo morre antes de qualquer teste
/// rodar.
int main(int argc, char* argv[]) {
    const QCoreApplication application(argc, argv);

    return Catch::Session().run(argc, argv);
}
