#pragma once

#include <string>
#include <vector>

namespace cleaner::collectors {

/// Um programa aberto agora.
struct RunningApp {
    std::string executable;
    std::size_t instances = 0;
};

/// Os programas em execucao no momento.
///
/// Somente leitura: lista quem esta aberto e nunca encerra nada. Fechar
/// programa por conta propria custaria ao usuario o trabalho nao salvo.
[[nodiscard]] std::vector<RunningApp> running_apps();

/// O programa esta aberto?
///
/// Serve para avisar antes de limpar: arquivo em uso nao e removido, e o
/// usuario precisa saber disso antes de aprovar, para nao estranhar o espaco
/// liberado a menos.
[[nodiscard]] bool is_running(const std::string& executable);

}
