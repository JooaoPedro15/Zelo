#pragma once

#include "core/risk/risk_level.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace zelo::core {

/// O que acontece depois de remover o conteudo de um local.
enum class RegenerationCost {
    /// Volta sozinho, sem custo perceptivel. Cache de miniaturas, shaders.
    Free,

    /// Volta sozinho, mas custa tempo ou banda: o programa baixa de novo.
    NeedsDownload,

    /// Volta so quando o programa refaz o trabalho — recompilar, reindexar.
    NeedsRework,

    /// Nao volta. Perde-se algo que so existia ali.
    Permanent,
};

/// Um local conhecido do disco, com a explicacao de que ele guarda.
///
/// O catalogo existe porque a pergunta do usuario nunca e "quantos bytes tem
/// aqui", e sim "posso apagar isto sem me arrepender". Cada entrada precisa
/// responder o que e, o que se perde e se volta sozinho — sem isso, o aplicativo
/// estaria pedindo uma decisao as cegas.
struct KnownLocation {
    std::string id;
    std::string display_name;
    std::string path;

    /// Programa dono, quando faz sentido nomear. Ajuda o usuario a reconhecer
    /// algo que ele instalou e esqueceu.
    std::string owner;

    /// O que ha ali, em linguagem de quem nao conhece o programa.
    std::string what_it_is;

    /// O que muda na pratica depois de remover. E a informacao que falta na
    /// maioria dos limpadores.
    std::string what_you_lose;

    RegenerationCost regeneration = RegenerationCost::Free;
    RiskLevel risk = RiskLevel::Yellow;

    std::uint64_t size_bytes = 0;

    /// Falso quando o local nao existe nesta maquina.
    bool present = false;
};

struct ReclaimableInfo {
    bool available = false;
    std::vector<KnownLocation> locations;
};

}
