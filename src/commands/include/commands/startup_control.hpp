#pragma once

#include <core/models/system_snapshot.hpp>

#include <string>

namespace cleaner::commands {

enum class StartupChange {
    /// O item passou a nao iniciar mais com o Windows.
    Disabled,

    /// O item voltou a iniciar com o Windows.
    Enabled,

    /// Ja estava no estado pedido. Nada foi escrito.
    Unchanged,

    /// Precisa de administrador. Itens da maquina inteira valem para todos os
    /// usuarios, e o Windows nao deixa um usuario comum mexer neles.
    Denied,

    /// O Windows recusou a escrita por outro motivo.
    Failed,
};

/// Liga ou desliga um item de inicializacao.
///
/// Usa o mesmo interruptor do Gerenciador de Tarefas: o Windows guarda o estado
/// em `StartupApproved`, sem tocar na entrada original. O programa continua
/// instalado, a chave continua no lugar, e voltar atras e escrever o valor
/// oposto.
///
/// Nada e apagado aqui. Desativar item de inicializacao e a unica acao do
/// Cleaner que se desfaz por completo, e e assim de proposito: mexer no que
/// abre junto com o computador precisa ter volta.
StartupChange set_startup_enabled(const std::string& name, core::StartupOrigin origin,
                                  bool enabled);

/// Le se o item esta ligado. Item sem registro em `StartupApproved` nunca foi
/// desativado, entao esta ligado.
[[nodiscard]] bool startup_is_enabled(const std::string& name, core::StartupOrigin origin);

}
