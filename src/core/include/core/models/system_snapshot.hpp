#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zelo::core {

/// O que os coletores conseguiram observar em uma analise. As regras leem
/// daqui e nunca tocam no sistema — e por isso que elas rodam em teste sem
/// depender do Windows.
///
/// Todo campo tem um par `*_available`: dado ausente nao e o mesmo que dado
/// zerado. Sem essa distincao a analise concluiria "nenhum arquivo temporario"
/// quando na verdade nao conseguiu olhar.
struct VolumeInfo {
    std::string letter;
    std::uint64_t total_bytes = 0;
    std::uint64_t free_bytes = 0;
    bool is_system = false;

    [[nodiscard]] double free_ratio() const {
        return total_bytes == 0 ? 0.0 : static_cast<double>(free_bytes) / static_cast<double>(total_bytes);
    }
};

struct StartupItemInfo {
    std::string name;
    std::string publisher;
    std::string path;

    /// Antivirus, drivers, componentes de audio e video, ferramentas de
    /// hardware. O app nunca sugere desativar estes.
    bool essential = false;
};

struct TemporaryFilesInfo {
    bool available = false;
    std::uint64_t total_bytes = 0;
    std::size_t file_count = 0;
};

struct SystemSnapshot {
    bool volumes_available = false;
    std::vector<VolumeInfo> volumes;

    bool startup_available = false;
    std::vector<StartupItemInfo> startup_items;

    TemporaryFilesInfo temporary_files;
};

}
