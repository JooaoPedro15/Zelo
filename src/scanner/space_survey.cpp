#include "scanner/space_survey.hpp"

#include "scanner/storage_scanner.hpp"

#include <core/classify/classifier.hpp>

#include <windows.h>

#include <algorithm>
#include <map>

namespace cleaner::scanner {

namespace {

/// Normaliza para comparar caminhos de pai e filho.
///
/// A varredura devolve os caminhos como o Windows os entregou. Sem tirar a
/// barra final, "C:\Users\" e "C:\Users" seriam dois nos diferentes para a
/// mesma pasta, e a soma subiria pelo galho errado.
std::string normalized(std::string path) {
    while (path.size() > 3 && (path.back() == '\\' || path.back() == '/')) {
        path.pop_back();
    }
    return path;
}

std::string parent_of(const std::string& path) {
    const auto slash = path.find_last_of("\\/");
    if (slash == std::string::npos) {
        return {};
    }

    // "C:\algo" tem pai "C:\", nao "C:".
    if (slash <= 2) {
        return path.substr(0, 3);
    }
    return path.substr(0, slash);
}

std::string leaf_of(const std::string& path) {
    const auto slash = path.find_last_of("\\/");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::size_t depth_of(const std::string& path, const std::string& root) {
    if (path.size() <= root.size()) {
        return 0;
    }
    return static_cast<std::size_t>(
        std::count(path.begin() + static_cast<std::ptrdiff_t>(root.size()), path.end(), '\\'));
}

/// O que cada pasta acumula, antes de virar arvore.
struct Bucket {
    std::uint64_t allocated = 0;
    std::uint64_t logical = 0;
    std::uint64_t online_only = 0;
    std::size_t files = 0;
    std::size_t unreadable = 0;
};

void read_volume(const std::filesystem::path& root, core::SpaceSurvey& survey) {
    ULARGE_INTEGER free_for_caller{};
    ULARGE_INTEGER total{};
    ULARGE_INTEGER free_total{};

    if (GetDiskFreeSpaceExW(root.wstring().c_str(), &free_for_caller, &total, &free_total) != 0) {
        survey.volume_total_bytes = total.QuadPart;
        survey.volume_free_bytes = free_total.QuadPart;
    }
}

}

core::SpaceSurvey survey_space(const std::filesystem::path& root, const std::string& volume,
                               SurveyOptions options, std::stop_token token,
                               const SurveyProgress& progress) {
    core::SpaceSurvey survey;
    survey.volume = volume;
    read_volume(root, survey);

    // A varredura pesada e a mesma de sempre: ela ja trata caminho longo,
    // permissao negada, reparse point, hard link e placeholder de nuvem. Aqui
    // so se pede que ela emita toda pasta, para haver por onde a soma subir.
    const StorageScanner scanner{ScanOptions{
        .largest_files_kept = 0,
        .rollup_depth = 0,
        .emit_all_directories = true,
        .unreadable_paths_kept = 50,
    }};

    if (progress) {
        progress(root.string());
    }

    const auto result = scanner.scan(root, token);

    survey.complete = result.completed;
    survey.unreadable_count = result.skipped_count;
    survey.unreadable_examples = result.unreadable_paths;

    // O total do volume vem do proprio scanner, que ja descontou hard link
    // repetido e ja deixou o que mora na nuvem de fora do alocado.
    survey.identified_bytes = result.allocated_bytes;

    const std::string root_key = normalized(root.string());

    // Primeira passada: cada pasta com o que ha solto nela.
    std::map<std::string, Bucket> buckets;
    for (const auto& directory : result.directories) {
        auto& bucket = buckets[normalized(directory.path)];
        bucket.allocated += directory.allocated_bytes;
        bucket.logical += directory.total_bytes;
        bucket.files += directory.file_count;
    }

    for (const auto& path : result.unreadable_paths) {
        ++buckets[normalized(path)].unreadable;
    }

    // Segunda passada: soma de baixo para cima. Um `std::map` de string entrega
    // as chaves em ordem alfabetica, e nela o filho vem sempre depois do pai —
    // entao percorrer de tras para frente e visitar o mais fundo primeiro.
    std::map<std::string, Bucket> totals = buckets;
    for (auto entry = totals.rbegin(); entry != totals.rend(); ++entry) {
        if (entry->first == root_key) {
            continue;
        }

        const auto parent = parent_of(entry->first);
        if (parent.empty() || parent.size() < root_key.size()) {
            continue;
        }

        auto& target = totals[parent];
        target.allocated += entry->second.allocated;
        target.logical += entry->second.logical;
        target.online_only += entry->second.online_only;
        target.files += entry->second.files;
        target.unreadable += entry->second.unreadable;
    }

    // Terceira passada: os nos que a interface vai mostrar.
    //
    // Ficam num `std::map` ate a montagem terminar. Guardar ponteiro para
    // elemento de vector nao funcionaria: a proxima insercao realoca o vetor e
    // todos os ponteiros anteriores passam a apontar para memoria liberada.
    std::map<std::string, core::SpaceNode> flat;

    for (const auto& [path, bucket] : totals) {
        if (path == root_key || bucket.allocated < options.minimum_node_bytes) {
            continue;
        }
        if (depth_of(path, root_key) > options.tree_depth) {
            continue;
        }

        core::SpaceNode node;
        node.path = path;
        node.display_name = leaf_of(path);
        node.allocated_bytes = bucket.allocated;
        node.logical_bytes = bucket.logical;
        node.file_count = bucket.files;
        node.unreadable_count = bucket.unreadable;

        flat.emplace(path, std::move(node));
    }

    survey.root.path = root_key;
    survey.root.display_name = root_key;
    survey.root.allocated_bytes = totals[root_key].allocated;
    survey.root.logical_bytes = totals[root_key].logical;
    survey.root.online_only_bytes = result.online_only_bytes;
    survey.root.file_count = totals[root_key].files;
    survey.root.unreadable_count = totals[root_key].unreadable;

    // Do mais fundo para o mais raso: quando um no e movido para dentro do pai,
    // os filhos dele ja estao la dentro.
    for (auto entry = flat.rbegin(); entry != flat.rend(); ++entry) {
        auto parent = parent_of(entry->first);

        // Pai que ficou de fora por tamanho ou profundidade: sobe ate achar um
        // ancestral presente. Pendurar direto na raiz faria o galho aparecer
        // duas vezes, uma dentro do ancestral e outra solta.
        while (!parent.empty() && parent.size() >= root_key.size() && parent != root_key &&
               flat.find(parent) == flat.end()) {
            parent = parent_of(parent);
        }

        if (parent == root_key || parent.empty() || parent.size() < root_key.size()) {
            survey.root.children.push_back(std::move(entry->second));
            continue;
        }

        flat[parent].children.push_back(std::move(entry->second));
    }

    // Maiores primeiro, em cada nivel: e a ordem em que o usuario procura.
    const auto sort_children = [](auto&& self, core::SpaceNode& node) -> void {
        std::sort(node.children.begin(), node.children.end(),
                  [](const core::SpaceNode& left, const core::SpaceNode& right) {
                      return left.allocated_bytes > right.allocated_bytes;
                  });
        for (auto& child : node.children) {
            self(self, child);
        }
    };
    sort_children(sort_children, survey.root);

    // Quarta passada: o que ha em cada no.
    //
    // A soma por classe anda so pelas folhas. Contar pai e filho juntos dobraria
    // os bytes, e a barra de "quanto da para limpar" mentiria para mais — que e
    // o erro mais caro que um limpador pode cometer.
    const core::ContentClassifier content;

    const auto classify_tree = [&content](auto&& self, core::SpaceNode& node,
                                          core::SpaceByClass& totals) -> void {
        node.classification = content.classify(node.path);

        for (auto& child : node.children) {
            self(self, child, totals);
        }

        if (!node.children.empty()) {
            return;
        }

        switch (node.classification.content_class) {
        case core::ContentClass::SafeToClean:
            totals.safe_bytes += node.allocated_bytes;
            break;
        case core::ContentClass::CleanWithConsequence:
            totals.consequence_bytes += node.allocated_bytes;
            break;
        case core::ContentClass::NeedsReview:
            totals.review_bytes += node.allocated_bytes;
            break;
        case core::ContentClass::Protected:
            totals.protected_bytes += node.allocated_bytes;
            break;
        }
    };

    classify_tree(classify_tree, survey.root, survey.by_class);

    return survey;
}

}
