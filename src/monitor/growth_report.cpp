#include "monitor/growth_report.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>

namespace cleaner::monitor {

namespace {

std::string comparable(std::string path) {
    for (char& character : path) {
        if (character == '/') {
            character = '\\';
            continue;
        }
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return path;
}

std::size_t depth_of(const std::string& path) {
    return static_cast<std::size_t>(std::count(path.begin(), path.end(), '\\'));
}

}

GrowthReport build_growth_report(const SnapshotDiff& diff) {
    GrowthReport report;
    report.from_taken_at = diff.from_taken_at;
    report.to_taken_at = diff.to_taken_at;
    report.free_space_delta = diff.free_space_delta;

    // Comeca com o crescimento da subarvore e vai descontando o que as filhas
    // explicam. O que sobra sao os arquivos que cresceram na propria pasta.
    std::map<std::string, GrowthItem> exclusive;
    for (const auto& change : diff.changes) {
        if (change.delta_bytes == 0) {
            continue;
        }

        exclusive.emplace(comparable(change.path), GrowthItem{
                                                       .path = change.path,
                                                       .exclusive_bytes = change.delta_bytes,
                                                       .subtree_bytes = change.delta_bytes,
                                                   });
    }

    // Do mais fundo para o mais raso: quando uma pasta e processada, ela ja
    // recebeu o desconto das proprias filhas e sabe o que passar ao pai.
    std::vector<std::string> ordered;
    ordered.reserve(exclusive.size());
    for (const auto& [key, item] : exclusive) {
        ordered.push_back(key);
    }

    std::sort(ordered.begin(), ordered.end(), [](const std::string& left, const std::string& right) {
        return depth_of(left) > depth_of(right);
    });

    for (const auto& key : ordered) {
        const auto child = exclusive.find(key);
        if (child == exclusive.end()) {
            continue;
        }

        const std::filesystem::path path(child->second.path);
        const auto parent = path.parent_path();

        if (parent.empty() || parent == path) {
            continue;
        }

        const auto found = exclusive.find(comparable(parent.string()));
        if (found == exclusive.end()) {
            continue;
        }

        // O pai deixa de reivindicar o que a filha ja explica.
        found->second.exclusive_bytes -= child->second.subtree_bytes;
    }

    for (auto& [key, item] : exclusive) {
        if (item.exclusive_bytes >= kGrowthNoiseFloor) {
            report.items.push_back(item);
            continue;
        }

        if (item.exclusive_bytes <= -kGrowthNoiseFloor) {
            report.shrunk.push_back(item);
        }
    }

    const auto by_size = [](const GrowthItem& left, const GrowthItem& right) {
        return std::abs(left.exclusive_bytes) > std::abs(right.exclusive_bytes);
    };

    std::sort(report.items.begin(), report.items.end(), by_size);
    std::sort(report.shrunk.begin(), report.shrunk.end(), by_size);

    return report;
}

}
