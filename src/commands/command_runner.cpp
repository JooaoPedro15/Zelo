#include "commands/command_runner.hpp"

#include <windows.h>

#include <array>
#include <utility>

namespace cleaner::commands {

namespace {

/// Fecha alcas do Windows sem exigir que cada caminho de erro lembre disso.
class Handle {
public:
    Handle() = default;
    explicit Handle(HANDLE value) : value_(value) {}

    ~Handle() { reset(); }

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    Handle(Handle&& other) noexcept : value_(std::exchange(other.value_, nullptr)) {}

    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) {
            reset();
            value_ = std::exchange(other.value_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] HANDLE get() const { return value_; }
    [[nodiscard]] bool valid() const { return value_ != nullptr && value_ != INVALID_HANDLE_VALUE; }

    HANDLE* out() { return &value_; }

    void reset() {
        if (valid()) {
            CloseHandle(value_);
        }
        value_ = nullptr;
    }

private:
    HANDLE value_ = nullptr;
};

/// A saida de um comando de console vem no codigo de pagina do console, nao em
/// UTF-8. Em um Windows em portugues isso e a diferenca entre "Concluido" e
/// bytes quebrados no meio da frase.
std::string console_output_to_utf8(const std::string& raw) {
    if (raw.empty()) {
        return {};
    }

    UINT code_page = GetConsoleOutputCP();
    if (code_page == 0) {
        // Aplicativo grafico nao tem console; o codigo OEM e o que os programas
        // do sistema usam quando a saida vai para um cano.
        code_page = GetOEMCP();
    }

    const int wide_length = MultiByteToWideChar(code_page, 0, raw.data(),
                                                static_cast<int>(raw.size()), nullptr, 0);
    if (wide_length <= 0) {
        return raw;
    }

    std::wstring wide(static_cast<std::size_t>(wide_length), L'\0');
    MultiByteToWideChar(code_page, 0, raw.data(), static_cast<int>(raw.size()), wide.data(),
                        wide_length);

    const int utf8_length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_length, nullptr, 0,
                                                nullptr, nullptr);
    if (utf8_length <= 0) {
        return raw;
    }

    std::string utf8(static_cast<std::size_t>(utf8_length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_length, utf8.data(), utf8_length, nullptr,
                        nullptr);
    return utf8;
}

std::wstring widen(const std::string& text) {
    if (text.empty()) {
        return {};
    }

    const int length =
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(),
                        length);
    return wide;
}

/// Argumento com espaco ou aspas exigiria regras de citacao, e regra de citacao
/// errada e como um argumento vira dois. Nenhum comando do catalogo precisa
/// disso, entao a resposta e recusar em vez de tentar adivinhar.
bool argument_is_simple(const std::string& argument) {
    return !argument.empty() &&
           argument.find_first_of(" \t\"^&|<>%") == std::string::npos;
}

}

std::wstring resolve_system_executable(const std::string& executable) {
    std::array<wchar_t, MAX_PATH> directory{};
    const UINT length = GetSystemDirectoryW(directory.data(), static_cast<UINT>(directory.size()));
    if (length == 0 || length >= directory.size()) {
        return {};
    }

    std::wstring path(directory.data(), length);
    path += L'\\';
    path += widen(executable);

    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return {};
    }

    return path;
}

bool running_elevated() {
    Handle token;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.out()) == 0) {
        return false;
    }

    TOKEN_ELEVATION elevation{};
    DWORD size = sizeof(elevation);
    if (GetTokenInformation(token.get(), TokenElevation, &elevation, sizeof(elevation), &size) ==
        0) {
        return false;
    }

    return elevation.TokenIsElevated != 0;
}

CommandResult run_official_command(const CommandRequest& request) {
    const OfficialCommand& command = command_by_id(request.id);

    CommandResult result;

    if (command.modifies_system && !request.confirmed) {
        result.outcome = CommandOutcome::Refused;
        result.explanation = "Este comando altera o computador e nao foi confirmado.";
        return result;
    }

    for (const auto& argument : command.arguments) {
        if (!argument_is_simple(argument)) {
            result.outcome = CommandOutcome::Refused;
            result.explanation = "Argumento fora do formato aceito: o comando nao foi executado.";
            return result;
        }
    }

    if (command.requires_elevation && !running_elevated()) {
        result.outcome = CommandOutcome::NeedsElevation;
        result.explanation = "Este comando so funciona com o Cleaner aberto como administrador.";
        return result;
    }

    const std::wstring executable = resolve_system_executable(command.executable);
    if (executable.empty()) {
        result.outcome = CommandOutcome::NotLaunched;
        result.explanation = "O programa do Windows que faria essa limpeza nao foi encontrado.";
        return result;
    }

    // O caminho vai entre aspas e os argumentos, ja verificados, vao soltos. Sem
    // interpretador de comandos no meio: nada aqui pode virar outro programa.
    std::wstring command_line = L'"' + executable + L'"';
    for (const auto& argument : command.arguments) {
        command_line += L' ';
        command_line += widen(argument);
    }

    SECURITY_ATTRIBUTES inheritable{};
    inheritable.nLength = sizeof(inheritable);
    inheritable.bInheritHandle = TRUE;

    Handle read_end;
    Handle write_end;
    if (CreatePipe(read_end.out(), write_end.out(), &inheritable, 0) == 0) {
        result.outcome = CommandOutcome::NotLaunched;
        result.explanation = "Nao foi possivel preparar a leitura da saida do comando.";
        return result;
    }

    // So a ponta que o filho escreve e herdada. Sem isso o cano nunca fecha e a
    // leitura espera para sempre por um fim que nao chega.
    SetHandleInformation(read_end.get(), HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdOutput = write_end.get();
    startup.hStdError = write_end.get();
    startup.hStdInput = nullptr;

    PROCESS_INFORMATION process_info{};
    std::wstring mutable_command_line = command_line;

    const BOOL started = CreateProcessW(executable.c_str(), mutable_command_line.data(), nullptr,
                                        nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup,
                                        &process_info);
    if (started == 0) {
        result.outcome = CommandOutcome::NotLaunched;
        result.explanation = "O Windows recusou iniciar o comando.";
        return result;
    }

    Handle process{process_info.hProcess};
    Handle thread{process_info.hThread};

    // A ponta de escrita do pai fecha agora. Enquanto ela existir, o cano nunca
    // sinaliza fim de arquivo.
    write_end.reset();

    std::string raw;
    std::array<char, 4096> buffer{};
    const auto deadline = std::chrono::steady_clock::now() + request.timeout;
    bool timed_out = false;

    while (true) {
        DWORD available = 0;
        if (PeekNamedPipe(read_end.get(), nullptr, 0, nullptr, &available, nullptr) != 0 &&
            available > 0) {
            DWORD read_bytes = 0;
            if (ReadFile(read_end.get(), buffer.data(), static_cast<DWORD>(buffer.size()),
                         &read_bytes, nullptr) != 0 &&
                read_bytes > 0) {
                raw.append(buffer.data(), read_bytes);
                continue;
            }
        }

        const DWORD waited = WaitForSingleObject(process.get(), 50);
        if (waited == WAIT_OBJECT_0) {
            // Terminou: o que sobrou no cano ainda precisa ser lido.
            DWORD remaining = 0;
            while (PeekNamedPipe(read_end.get(), nullptr, 0, nullptr, &remaining, nullptr) != 0 &&
                   remaining > 0) {
                DWORD read_bytes = 0;
                if (ReadFile(read_end.get(), buffer.data(), static_cast<DWORD>(buffer.size()),
                             &read_bytes, nullptr) == 0 ||
                    read_bytes == 0) {
                    break;
                }
                raw.append(buffer.data(), read_bytes);
            }
            break;
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            timed_out = true;
            break;
        }
    }

    result.output = console_output_to_utf8(raw);

    if (timed_out) {
        // O processo continua rodando de proposito. Interromper uma limpeza
        // oficial no meio pode deixar o Windows em estado pior do que o de
        // antes, e o Cleaner nao troca uma espera longa por um risco desses.
        result.outcome = CommandOutcome::TimedOut;
        result.explanation =
            "O comando passou do tempo previsto e continua rodando em segundo plano. "
            "Interromper uma limpeza do Windows pela metade e mais arriscado do que esperar.";
        return result;
    }

    DWORD exit_code = 0;
    GetExitCodeProcess(process.get(), &exit_code);
    result.exit_code = static_cast<int>(exit_code);

    const auto meaning =
        std::find_if(command.exit_meanings.begin(), command.exit_meanings.end(),
                     [&](const ExitMeaning& entry) { return entry.code == result.exit_code; });

    if (meaning != command.exit_meanings.end()) {
        result.outcome = meaning->success ? CommandOutcome::Success : CommandOutcome::Failed;
        result.explanation = meaning->explanation;
    } else if (exit_code == 0) {
        result.outcome = CommandOutcome::Success;
        result.explanation = "O comando terminou com sucesso.";
    } else {
        result.outcome = CommandOutcome::Failed;
        result.explanation = "O comando terminou com erro. A saida abaixo veio do proprio Windows.";
    }

    return result;
}

}
