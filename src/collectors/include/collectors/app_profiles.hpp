#pragma once

#include <core/models/app_profile.hpp>
#include <core/models/system_snapshot.hpp>
#include <core/risk/protected_paths.hpp>

#include <stop_token>
#include <vector>

namespace zelo::collectors {

/// Os perfis conhecidos, com os caminhos resolvidos para esta maquina.
///
/// Escritos olhando a estrutura real de cada programa, nao pelo nome das
/// pastas. Onde nao houve certeza sobre o conteudo, o item nao foi escrito — e
/// o que nao esta escrito vira desconhecido, que e o comportamento correto.
[[nodiscard]] std::vector<core::AppProfile> app_profiles_catalog();

/// Percorre a area de cada programa e classifica o que encontra.
///
/// Enumera o que existe no disco e procura na tabela do perfil. O que nao
/// estiver la sai como desconhecido, com o tamanho medido: o usuario ve que ha
/// espaco ali e ve tambem que o Zelo nao sabe o que e.
class AppProfileCollector {
public:
    explicit AppProfileCollector(core::ProtectedPaths protected_paths);

    bool collect_into(core::SystemSnapshot& snapshot, std::stop_token token = {}) const;

    [[nodiscard]] std::vector<core::ProfileFinding> collect(std::stop_token token = {}) const;

private:
    core::ProtectedPaths protected_paths_;
};

}
