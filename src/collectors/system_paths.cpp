#include "collectors/system_paths.hpp"

#include <array>

#include <windows.h>

namespace zelo::collectors {

namespace {

std::string to_utf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }

    const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }

    std::string result(static_cast<std::size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(),
                          size, nullptr, nullptr);
    return result;
}

std::string environment_path(const wchar_t* name) {
    std::array<wchar_t, 32767> buffer{};

    const DWORD written = ::GetEnvironmentVariableW(name, buffer.data(),
                                                    static_cast<DWORD>(buffer.size()));
    if (written == 0 || written >= buffer.size()) {
        return {};
    }
    return to_utf8(std::wstring(buffer.data(), written));
}

std::vector<std::string> collect_drive_roots() {
    std::array<wchar_t, 512> buffer{};

    const DWORD written = ::GetLogicalDriveStringsW(static_cast<DWORD>(buffer.size()),
                                                    buffer.data());
    if (written == 0 || written >= buffer.size()) {
        return {};
    }

    // A API devolve strings coladas, cada uma terminada em nulo, e a lista
    // inteira terminada por um nulo extra.
    std::vector<std::string> roots;
    for (const wchar_t* entry = buffer.data(); *entry != L'\0'; entry += std::wcslen(entry) + 1) {
        roots.push_back(to_utf8(entry));
    }
    return roots;
}

void append_if_present(std::vector<std::string>& target, const std::string& value) {
    if (!value.empty()) {
        target.push_back(value);
    }
}

}

SystemPaths collect_system_paths() {
    SystemPaths paths;

    std::array<wchar_t, MAX_PATH + 1> windows_directory{};
    const UINT written = ::GetWindowsDirectoryW(windows_directory.data(),
                                                static_cast<UINT>(windows_directory.size()));
    if (written > 0 && written < windows_directory.size()) {
        paths.windows_directory = to_utf8(std::wstring(windows_directory.data(), written));
    }

    paths.program_files = environment_path(L"ProgramFiles");
    paths.program_files_x86 = environment_path(L"ProgramFiles(x86)");
    paths.program_data = environment_path(L"ProgramData");
    paths.user_profile = environment_path(L"USERPROFILE");
    paths.drive_roots = collect_drive_roots();

    return paths;
}

core::ProtectedPaths build_protected_paths(const SystemPaths& paths) {
    core::ProtectedPathsSpec spec;

    append_if_present(spec.subtree_roots, paths.windows_directory);
    append_if_present(spec.subtree_roots, paths.program_files);
    append_if_present(spec.subtree_roots, paths.program_files_x86);
    append_if_present(spec.subtree_roots, paths.program_data);

    append_if_present(spec.exact_paths, paths.user_profile);
    for (const auto& root : paths.drive_roots) {
        append_if_present(spec.exact_paths, root);
    }

    // Carve-out aprovado: o proprio Disk Cleanup limpa esta pasta. So entra na
    // lista se o diretorio do Windows foi lido, senao a excecao ficaria orfa e
    // o construtor a recusaria.
    if (!paths.windows_directory.empty()) {
        spec.exceptions.push_back(paths.windows_directory + "\\Temp");
    }

    return core::ProtectedPaths{std::move(spec)};
}

}
