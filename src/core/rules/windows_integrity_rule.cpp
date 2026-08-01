#include "core/rules/windows_integrity_rule.hpp"

namespace cleaner::core {

std::string WindowsIntegrityRule::id() const {
    return "integrity.corruption-signs";
}

int WindowsIntegrityRule::version() const {
    return 1;
}

std::vector<Recommendation> WindowsIntegrityRule::evaluate(const SystemSnapshot& snapshot) const {
    const auto& integrity = snapshot.integrity;
    if (!integrity.available) {
        return {};
    }

    const std::size_t total = integrity.corruption_events + integrity.component_events;
    if (total < kMinimumEvents) {
        return {};
    }

    std::vector<Evidence> evidence;
    if (integrity.corruption_events > 0) {
        evidence.push_back(
            Evidence{.source = "canal Setup, servico de componentes do Windows",
                     .description = "eventos indicando componente danificado",
                     .value = std::to_string(integrity.corruption_events) + " ocorrencias"});
    }
    if (integrity.component_events > 0) {
        evidence.push_back(
            Evidence{.source = "canal Application, componentes lado a lado",
                     .description = "eventos de assembly ou manifesto invalido",
                     .value = std::to_string(integrity.component_events) + " ocorrencias"});
    }
    if (!integrity.last_seen.empty()) {
        evidence.push_back(Evidence{.source = "Visualizador de Eventos",
                                    .description = "ocorrencia mais recente",
                                    .value = integrity.last_seen});
    }

    return {Recommendation{
        .id = id(),
        .rule_id = id(),
        .rule_version = version(),
        .title = "Ha sinais de componentes do Windows danificados",
        .description =
            "O Windows registrou " + std::to_string(total) + " eventos nos ultimos " +
            std::to_string(integrity.window_days) +
            " dias envolvendo seus proprios componentes. Isso costuma aparecer depois de "
            "atualizacoes interrompidas ou desligamentos bruscos, e pode explicar erros que nao "
            "tem causa aparente.",
        .category = ActionCategory::ReadOnlyAnalysis,
        .health_category = HealthCategory::WindowsIntegrity,
        .severity = Severity::Attention,
        .risk = RiskLevel::Green,
        // Confianca moderada: os eventos indicam que algo foi registrado, nao
        // que a instalacao esta corrompida agora. So a verificacao oficial diz.
        .confidence = Confidence::from_signals(
            {{"eventos registrados pelo proprio Windows", 0.45},
             {"mais de uma ocorrencia no periodo", 0.2}}),
        .evidence = std::move(evidence),
        .recommended_action =
            "O Windows tem duas ferramentas oficiais para isto. O verificador de arquivos do "
            "sistema (sfc /scannow) confere os arquivos e repara o que encontrar. O DISM "
            "(DISM /Online /Cleanup-Image /ScanHealth) verifica a imagem do Windows sem alterar "
            "nada. Ambas exigem administrador e podem demorar.",
        .alternative_action =
            "Se o computador vem funcionando bem, acompanhar tambem e uma opcao legitima: estes "
            "eventos nem sempre significam problema em uso.",
        .tool = "Verificador de arquivos do sistema e DISM",
        .requires_admin = true,
        .expected_result =
            "A verificacao dira se ha de fato arquivos danificados, e o verificador repara os que "
            "conseguir a partir da copia local do Windows.",
        .limitations =
            "Esta versao do Cleaner nao executa essas ferramentas — ela reune a evidencia e explica o "
            "que cada uma faz. Os eventos mostram que algo foi registrado no passado, nao que a "
            "instalacao esteja danificada agora. Verificar tambem nao conserta tudo: parte dos "
            "problemas so se resolve com reparo avancado ou reinstalacao do Windows.",
    }};
}

}
