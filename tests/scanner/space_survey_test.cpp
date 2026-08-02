#include "temporary_tree.hpp"

#include <catch2/catch_test_macros.hpp>
#include <scanner/space_survey.hpp>

#include <algorithm>
#include <array>

#include <windows.h>

using cleaner::core::SpaceNode;
using cleaner::scanner::SurveyOptions;
using cleaner::scanner::survey_space;
using cleaner::testing::TemporaryTree;

namespace {

const SpaceNode* find_child(const SpaceNode& node, const std::string& name) {
    const auto found =
        std::find_if(node.children.begin(), node.children.end(),
                     [&name](const SpaceNode& child) { return child.display_name == name; });
    return found == node.children.end() ? nullptr : &*found;
}

std::uint64_t sum_children(const SpaceNode& node) {
    std::uint64_t total = 0;
    for (const auto& child : node.children) {
        total += child.allocated_bytes;
    }
    return total;
}

// Sem teto de tamanho: a arvore do teste tem arquivos de poucos kilobytes, e o
// padrao de 32 MB deixaria tudo de fora.
SurveyOptions everything() {
    return SurveyOptions{.tree_depth = 8, .minimum_node_bytes = 1};
}

}

TEST_CASE("a arvore soma de baixo para cima", "[space_survey]") {
    TemporaryTree tree;
    tree.add_file("apps/editor/cache/a.bin", 40000);
    tree.add_file("apps/editor/cache/b.bin", 30000);
    tree.add_file("apps/editor/config.json", 1000);
    tree.add_file("documentos/nota.txt", 5000);

    const auto survey = survey_space(tree.root(), "T:", everything());

    REQUIRE(survey.root.allocated_bytes > 0);

    const auto* apps = find_child(survey.root, "apps");
    REQUIRE(apps != nullptr);

    const auto* editor = find_child(*apps, "editor");
    REQUIRE(editor != nullptr);

    const auto* cache = find_child(*editor, "cache");
    REQUIRE(cache != nullptr);

    // O pai carrega o que ha nele mais tudo que ha abaixo.
    CHECK(editor->allocated_bytes > cache->allocated_bytes);
    CHECK(apps->allocated_bytes == editor->allocated_bytes);
    CHECK(survey.root.allocated_bytes > apps->allocated_bytes);
}

// O galho nao pode aparecer duas vezes: uma dentro do pai e outra solto na
// raiz. Se aparecesse, o total da raiz contaria os mesmos bytes em dobro.
TEST_CASE("nenhum galho e contado duas vezes", "[space_survey]") {
    TemporaryTree tree;
    tree.add_file("a/b/c/arquivo.bin", 50000);
    tree.add_file("a/outro.bin", 20000);

    const auto survey = survey_space(tree.root(), "T:", everything());

    CHECK(survey.root.children.size() == 1);
    CHECK(survey.root.allocated_bytes >= sum_children(survey.root));
}

TEST_CASE("pasta pequena some da arvore mas continua na soma", "[space_survey]") {
    TemporaryTree tree;
    tree.add_file("grande/arquivo.bin", 200000);
    tree.add_file("miudo/nada.txt", 10);

    const auto survey = survey_space(
        tree.root(), "T:", SurveyOptions{.tree_depth = 8, .minimum_node_bytes = 100000});

    const auto* grande = find_child(survey.root, "grande");
    REQUIRE(grande != nullptr);

    // Fora da arvore por ser pequena demais para virar linha propria.
    CHECK(find_child(survey.root, "miudo") == nullptr);

    // Mas o que ha nela continua no total da raiz: sumir da vista nao pode
    // significar sumir da conta.
    CHECK(survey.root.allocated_bytes > grande->allocated_bytes);
}

TEST_CASE("o limite de profundidade nao muda o total", "[space_survey]") {
    TemporaryTree tree;
    tree.add_file("a/b/c/d/e/fundo.bin", 80000);

    const auto raso =
        survey_space(tree.root(), "T:", SurveyOptions{.tree_depth = 1, .minimum_node_bytes = 1});
    const auto fundo =
        survey_space(tree.root(), "T:", SurveyOptions{.tree_depth = 9, .minimum_node_bytes = 1});

    CHECK(raso.root.allocated_bytes == fundo.root.allocated_bytes);
    CHECK(raso.root.children.size() == 1);
}

TEST_CASE("a varredura cancelada se declara incompleta", "[space_survey]") {
    TemporaryTree tree;
    tree.add_file("a/arquivo.bin", 1000);

    std::stop_source source;
    source.request_stop();

    const auto survey = survey_space(tree.root(), "T:", everything(), source.get_token());

    CHECK_FALSE(survey.complete);
}

// A conta com o Windows e o motivo da tela existir: sem ela o usuario ve uma
// lista de pastas e nao sabe se ela explica o disco ou so um pedaco dele.
TEST_CASE("o espaco do volume vem do Windows", "[space_survey][integration]") {
    const auto survey = survey_space("C:\\Windows\\Fonts", "C:", everything());

    REQUIRE(survey.volume_total_bytes > 0);
    CHECK(survey.used_bytes() > 0);
    CHECK(survey.used_bytes() < survey.volume_total_bytes);

    // Varrendo so uma pasta, quase todo o disco fica sem explicacao, e o
    // numero precisa dizer isso.
    CHECK(survey.unexplained_bytes() > 0);
    CHECK(survey.coverage() < 1.0);
}

// O caso que motivou a tela: o AppData deste computador tem dezenas de GB e a
// analise antiga mostrava so os poucos caminhos que ela conhecia de cor.
TEST_CASE("o AppData inteiro entra na conta", "[space_survey][integration]") {
    std::array<wchar_t, 32767> buffer{};
    const DWORD written =
        GetEnvironmentVariableW(L"LOCALAPPDATA", buffer.data(), static_cast<DWORD>(buffer.size()));
    if (written == 0) {
        SUCCEED("sem LOCALAPPDATA nesta maquina");
        return;
    }

    const std::filesystem::path local(std::wstring(buffer.data(), written));
    const auto survey = survey_space(local.parent_path(), "C:", SurveyOptions{});

    INFO("AppData ocupa " << survey.identified_bytes << " bytes");
    INFO("nos de primeiro nivel: " << survey.root.children.size());
    INFO("pastas ilegiveis: " << survey.unreadable_count);
    for (const auto& child : survey.root.children) {
        INFO(child.display_name << " = " << child.allocated_bytes);
    }

    CHECK(survey.identified_bytes > 0);

    // A soma dos filhos nao pode passar do pai: se passasse, algum galho estaria
    // sendo contado duas vezes.
    CHECK(sum_children(survey.root) <= survey.root.allocated_bytes);
}
