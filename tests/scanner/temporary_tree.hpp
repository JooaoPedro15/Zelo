#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace zelo::testing {

/// Cria uma arvore de arquivos descartavel sob o diretorio temporario do
/// sistema e apaga tudo no destrutor. Os testes do scanner precisam de arquivos
/// de verdade — junctions, permissoes e caminhos longos nao se simulam com mock.
class TemporaryTree {
public:
    TemporaryTree();
    ~TemporaryTree();

    TemporaryTree(const TemporaryTree&) = delete;
    TemporaryTree& operator=(const TemporaryTree&) = delete;
    TemporaryTree(TemporaryTree&&) = delete;
    TemporaryTree& operator=(TemporaryTree&&) = delete;

    [[nodiscard]] const std::filesystem::path& root() const;

    /// Cria o arquivo com exatamente `bytes` de conteudo, criando os diretorios
    /// intermediarios se precisar.
    std::filesystem::path add_file(const std::string& relative, std::uint64_t bytes);

    std::filesystem::path add_directory(const std::string& relative);

    /// Cria uma junction apontando para `target`. Devolve false quando o sistema
    /// recusa (o teste entao se declara ignorado em vez de falhar).
    bool add_junction(const std::string& relative, const std::filesystem::path& target);

    /// Cria um segundo nome para o mesmo conteudo no disco. Hard link nao ocupa
    /// espaco novo: contar os dois inflaria o total.
    bool add_hard_link(const std::string& relative, const std::filesystem::path& target);

    /// Cria um arquivo esparso: o tamanho declarado e grande, mas quase nada
    /// esta gravado. E o caso em que tamanho logico e espaco ocupado divergem
    /// mais.
    bool add_sparse_file(const std::string& relative, std::uint64_t declared_bytes);

    /// Marca o arquivo como compactado pelo NTFS, para exercitar a diferenca
    /// entre o tamanho que o arquivo diz ter e o que ele ocupa.
    bool compress(const std::filesystem::path& path);

private:
    std::filesystem::path root_;
};

}
