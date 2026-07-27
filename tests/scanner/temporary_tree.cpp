#include "temporary_tree.hpp"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <string>
#include <vector>

#include <windows.h>

namespace zelo::testing {

namespace {

/// O fixture cria arvores mais fundas que MAX_PATH de proposito, e as funcoes
/// de <filesystem> falham nesse terreno. Por isso a criacao e a remocao usam
/// Win32 direto, sempre com o prefixo de caminho longo.
std::wstring prefixed(const std::filesystem::path& path) {
    std::wstring native = path.native();
    std::replace(native.begin(), native.end(), L'/', L'\\');

    return native.starts_with(LR"(\\?\)") ? native : LR"(\\?\)" + native;
}

void create_directory_chain(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> levels;
    for (std::filesystem::path current = directory; !current.empty() && current.has_relative_path();
         current = current.parent_path()) {
        levels.push_back(current);
    }

    for (auto level = levels.rbegin(); level != levels.rend(); ++level) {
        ::CreateDirectoryW(prefixed(*level).c_str(), nullptr);
    }
}

void remove_tree(const std::filesystem::path& directory) {
    const std::wstring pattern = prefixed(directory) + L"\\*";

    WIN32_FIND_DATAW entry{};
    const HANDLE search = ::FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &entry,
                                             FindExSearchNameMatch, nullptr, 0);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            const std::wstring name = entry.cFileName;
            if (name == L"." || name == L"..") {
                continue;
            }

            const std::filesystem::path child = directory / name;
            const bool is_directory = (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            const bool is_reparse = (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;

            if (is_directory && !is_reparse) {
                remove_tree(child);
            } else if (is_directory) {
                // Junction: remove o link, nunca o conteudo do destino.
                ::RemoveDirectoryW(prefixed(child).c_str());
            } else {
                ::DeleteFileW(prefixed(child).c_str());
            }
        } while (::FindNextFileW(search, &entry) != 0);
        ::FindClose(search);
    }

    ::RemoveDirectoryW(prefixed(directory).c_str());
}

std::filesystem::path unique_root() {
    static std::atomic<int> counter{0};

    const auto id = std::to_string(counter.fetch_add(1));
    return std::filesystem::temp_directory_path() / ("zelo-test-" + id);
}

}

TemporaryTree::TemporaryTree() : root_(unique_root()) {
    remove_tree(root_);
    create_directory_chain(root_);
}

TemporaryTree::~TemporaryTree() {
    remove_tree(root_);
}

const std::filesystem::path& TemporaryTree::root() const {
    return root_;
}

std::filesystem::path TemporaryTree::add_file(const std::string& relative, std::uint64_t bytes) {
    const std::filesystem::path path = root_ / relative;
    create_directory_chain(path.parent_path());

    const HANDLE file = ::CreateFileW(prefixed(path).c_str(), GENERIC_WRITE, 0, nullptr,
                                      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return path;
    }

    const std::vector<char> block(1024, 'z');
    std::uint64_t written = 0;
    while (written < bytes) {
        const auto chunk = static_cast<DWORD>(std::min<std::uint64_t>(block.size(), bytes - written));
        DWORD produced = 0;
        if (::WriteFile(file, block.data(), chunk, &produced, nullptr) == 0) {
            break;
        }
        written += produced;
    }

    ::CloseHandle(file);
    return path;
}

std::filesystem::path TemporaryTree::add_directory(const std::string& relative) {
    const std::filesystem::path path = root_ / relative;
    create_directory_chain(path);
    return path;
}

bool TemporaryTree::add_junction(const std::string& relative, const std::filesystem::path& target) {
    const std::filesystem::path path = root_ / relative;
    create_directory_chain(path.parent_path());

    // Junction em vez de symlink: nao exige privilegio nem modo desenvolvedor.
    const std::string command =
        "cmd /c mklink /J \"" + path.string() + "\" \"" + target.string() + "\" >nul 2>&1";
    return std::system(command.c_str()) == 0;
}

}
