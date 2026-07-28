#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace zelo::collectors::detail {

/// Uma linha de resultado do WMI, com os campos ja convertidos para texto.
class WmiRow {
public:
    virtual ~WmiRow() = default;

    [[nodiscard]] virtual std::optional<std::string> text(const wchar_t* property) const = 0;
    [[nodiscard]] virtual std::optional<std::int64_t> number(const wchar_t* property) const = 0;
    [[nodiscard]] virtual std::optional<bool> boolean(const wchar_t* property) const = 0;
};

/// Executa uma consulta WQL e chama `handle` para cada linha.
///
/// Devolve falso quando a consulta nao pode sequer ser feita — namespace
/// ausente, servico WMI parado, falta de permissao. Quem chama precisa
/// distinguir isso de "consultei e nao havia nada", porque tratar os dois como
/// iguais faria a analise afirmar que esta tudo bem sem ter olhado.
///
/// O WMI e notoriamente lento e pode travar. A implementacao aplica um limite
/// de tempo e desiste em vez de deixar a analise pendurada.
bool query_wmi(const wchar_t* wmi_namespace, const wchar_t* query,
               const std::function<void(const WmiRow&)>& handle);

}
