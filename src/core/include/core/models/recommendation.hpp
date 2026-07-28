#pragma once

#include "core/confidence/confidence.hpp"
#include "core/models/health_category.hpp"
#include "core/risk/risk_classifier.hpp"
#include "core/risk/risk_level.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace zelo::core {

/// Um sinal observado que sustenta a recomendacao. `source` diz de onde o dado
/// veio, para que o usuario possa conferir por conta propria.
struct Evidence {
    std::string source;
    std::string description;
    std::string value;
};

/// O quanto o achado importa. Nao se confunde com risco (que e da acao) nem
/// com confianca (que e da conclusao).
enum class Severity {
    Info,
    Attention,
    Serious,
};

/// Uma recomendacao explicavel, conforme a secao 32 do planejamento. Todo campo
/// existe para responder uma pergunta do usuario: o que foi achado, como, por
/// que importa, qual o risco, o que da para fazer em vez disso.
struct Recommendation {
    std::string id;

    /// Qual regra produziu isto, e em que versao. Permite reavaliar achados
    /// antigos quando uma regra muda de criterio.
    std::string rule_id;
    int rule_version = 0;

    std::string title;
    std::string description;

    ActionCategory category = ActionCategory::ReadOnlyAnalysis;

    /// Em que area da saude este achado pesa. Cada regra declara a sua: deduzir
    /// isso do tipo da acao nao funciona, porque ler eventos e ler disco sao
    /// ambas analise somente leitura e falam de areas diferentes.
    HealthCategory health_category = HealthCategory::Storage;

    Severity severity = Severity::Info;
    RiskLevel risk = RiskLevel::Red;
    Confidence confidence;

    std::vector<Evidence> evidence;

    std::uint64_t size_bytes = 0;
    std::uint64_t reclaimable_bytes = 0;
    std::vector<std::string> affected_paths;

    std::string recommended_action;
    std::string alternative_action;

    /// Ferramenta oficial envolvida e, quando houver, o identificador do comando
    /// na allowlist. A interface nunca monta comando: ela referencia o id.
    std::string tool;
    std::optional<std::string> command_id;

    bool requires_admin = false;
    bool requires_internet = false;
    bool requires_reboot = false;
    bool cancellable = true;
    bool undoable = false;

    std::string expected_result;

    /// O que esta recomendacao nao consegue garantir. Campo obrigatorio por
    /// principio: o app precisa ser honesto sobre os proprios limites.
    std::string limitations;
};

/// Devolve a lista de problemas que impedem a recomendacao de ser exibida.
/// Vazia significa valida.
[[nodiscard]] std::vector<std::string> validate(const Recommendation& recommendation);

/// Recomendacao vermelha e apenas explicada. O aplicativo nunca a executa por
/// conta propria, mesmo com autorizacao generica do usuario.
[[nodiscard]] bool app_may_execute(const Recommendation& recommendation);

}
