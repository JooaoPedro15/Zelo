#pragma once

#include <core/models/space_tree.hpp>

#include <filesystem>
#include <functional>
#include <stop_token>

namespace cleaner::scanner {

struct SurveyOptions {
    /// Ate que profundidade a arvore guarda nos.
    ///
    /// A soma vai ate o fim sempre; o limite e so de apresentacao. Sem ele, o
    /// C: inteiro produziria centenas de milhares de nos que ninguem abriria.
    std::size_t tree_depth = 4;

    /// Pastas menores que isto nao viram no proprio, mas continuam somadas no
    /// pai. Evita uma arvore com milhares de folhas de poucos kilobytes.
    std::uint64_t minimum_node_bytes = 32ULL * 1024 * 1024;
};

/// Chamado enquanto a varredura anda. Recebe o caminho da vez.
///
/// Serve para a interface mostrar que algo esta acontecendo. E chamado da
/// thread que varre, nunca da thread da interface.
using SurveyProgress = std::function<void(const std::string&)>;

/// Percorre uma arvore e devolve onde o espaco esta, com a conta fechada contra
/// o que o Windows informa sobre o volume.
///
/// Somente leitura. Cancelavel pelo token: a varredura confere a cada entrada.
[[nodiscard]] core::SpaceSurvey survey_space(const std::filesystem::path& root,
                                             const std::string& volume,
                                             SurveyOptions options = {},
                                             std::stop_token token = {},
                                             const SurveyProgress& progress = {});

}
