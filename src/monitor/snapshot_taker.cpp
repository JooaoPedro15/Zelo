#include "monitor/snapshot_taker.hpp"

#include <scanner/storage_scanner.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <map>
#include <utility>

#include <windows.h>

namespace cleaner::monitor {

namespace {

std::string now_iso8601() {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::tm parts{};
    ::localtime_s(&parts, &now);

    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &parts);
    return buffer;
}

/// Comparacao de caminho no padrao do Windows: caixa e separador nao importam.
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

bool is_inside(const std::string& path, const std::string& folder) {
    if (path.size() < folder.size() || path.compare(0, folder.size(), folder) != 0) {
        return false;
    }
    return path.size() == folder.size() || path[folder.size()] == '\\';
}

/// Espaco total e livre do volume, para o retrato dizer de quanto se esta
/// falando. Sem isso, "a pasta cresceu 2 GB" nao se liga a "o disco encheu".
std::pair<std::uint64_t, std::uint64_t> volume_space(const std::filesystem::path& root) {
    ULARGE_INTEGER free_for_caller{};
    ULARGE_INTEGER total{};
    ULARGE_INTEGER total_free{};

    std::error_code error;
    const auto absolute = std::filesystem::absolute(root, error);
    if (error) {
        return {0, 0};
    }

    if (::GetDiskFreeSpaceExW(absolute.root_path().c_str(), &free_for_caller, &total,
                              &total_free) == 0) {
        return {0, 0};
    }

    return {total.QuadPart, free_for_caller.QuadPart};
}

}

std::vector<FolderSize> accumulate_subtrees(std::vector<FolderSize> direct_sizes) {
    // Indexado por caminho comparavel, para achar o pai sem depender de como o
    // Windows devolveu a caixa das letras.
    std::map<std::string, FolderSize*> by_path;
    for (auto& folder : direct_sizes) {
        by_path.emplace(comparable(folder.path), &folder);
    }

    // Do mais fundo para o mais raso: quando uma pasta e processada, tudo
    // abaixo dela ja subiu.
    std::vector<FolderSize*> ordered;
    ordered.reserve(direct_sizes.size());
    for (auto& folder : direct_sizes) {
        ordered.push_back(&folder);
    }

    std::sort(ordered.begin(), ordered.end(),
              [](const FolderSize* left, const FolderSize* right) {
                  return left->depth > right->depth;
              });

    for (auto* folder : ordered) {
        const std::filesystem::path path(folder->path);
        const auto parent = path.parent_path();

        if (parent.empty() || parent == path) {
            continue;
        }

        const auto found = by_path.find(comparable(parent.string()));
        if (found == by_path.end()) {
            continue;
        }

        found->second->logical_bytes += folder->logical_bytes;
        found->second->allocated_bytes += folder->allocated_bytes;
        found->second->file_count += folder->file_count;
    }

    return direct_sizes;
}

SnapshotTaker::SnapshotTaker(SnapshotOptions options) : options_(std::move(options)) {}

Snapshot SnapshotTaker::take(const std::filesystem::path& root, const std::string& volume,
                             std::stop_token token, const ProgressCallback& progress) const {
    Snapshot snapshot;
    snapshot.taken_at = now_iso8601();
    snapshot.volume = volume;
    snapshot.kind = "manual";

    const auto [total, free] = volume_space(root);
    snapshot.total_bytes = total;
    snapshot.free_bytes = free;

    const scanner::StorageScanner storage_scanner{scanner::ScanOptions{
        // O bastante para os maiores arquivos do volume; o filtro por tamanho
        // vem depois.
        .largest_files_kept = 2000,
        .emit_all_directories = true,
    }};

    const auto result = storage_scanner.scan(root, token);

    if (!result.completed) {
        // Sem marcar como completo: comparar um retrato interrompido com um
        // inteiro mostraria como espaco liberado o que nao foi visitado.
        spdlog::warn("retrato interrompido: fica registrado como incompleto");
        return snapshot;
    }

    std::vector<std::string> excluded;
    excluded.reserve(options_.excluded_paths.size());
    for (const auto& path : options_.excluded_paths) {
        excluded.push_back(comparable(path));
    }

    const auto is_excluded = [&excluded](const std::string& path) {
        const auto candidate = comparable(path);
        return std::any_of(excluded.begin(), excluded.end(),
                           [&candidate](const std::string& folder) {
                               return is_inside(candidate, folder);
                           });
    };

    std::vector<FolderSize> direct;
    direct.reserve(result.directories.size());

    for (const auto& folder : result.directories) {
        if (is_excluded(folder.path)) {
            continue;
        }

        direct.push_back(FolderSize{
            .path = folder.path,
            .logical_bytes = folder.total_bytes,
            .allocated_bytes = folder.allocated_bytes,
            .file_count = folder.file_count,
            .depth = static_cast<int>(folder.depth),
        });
    }

    if (progress) {
        progress(direct.size());
    }

    auto totals = accumulate_subtrees(std::move(direct));

    // Guardar toda pasta de um volume estouraria o banco. Os niveis de cima
    // entram sempre, porque sao os que a interface mostra; mais fundo, so o que
    // for grande o bastante para explicar uma mudanca perceptivel.
    for (auto& folder : totals) {
        const bool shallow = folder.depth <= options_.always_keep_depth;
        const bool large = folder.allocated_bytes >= options_.deep_folder_threshold;

        if (shallow || large) {
            snapshot.folders.push_back(std::move(folder));
        }
    }

    for (const auto& file : result.largest_files) {
        if (file.allocated_bytes < options_.tracked_file_threshold || is_excluded(file.path)) {
            continue;
        }

        snapshot.files.push_back(TrackedFile{
            .path = file.path,
            .logical_bytes = file.bytes,
            .allocated_bytes = file.allocated_bytes,
            .modified_at = file.modified_at,
        });
    }

    snapshot.complete = true;
    return snapshot;
}

}
