#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace zelo::core {

/// Deny-list dos diretorios criticos do sistema. Fonte unica de verdade:
/// qualquer caminho sob uma raiz protegida e sempre risco vermelho.
///
/// As raizes chegam prontas de fora (a camada Windows resolve %SystemRoot% e
/// afins), para que este modulo continue puro e testavel sem o SO.
class ProtectedPaths {
public:
    /// `exceptions` sao carve-outs: subpastas dentro de uma raiz protegida que
    /// voltam a ser tratadas como normais (ex.: `C:\Windows\Temp`, que o proprio
    /// Disk Cleanup limpa). Cada excecao precisa estar *estritamente* dentro de
    /// alguma raiz — caso contrario lanca `std::invalid_argument`, porque uma
    /// excecao larga demais desligaria a deny-list inteira.
    explicit ProtectedPaths(std::vector<std::string> roots,
                            std::vector<std::string> exceptions = {});

    /// Compara texto, sem tocar no disco. Caixa e separadores sao
    /// normalizados; a comparacao respeita limite de componente, entao
    /// "C:\Program Files Backup" nao casa com a raiz "C:\Program Files".
    ///
    /// Responde `true` por precaucao quando nao da para decidir com seguranca:
    /// caminho vazio ou com componente relativo (`.` / `..`), que poderia
    /// apontar para dentro de uma raiz protegida sem aparecer no prefixo.
    /// Passe caminhos absolutos ja canonizados.
    [[nodiscard]] bool is_protected(std::string_view path) const;

private:
    std::vector<std::string> roots_;
    std::vector<std::string> exceptions_;
};

}
