#include <catch2/catch_test_macros.hpp>
#include <collectors/detail/wmi.hpp>

#include <windows.h>

using cleaner::collectors::detail::WmiRow;
using cleaner::collectors::detail::query_wmi;

namespace {

/// Inicializa COM como STA e desfaz no fim, imitando o que o Qt faz na thread
/// principal do aplicativo.
class SingleThreadedApartment {
public:
    SingleThreadedApartment() : result_(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}

    ~SingleThreadedApartment() {
        if (SUCCEEDED(result_)) {
            ::CoUninitialize();
        }
    }

    SingleThreadedApartment(const SingleThreadedApartment&) = delete;
    SingleThreadedApartment& operator=(const SingleThreadedApartment&) = delete;
    SingleThreadedApartment(SingleThreadedApartment&&) = delete;
    SingleThreadedApartment& operator=(SingleThreadedApartment&&) = delete;

private:
    HRESULT result_;
};

std::size_t count_rows(const wchar_t* wmi_namespace, const wchar_t* query, bool& queried) {
    std::size_t rows = 0;
    queried = query_wmi(wmi_namespace, query, [&rows](const WmiRow&) { ++rows; });
    return rows;
}

}

// O Qt inicializa COM como STA na thread principal. Se a consulta ao WMI
// insistir em MULTITHREADED, o Windows responde RPC_E_CHANGED_MODE — que
// significa "ja inicializado de outro jeito", nao "falhou". Tratar isso como
// erro derrubava as duas coletas WMI dentro do aplicativo, enquanto no teste,
// sem COM previo, tudo passava.
TEST_CASE("a consulta WMI funciona com COM ja inicializado como STA", "[wmi][integration]") {
    const SingleThreadedApartment apartment;

    bool queried = false;
    const std::size_t rows =
        count_rows(LR"(ROOT\CIMV2)", L"SELECT Caption FROM Win32_OperatingSystem", queried);

    INFO("consulta realizada: " << queried << ", linhas: " << rows);

    CHECK(queried);
    CHECK(rows >= 1);
}

TEST_CASE("a consulta WMI funciona sem COM previo", "[wmi][integration]") {
    bool queried = false;
    const std::size_t rows =
        count_rows(LR"(ROOT\CIMV2)", L"SELECT Caption FROM Win32_OperatingSystem", queried);

    CHECK(queried);
    CHECK(rows >= 1);
}

// Namespace inexistente tem que devolver falso, e nao lista vazia: quem chama
// precisa distinguir "nao ha nada" de "nao consegui olhar".
TEST_CASE("namespace inexistente devolve falha, nao vazio", "[wmi][integration]") {
    bool queried = false;
    count_rows(LR"(ROOT\NamespaceQueNaoExiste)", L"SELECT * FROM Qualquer", queried);

    CHECK_FALSE(queried);
}
