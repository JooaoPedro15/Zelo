#include "scanner/volume_geometry.hpp"

#include <windows.h>

namespace cleaner::scanner {

namespace {

constexpr std::uint32_t kDefaultClusterSize = 4096;

}

std::uint32_t cluster_size_for(const std::filesystem::path& path) {
    std::error_code error;
    const auto absolute = std::filesystem::absolute(path, error);
    if (error) {
        return kDefaultClusterSize;
    }

    const auto root = absolute.root_path();

    DWORD sectors_per_cluster = 0;
    DWORD bytes_per_sector = 0;
    DWORD free_clusters = 0;
    DWORD total_clusters = 0;

    if (::GetDiskFreeSpaceW(root.c_str(), &sectors_per_cluster, &bytes_per_sector, &free_clusters,
                            &total_clusters) == 0) {
        return kDefaultClusterSize;
    }

    const std::uint64_t cluster =
        static_cast<std::uint64_t>(sectors_per_cluster) * bytes_per_sector;
    if (cluster == 0 || cluster > 1024ULL * 1024ULL) {
        return kDefaultClusterSize;
    }

    return static_cast<std::uint32_t>(cluster);
}

std::uint64_t rounded_to_cluster(std::uint64_t bytes, std::uint32_t cluster_size) {
    if (cluster_size == 0) {
        return bytes;
    }

    const std::uint64_t clusters = (bytes + cluster_size - 1) / cluster_size;
    return clusters * cluster_size;
}

}
