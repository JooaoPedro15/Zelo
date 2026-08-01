#pragma once

#include "core/models/known_location.hpp"
#include "core/risk/risk_level.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace zelo::core {

/// Uma pasta ou arquivo dentro da area de um programa, ja classificado.
///
/// O catalogo plano trata a pasta de um programa como um bloco so. Isso e
/// grosseiro demais para as ferramentas que mais ocupam espaco: dentro de
/// `.codex` convivem temporarios descartaveis e sessoes insubstituiveis, e
/// oferecer os dois juntos seria oferecer a perda do historico junto com a
/// limpeza do lixo.
struct ProfileItem {
    /// Caminho relativo a raiz do programa, como o programa o nomeia.
    std::string relative_path;

    std::string display_name;
    std::string what_it_is;
    std::string what_you_lose;

    RiskLevel risk = RiskLevel::Unknown;
    RegenerationCost regeneration = RegenerationCost::Permanent;
};

/// O que foi encontrado de fato no disco para um item do perfil.
struct ProfileFinding {
    std::string path;
    std::string application;

    ProfileItem item;
    std::uint64_t size_bytes = 0;
};

/// A area de um programa e como classificar o que ha dentro dela.
struct AppProfile {
    std::string id;
    std::string application;

    /// Onde o programa guarda seus dados nesta maquina.
    std::string root;

    /// Itens reconhecidos. O que aparecer na raiz e nao estiver aqui e
    /// classificado como desconhecido — nunca como seguro por semelhanca de
    /// nome. Uma pasta chamada "cache" pode guardar qualquer coisa.
    std::vector<ProfileItem> items;
};

}
