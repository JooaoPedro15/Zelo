#pragma once

#include <filesystem>
#include <string>

namespace cleaner::storage {

/// Prepara o registro em arquivo. Tudo fica na maquina: nada e enviado para
/// servidor nenhum, nem agora nem por engano depois.
///
/// Chamar mais de uma vez e inofensivo — a segunda chamada nao faz nada.
void initialize_logging(const std::filesystem::path& directory);

/// O arquivo em uso, para a interface poder mostrar ao usuario onde olhar.
[[nodiscard]] std::filesystem::path current_log_file();

/// Apaga registros antigos. Log de diagnostico e util por semanas, nao para
/// sempre — e quem usa o Cleaner costuma estar sem espaco.
void apply_log_retention(const std::filesystem::path& directory, int keep_days);

/// Solta o arquivo atual para que a proxima inicializacao valha. Existe apenas
/// para os testes conseguirem exercitar diretorios diferentes no mesmo processo.
void reset_logging_for_test();

}
