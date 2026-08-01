#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace zelo::collectors {

/// Uma pasta que um servico de nuvem mantem sincronizada.
struct CloudFolder {
    std::string path;

    /// Qual servico. Hoje so o OneDrive, que vem com o Windows.
    std::string service;

    /// Qual conta dentro do servico ("Personal", "Business1"). Serve para
    /// distinguir a pasta pessoal da corporativa, que costumam conviver.
    std::string account;
};

/// As pastas sincronizadas configuradas neste usuario.
///
/// Sai do registro do proprio servico. Adivinhar pelo nome da pasta erraria nos
/// dois sentidos: quem moveu a pasta ficaria de fora, e uma pasta chamada
/// "OneDrive" sem sincronizacao entraria sem ser.
[[nodiscard]] std::vector<CloudFolder> cloud_folders();

/// Quanto de uma pasta sincronizada esta realmente baixado.
struct CloudSpace {
    std::string path;

    /// Ocupa disco agora. E este o unico numero que pode virar espaco livre.
    std::uint64_t local_bytes = 0;
    std::size_t local_files = 0;

    /// Mora so na nuvem. Nao ocupa disco: apagar isso nao libera nada e ainda
    /// remove o arquivo da nuvem e dos outros dispositivos.
    std::uint64_t online_only_bytes = 0;
    std::size_t online_only_files = 0;

    /// Falso quando a varredura foi interrompida ou nao leu tudo.
    bool complete = false;
};

/// Mede uma pasta sincronizada separando o que esta no disco do que esta so na
/// nuvem. Somente leitura.
[[nodiscard]] CloudSpace measure_cloud_folder(const std::filesystem::path& root);

/// O caminho esta dentro de alguma pasta sincronizada?
///
/// Serve para barrar exclusao automatica: dentro de pasta de nuvem, apagar nao
/// e uma operacao local — ela viaja para os outros dispositivos.
[[nodiscard]] bool is_inside_cloud_folder(const std::filesystem::path& path,
                                          const std::vector<CloudFolder>& folders);

}
