#pragma once

#include <core/usecases/quick_analysis.hpp>

#include <nlohmann/json.hpp>

#include <string>

namespace cleaner::storage {

/// Uma analise guardada em disco, com o que e preciso para reler o achado
/// depois: quando rodou e com qual versao do aplicativo e das regras.
struct StoredSession {
    std::string id;
    std::string started_at;
    std::string app_version;
    core::AnalysisResult result;
};

/// Sobe de 1 quando o formato mudar de um jeito que a leitura precise tratar.
inline constexpr int kSchemaVersion = 1;

[[nodiscard]] nlohmann::json to_json(const StoredSession& session);

/// Le uma sessao guardada. Lanca `std::runtime_error` quando o documento nao e
/// reconhecivel — quem chama decide o que fazer com um arquivo estragado.
[[nodiscard]] StoredSession session_from_json(const nlohmann::json& document);

/// Nomes estaveis para os enums. Numero seria mais curto, mas quebraria todo o
/// historico ao inserir um valor no meio de um enum.
[[nodiscard]] std::string to_string(core::RiskLevel risk);
[[nodiscard]] std::string to_string(core::Severity severity);
[[nodiscard]] std::string to_string(core::ActionCategory category);
[[nodiscard]] std::string to_string(core::HealthCategory category);

}
