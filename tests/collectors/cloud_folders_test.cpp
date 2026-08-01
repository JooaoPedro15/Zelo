#include <catch2/catch_test_macros.hpp>
#include <collectors/cloud_folders.hpp>
#include <storage/cloud_release.hpp>

using zelo::collectors::CloudFolder;
using zelo::collectors::cloud_folders;
using zelo::collectors::is_inside_cloud_folder;
using zelo::collectors::measure_cloud_folder;
using zelo::storage::plan_cloud_release;

TEST_CASE("as pastas de nuvem saem do registro do servico", "[cloud]") {
    const auto folders = cloud_folders();

    for (const auto& folder : folders) {
        INFO(folder.path);
        CHECK_FALSE(folder.path.empty());
        CHECK(folder.service == "OneDrive");
        CHECK_FALSE(folder.account.empty());

        // Conta sem pasta local nao entra na lista: ela nao ocupa disco, e
        // mostra-la levaria o usuario a procurar espaco onde nao ha nada.
        CHECK(std::filesystem::exists(folder.path));
    }
}

TEST_CASE("caminho de fora nao passa por dentro da nuvem", "[cloud]") {
    const std::vector<CloudFolder> folders = {
        CloudFolder{.path = "C:\\Users\\Alguem\\OneDrive", .service = "OneDrive"},
    };

    CHECK(is_inside_cloud_folder("C:\\Users\\Alguem\\OneDrive", folders));
    CHECK(is_inside_cloud_folder("C:\\Users\\Alguem\\OneDrive\\Documentos\\a.txt", folders));

    // Prefixo igual nao e a mesma pasta. Sem a checagem de limite, uma pasta
    // vizinha herdaria protecao — ou perderia a dela.
    CHECK_FALSE(is_inside_cloud_folder("C:\\Users\\Alguem\\OneDriveAntigo\\a.txt", folders));
    CHECK_FALSE(is_inside_cloud_folder("C:\\Users\\Alguem\\Documentos", folders));
}

TEST_CASE("a comparacao ignora maiusculas como o Windows", "[cloud]") {
    const std::vector<CloudFolder> folders = {
        CloudFolder{.path = "C:\\Users\\Alguem\\OneDrive", .service = "OneDrive"},
    };

    CHECK(is_inside_cloud_folder("c:\\users\\alguem\\onedrive\\a.txt", folders));
}

// Arquivo que mora so na nuvem nao ocupa disco. Somar os dois numeros como se
// fossem um seria prometer espaco que nao existe.
TEST_CASE("medir separa o que esta no disco do que esta so na nuvem", "[cloud]") {
    const auto folders = cloud_folders();
    if (folders.empty()) {
        SUCCEED("nao ha pasta de nuvem configurada neste computador");
        return;
    }

    const auto space = measure_cloud_folder(folders.front().path);

    INFO("no disco: " << space.local_bytes << " bytes em " << space.local_files << " arquivos");
    INFO("so na nuvem: " << space.online_only_bytes << " bytes em " << space.online_only_files
                         << " arquivos");

    CHECK(space.path == folders.front().path);
    CHECK(space.local_files + space.online_only_files > 0);

    if (space.online_only_files == 0) {
        CHECK(space.online_only_bytes == 0);
    }
}

// Liberar espaco local so faz sentido sobre o que esta baixado. O que ja esta na
// nuvem nao entra na conta, senao o plano prometeria liberar duas vezes.
TEST_CASE("o plano de liberar espaco conta so o que esta baixado", "[cloud]") {
    const auto folders = cloud_folders();
    if (folders.empty()) {
        SUCCEED("nao ha pasta de nuvem configurada neste computador");
        return;
    }

    const auto plan = plan_cloud_release(folders.front().path);
    const auto space = measure_cloud_folder(folders.front().path);

    CHECK(plan.root == folders.front().path);
    CHECK(plan.file_count + plan.pinned_kept <= space.local_files);
}
