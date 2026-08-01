#pragma once

#include <core/models/system_snapshot.hpp>

#include <stop_token>

namespace cleaner::collectors {

/// Reune os coletores e monta o retrato da maquina que as regras vao ler.
///
/// Um coletor que falha nao derruba os outros: o bloco correspondente
/// simplesmente fica marcado como indisponivel, e a analise dira que nao
/// conseguiu olhar aquela area em vez de fingir que esta tudo bem.
[[nodiscard]] core::SystemSnapshot collect_snapshot(std::stop_token token = {});

}
