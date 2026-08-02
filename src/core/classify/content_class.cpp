#include "core/classify/content_class.hpp"

namespace cleaner::core {

std::string to_string(ContentClass value) {
    switch (value) {
    case ContentClass::SafeToClean:
        return "Seguro para limpar";
    case ContentClass::CleanWithConsequence:
        return "Pode limpar, com consequencia";
    case ContentClass::NeedsReview:
        return "Revisar manualmente";
    case ContentClass::Protected:
        return "Protegido";
    }
    return "Revisar manualmente";
}

std::string to_string(DataKind value) {
    switch (value) {
    case DataKind::Cache:
        return "cache";
    case DataKind::Log:
        return "registro de eventos";
    case DataKind::TemporaryFile:
        return "arquivo temporario";
    case DataKind::Configuration:
        return "configuracao";
    case DataKind::Credential:
        return "credencial";
    case DataKind::Session:
        return "sessao";
    case DataKind::UserContent:
        return "conteudo seu";
    case DataKind::Package:
        return "pacote baixado";
    case DataKind::Environment:
        return "ambiente de desenvolvimento";
    case DataKind::Extension:
        return "extensao";
    case DataKind::Database:
        return "banco de dados";
    case DataKind::Installer:
        return "instalador";
    case DataKind::SystemComponent:
        return "componente do sistema";
    case DataKind::Unknown:
        break;
    }
    return "nao identificado";
}

Classification unclassified() {
    return Classification{
        .content_class = ContentClass::NeedsReview,
        .kind = DataKind::Unknown,
        .what_it_is = "Conteudo nao classificado — revisao necessaria.",
        .reason = "Nenhuma regra do Cleaner reconhece este caminho.",
        // Sem consequencia declarada porque nao ha remocao a oferecer. Prometer
        // que "nada se perde" sobre algo que nao foi identificado seria
        // exatamente o palpite que este nivel existe para evitar.
        .confidence = 0.0,
    };
}

}
