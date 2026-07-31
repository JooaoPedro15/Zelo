#include "core/rules/reclaimable_space_rule.hpp"

#include "core/rules/format.hpp"
#include "core/rules/low_free_space_rule.hpp"

#include <algorithm>

namespace zelo::core {

namespace {

std::string regeneration_text(RegenerationCost cost) {
    switch (cost) {
    case RegenerationCost::Free:
        return "Volta sozinho, sem custo perceptivel.";
    case RegenerationCost::NeedsDownload:
        return "Volta sozinho, mas sera baixado de novo quando for preciso — gasta tempo e "
               "internet.";
    case RegenerationCost::NeedsRework:
        return "Volta sozinho, mas o programa precisa refazer o trabalho na proxima vez, o que "
               "deixa o primeiro uso mais lento.";
    case RegenerationCost::Permanent:
        return "Nao volta. O que estiver ali sera perdido.";
    }
    return {};
}

bool system_volume_is_tight(const SystemSnapshot& snapshot) {
    if (!snapshot.volumes_available) {
        return false;
    }
    for (const auto& volume : snapshot.volumes) {
        if (volume.is_system && volume.total_bytes > 0) {
            return volume.free_ratio() < LowFreeSpaceRule::kLowFreeRatio;
        }
    }
    return false;
}

}

std::string ReclaimableSpaceRule::id() const {
    return "storage.reclaimable-location";
}

int ReclaimableSpaceRule::version() const {
    return 1;
}

std::vector<Recommendation> ReclaimableSpaceRule::evaluate(const SystemSnapshot& snapshot) const {
    if (!snapshot.reclaimable.available) {
        return {};
    }

    const std::uint64_t minimum =
        system_volume_is_tight(snapshot) ? kMinimumBytesUnderPressure : kMinimumBytes;

    std::vector<KnownLocation> relevant;
    for (const auto& location : snapshot.reclaimable.locations) {
        if (location.present && location.size_bytes >= minimum) {
            relevant.push_back(location);
        }
    }

    std::sort(relevant.begin(), relevant.end(),
              [](const KnownLocation& left, const KnownLocation& right) {
                  return left.size_bytes > right.size_bytes;
              });

    std::vector<Recommendation> recommendations;
    recommendations.reserve(relevant.size());

    for (const auto& location : relevant) {
        const std::string size_text = format_bytes(location.size_bytes);

        recommendations.push_back(Recommendation{
            .id = id() + ":" + location.id,
            .rule_id = id(),
            .rule_version = version(),
            .title = location.display_name + " — " + size_text,
            .description = location.what_it_is,
            .category = ActionCategory::KnownCache,
            .health_category = HealthCategory::Storage,
            .severity = Severity::Info,
            .counts_against_health = false,
            .risk = location.risk,
            .confidence =
                Confidence::from_signals({{"local conhecido, medido diretamente no disco", 0.9}}),
            .evidence = {Evidence{.source = location.owner.empty() ? location.path : location.owner,
                                  .description = "espaco ocupado por " + location.display_name,
                                  .value = size_text + " em " + location.path}},
            .reclaimable_bytes = location.size_bytes,
            .affected_paths = {location.path},
            .recommended_action = "Remover libera " + size_text + ". " +
                                  regeneration_text(location.regeneration),
            .alternative_action =
                "Manter. Nada esta errado com estes arquivos — eles existem para acelerar o "
                "programa que os criou.",
            .expected_result = size_text + " livres no disco.",
            .limitations = "O que voce perde: " + location.what_you_lose,
        });
    }

    return recommendations;
}

}
