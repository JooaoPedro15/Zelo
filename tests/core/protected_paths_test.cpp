#include <catch2/catch_test_macros.hpp>
#include <core/risk/protected_paths.hpp>

#include <stdexcept>

using zelo::core::ProtectedPaths;

TEST_CASE("caminho identico a uma raiz protegida e protegido", "[protected_paths]") {
    const ProtectedPaths paths{{"C:\\Windows"}};

    CHECK(paths.is_protected("C:\\Windows"));
}

TEST_CASE("caminho dentro de uma raiz protegida e protegido", "[protected_paths]") {
    const ProtectedPaths paths{{"C:\\Windows"}};

    CHECK(paths.is_protected("C:\\Windows\\System32\\drivers\\etc\\hosts"));
}

// O sistema de arquivos do Windows ignora caixa: perder a protecao por causa
// disso deixaria o System32 exposto.
TEST_CASE("comparacao de caminho ignora diferenca de caixa", "[protected_paths]") {
    const ProtectedPaths paths{{"C:\\Windows"}};

    CHECK(paths.is_protected("c:\\windows\\system32"));
    CHECK(paths.is_protected("C:\\WINDOWS\\SYSTEM32"));
}

// O Windows aceita os dois separadores; tratar so a contrabarra deixaria
// passar um caminho protegido escrito com barra normal.
TEST_CASE("barra normal e contrabarra sao equivalentes", "[protected_paths]") {
    const ProtectedPaths backslash_root{{"C:\\Windows"}};
    CHECK(backslash_root.is_protected("C:/Windows/System32"));

    const ProtectedPaths forward_slash_root{{"C:/Windows"}};
    CHECK(forward_slash_root.is_protected("C:\\Windows\\System32"));
}

// Comparar so o prefixo textual marcaria pastas do proprio usuario como
// intocaveis, sem que ela esteja dentro da raiz protegida.
TEST_CASE("pasta irma com o mesmo prefixo nao e protegida", "[protected_paths]") {
    const ProtectedPaths paths{{"C:\\Program Files"}};

    CHECK_FALSE(paths.is_protected("C:\\Program Files Backup"));
    CHECK_FALSE(paths.is_protected("C:\\Program Files Backup\\projeto.zip"));
}

TEST_CASE("barra no fim da raiz nao muda o resultado", "[protected_paths]") {
    const ProtectedPaths paths{{"C:\\Windows\\"}};

    CHECK(paths.is_protected("C:\\Windows"));
    CHECK(paths.is_protected("C:\\Windows\\System32"));
}

TEST_CASE("raiz de unidade protege todo o disco", "[protected_paths]") {
    const ProtectedPaths paths{{"C:\\"}};

    CHECK(paths.is_protected("C:\\"));
    CHECK(paths.is_protected("C:\\Users\\Joao\\video.mp4"));
    CHECK_FALSE(paths.is_protected("D:\\Projetos"));
}

// Esta classe compara texto, nao consulta o disco: "..\" pode apontar para
// dentro de uma raiz protegida sem que o prefixo mostre isso. Na duvida,
// proteger — deny-list erra para o lado seguro.
TEST_CASE("caminho com componente relativo e tratado como protegido", "[protected_paths]") {
    const ProtectedPaths paths{{"C:\\Windows"}};

    CHECK(paths.is_protected("C:\\Users\\Joao\\..\\..\\Windows\\System32"));
    CHECK(paths.is_protected("D:\\Projetos\\..\\qualquer"));
    CHECK(paths.is_protected("D:\\Projetos\\.\\cache"));
}

TEST_CASE("caminho vazio e tratado como protegido", "[protected_paths]") {
    const ProtectedPaths paths{{"C:\\Windows"}};

    CHECK(paths.is_protected(""));
}

// Carve-out: o Windows guarda temporarios dentro de uma raiz protegida, e o
// proprio Disk Cleanup limpa essa pasta. A excecao abre so ela, e o resto da
// raiz continua fechado.
TEST_CASE("excecao libera uma subpasta sem abrir a raiz protegida", "[protected_paths]") {
    const ProtectedPaths paths{{"C:\\Windows"}, {"C:\\Windows\\Temp"}};

    CHECK_FALSE(paths.is_protected("C:\\Windows\\Temp"));
    CHECK_FALSE(paths.is_protected("C:\\Windows\\Temp\\instalador.tmp"));

    CHECK(paths.is_protected("C:\\Windows"));
    CHECK(paths.is_protected("C:\\Windows\\System32"));
}

// A excecao usa a mesma regra de limite das raizes: uma pasta que apenas
// comeca com o mesmo texto nao entra no carve-out.
TEST_CASE("pasta com prefixo parecido nao entra na excecao", "[protected_paths]") {
    const ProtectedPaths paths{{"C:\\Windows"}, {"C:\\Windows\\Temp"}};

    CHECK(paths.is_protected("C:\\Windows\\Temporario"));
    CHECK(paths.is_protected("C:\\Windows\\TempAntigo\\arquivo.dat"));
}

TEST_CASE("nao da para sair da excecao com componente relativo", "[protected_paths]") {
    const ProtectedPaths paths{{"C:\\Windows"}, {"C:\\Windows\\Temp"}};

    CHECK(paths.is_protected("C:\\Windows\\Temp\\..\\System32"));
}

// Uma excecao igual a raiz — ou fora dela — desligaria a protecao inteira.
// A lista e fixa no binario, entao falhar aqui trava o build, nunca o usuario.
TEST_CASE("excecao que nao esta dentro de uma raiz e recusada", "[protected_paths]") {
    CHECK_THROWS_AS(ProtectedPaths({"C:\\Windows"}, {"C:\\Windows"}), std::invalid_argument);
    CHECK_THROWS_AS(ProtectedPaths({"C:\\Windows"}, {"C:\\"}), std::invalid_argument);
    CHECK_THROWS_AS(ProtectedPaths({"C:\\Windows"}, {"D:\\Projetos"}), std::invalid_argument);
}
