#pragma once

#include <core/models/system_snapshot.hpp>

#include <string>
#include <vector>

namespace zelo::collectors {

/// Le os programas configurados para iniciar com o Windows, a partir das
/// chaves Run e das pastas de inicializacao, do usuario e da maquina.
///
/// Somente leitura: nao desativa nem altera nada.
class StartupCollector {
public:
    bool collect_into(core::SystemSnapshot& snapshot) const;

    [[nodiscard]] std::vector<core::StartupItemInfo> collect() const;
};

/// Reconhece itens que nunca devem ser sugeridos para desativacao: antivirus,
/// audio, video e ferramentas de hardware.
///
/// E heuristica por nome e caminho, entao erra de proposito para o lado de
/// marcar demais: um item essencial classificado como comum apareceria numa
/// sugestao de desativar, e desativar protecao ou driver e o pior desfecho
/// possivel aqui. O contrario apenas deixa de sugerir algo.
[[nodiscard]] bool looks_essential(const std::string& name, const std::string& path);

/// Extrai o executavel de uma linha de comando de inicializacao.
///
/// O registro guarda a linha inteira, com aspas e argumentos:
/// `"C:\...\steam.exe" -silent`. Sem separar, o caminho nunca casaria com a
/// deny-list nem serviria para abrir a pasta do programa.
[[nodiscard]] std::string executable_from_command(const std::string& command);

}
