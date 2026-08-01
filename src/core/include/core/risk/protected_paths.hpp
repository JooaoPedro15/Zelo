#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace cleaner::core {

/// Deny-list dos diretorios criticos do sistema. Fonte unica de verdade:
/// qualquer caminho sob uma raiz protegida e sempre risco vermelho.
///
/// As raizes chegam prontas de fora (a camada Windows resolve %SystemRoot% e
/// afins), para que este modulo continue puro e testavel sem o SO.
/// Como a deny-list e montada. Separar os tres papeis evita o erro de proteger
/// uma pasta inteira quando a intencao era proteger apenas ela mesma.
struct ProtectedPathsSpec {
    /// A pasta e tudo abaixo dela. Para `C:\Windows`, `System32` e todo o resto.
    std::vector<std::string> subtree_roots;

    /// Somente a propria pasta. A raiz de unidade e o perfil do usuario entram
    /// aqui: ninguem pode apaga-las, mas o conteudo dentro delas e exatamente o
    /// que o aplicativo precisa analisar.
    std::vector<std::string> exact_paths;

    /// Carve-outs: subpastas dentro de uma raiz que voltam a ser tratadas como
    /// normais, como `C:\Windows\Temp`, que o proprio Disk Cleanup limpa.
    std::vector<std::string> exceptions;
};

class ProtectedPaths {
public:
    /// Cada excecao precisa estar *estritamente* dentro de alguma raiz de
    /// subarvore — caso contrario lanca `std::invalid_argument`, porque uma
    /// excecao larga demais desligaria a deny-list inteira.
    explicit ProtectedPaths(ProtectedPathsSpec spec);

    explicit ProtectedPaths(std::vector<std::string> roots,
                            std::vector<std::string> exceptions = {});

    /// Compara texto, sem tocar no disco. Separadores e caixa sao normalizados
    /// (ASCII e Latin-1, entao "JOÃO" casa com "João"); a comparacao respeita
    /// limite de componente, entao "C:\Program Files Backup" nao casa com a
    /// raiz "C:\Program Files".
    ///
    /// Responde `true` por precaucao quando nao da para decidir com seguranca:
    /// caminho vazio ou com componente relativo (`.` / `..`), que poderia
    /// apontar para dentro de uma raiz protegida sem aparecer no prefixo.
    /// Passe caminhos absolutos ja canonizados.
    [[nodiscard]] bool is_protected(std::string_view path) const;

private:
    std::vector<std::string> subtree_roots_;
    std::vector<std::string> exact_paths_;
    std::vector<std::string> exceptions_;
};

}
