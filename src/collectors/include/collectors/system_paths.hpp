#pragma once

#include <core/risk/protected_paths.hpp>

#include <string>
#include <vector>

namespace cleaner::collectors {

/// Os caminhos que o Windows define para si mesmo. Ficam separados da montagem
/// da deny-list de proposito: assim a parte critica para a seguranca e pura e
/// roda em teste sem depender da maquina.
struct SystemPaths {
    std::string windows_directory;
    std::string program_files;
    std::string program_files_x86;
    std::string program_data;
    std::string user_profile;
    std::vector<std::string> drive_roots;

    /// Pastas sincronizadas com a nuvem. Dentro delas, apagar nao e uma
    /// operacao local: a exclusao viaja para a nuvem e para os outros
    /// dispositivos do usuario.
    std::vector<std::string> cloud_roots;
};

/// Le os caminhos reais da maquina. Um caminho que o sistema nao informar volta
/// vazio, e a montagem simplesmente o ignora.
[[nodiscard]] SystemPaths collect_system_paths();

/// Monta a deny-list do projeto a partir dos caminhos informados.
///
/// Windows, Program Files, ProgramData e as pastas de nuvem protegem a subarvore
/// inteira. A raiz de unidade e o perfil do usuario protegem apenas a propria
/// pasta, senao nada dentro do disco poderia ser analisado. A pasta de
/// temporarios do Windows e o unico carve-out aprovado ate agora.
[[nodiscard]] core::ProtectedPaths build_protected_paths(const SystemPaths& paths);

}
