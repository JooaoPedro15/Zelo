#include "monitor/growth_alerts.hpp"

#include <core/rules/format.hpp>

#include <algorithm>

namespace cleaner::monitor {

namespace {

std::string bytes(std::int64_t value) {
    return core::format_bytes(static_cast<std::uint64_t>(std::abs(value)));
}

}

std::vector<Alert> evaluate_alerts(const Snapshot& current, const GrowthReport& growth,
                                   const AlertThresholds& thresholds) {
    std::vector<Alert> alerts;

    // Retrato incompleto nao sustenta alerta nenhum: o que nao foi visitado
    // apareceria como espaco que sumiu.
    if (!current.complete) {
        return alerts;
    }

    const double free_ratio =
        current.total_bytes == 0
            ? 1.0
            : static_cast<double>(current.free_bytes) / static_cast<double>(current.total_bytes);

    if (current.free_bytes < thresholds.minimum_free_bytes ||
        free_ratio < thresholds.minimum_free_ratio) {
        alerts.push_back(Alert{
            .kind = AlertKind::LowFreeSpace,
            .title = "Pouco espaco livre no disco " + current.volume,
            .evidence = core::format_bytes(current.free_bytes) + " livres de " +
                        core::format_bytes(current.total_bytes),
            .since = current.taken_at,
            .suggested_action =
                "Veja a aba do que cresceu para descobrir o que ocupou o espaco, e a lista de "
                "achados para o que pode ser liberado com seguranca.",
        });
    }

    // Crescimento rapido so vira alerta quando ha dois retratos para comparar.
    // Sem o anterior nao existe "rapido" — existe apenas um numero.
    if (growth.free_space_delta < 0 &&
        static_cast<std::uint64_t>(-growth.free_space_delta) >= thresholds.fast_growth_bytes) {
        std::vector<std::string> folders;
        for (std::size_t index = 0; index < std::min<std::size_t>(5, growth.items.size());
             ++index) {
            folders.push_back(growth.items[index].path);
        }

        alerts.push_back(Alert{
            .kind = AlertKind::FastGrowth,
            .title = "O disco perdeu " + bytes(growth.free_space_delta) + " no periodo",
            .evidence = "Comparando o retrato de " + growth.from_taken_at + " com o de " +
                        growth.to_taken_at,
            .since = growth.from_taken_at,
            .folders = std::move(folders),
            .suggested_action = "As pastas listadas explicam a maior parte. Veja o que cada uma "
                                "guarda antes de decidir.",
        });
    }

    for (const auto& item : growth.items) {
        if (item.exclusive_bytes < static_cast<std::int64_t>(thresholds.folder_jump_bytes)) {
            // A lista vem ordenada: a primeira abaixo do limite encerra.
            break;
        }

        alerts.push_back(Alert{
            .kind = AlertKind::FolderJump,
            .title = "Uma pasta cresceu " + bytes(item.exclusive_bytes),
            .evidence = item.path + " cresceu " + bytes(item.exclusive_bytes) + " desde " +
                        growth.from_taken_at,
            .since = growth.from_taken_at,
            .folders = {item.path},
            .suggested_action = "Confira o que ha nessa pasta na aba de analise antes de remover "
                                "qualquer coisa.",
        });
    }

    return alerts;
}

}
