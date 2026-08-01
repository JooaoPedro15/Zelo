#include "monitor/folder_watcher.hpp"

#include <spdlog/spdlog.h>

#include <array>
#include <ctime>
#include <map>
#include <mutex>
#include <set>
#include <thread>

#include <windows.h>

namespace zelo::monitor {

namespace {

std::string now_iso8601() {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::tm parts{};
    ::localtime_s(&parts, &now);

    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &parts);
    return buffer;
}

/// Fecha o handle sozinho: o laco de observacao tem varias saidas, e vazar
/// handle num aplicativo que fica aberto o dia todo e problema real.
class Handle {
public:
    explicit Handle(HANDLE value = INVALID_HANDLE_VALUE) : value_(value) {}

    ~Handle() {
        if (value_ != INVALID_HANDLE_VALUE && value_ != nullptr) {
            ::CloseHandle(value_);
        }
    }

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept : value_(other.value_) {
        other.value_ = INVALID_HANDLE_VALUE;
    }
    Handle& operator=(Handle&&) = delete;

    [[nodiscard]] HANDLE get() const { return value_; }
    [[nodiscard]] bool valid() const {
        return value_ != INVALID_HANDLE_VALUE && value_ != nullptr;
    }

private:
    HANDLE value_;
};

struct Accumulated {
    std::size_t event_count = 0;
    std::string first_seen;
    std::string last_seen;
    std::set<std::string> files;
};

}

struct FolderWatcher::Impl {
    mutable std::mutex mutex;
    std::map<std::string, Accumulated> activity;

    std::vector<std::jthread> workers;
    std::vector<std::shared_ptr<Handle>> directories;

    void note(const std::filesystem::path& changed) {
        const auto folder = changed.parent_path().string();
        const auto name = changed.filename().string();
        const auto when = now_iso8601();

        const std::scoped_lock lock(mutex);

        auto& entry = activity[folder];
        if (entry.event_count == 0) {
            entry.first_seen = when;
        }
        entry.last_seen = when;
        ++entry.event_count;

        // Limite por pasta: um programa que grava sem parar nao pode fazer a
        // memoria do monitor crescer sem fim. Depois disso a contagem continua,
        // so a lista de nomes para de crescer.
        constexpr std::size_t kMaxDistinctFiles = 500;
        if (entry.files.size() < kMaxDistinctFiles) {
            entry.files.insert(name);
        }
    }
};

FolderWatcher::FolderWatcher() : impl_(std::make_unique<Impl>()) {}

FolderWatcher::~FolderWatcher() {
    stop();
}

bool FolderWatcher::watch(const std::filesystem::path& folder) {
    std::error_code error;
    if (!std::filesystem::is_directory(folder, error)) {
        return false;
    }

    auto directory = std::make_shared<Handle>(::CreateFileW(
        folder.c_str(), FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr));

    if (!directory->valid()) {
        spdlog::warn("nao foi possivel observar {}", folder.string());
        return false;
    }

    impl_->directories.push_back(directory);

    impl_->workers.emplace_back([this, folder, directory](const std::stop_token& token) {
        // Prioridade baixa: observar nao pode disputar recurso com o que o
        // usuario esta realmente fazendo.
        ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

        const Handle signal(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
        if (!signal.valid()) {
            return;
        }

        // O aviso de parada precisa acordar a espera; sem ele, fechar o
        // aplicativo ficaria preso ate a proxima escrita em disco acontecer.
        const std::stop_callback wake(token, [&signal] { ::SetEvent(signal.get()); });

        std::array<std::byte, 64 * 1024> buffer{};

        while (!token.stop_requested()) {
            OVERLAPPED overlapped{};
            overlapped.hEvent = signal.get();
            ::ResetEvent(signal.get());

            if (::ReadDirectoryChangesW(directory->get(), buffer.data(),
                                        static_cast<DWORD>(buffer.size()), TRUE,
                                        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE |
                                            FILE_NOTIFY_CHANGE_LAST_WRITE,
                                        nullptr, &overlapped, nullptr) == 0) {
                return;
            }

            if (::WaitForSingleObject(signal.get(), INFINITE) != WAIT_OBJECT_0 ||
                token.stop_requested()) {
                ::CancelIo(directory->get());
                return;
            }

            DWORD produced = 0;
            if (::GetOverlappedResult(directory->get(), &overlapped, &produced, FALSE) == 0 ||
                produced == 0) {
                // Sem bytes: o sistema perdeu avisos por excesso de escrita.
                // A varredura seguinte corrige o total; aqui so nao ha o que
                // registrar.
                continue;
            }

            const auto* entry = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buffer.data());
            while (true) {
                const std::wstring name(entry->FileName,
                                        entry->FileNameLength / sizeof(wchar_t));
                impl_->note(folder / name);

                if (entry->NextEntryOffset == 0) {
                    break;
                }
                entry = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
                    reinterpret_cast<const std::byte*>(entry) + entry->NextEntryOffset);
            }
        }
    });

    return true;
}

void FolderWatcher::stop() {
    for (auto& worker : impl_->workers) {
        worker.request_stop();
    }
    impl_->workers.clear();
    impl_->directories.clear();
}

std::vector<FolderActivity> FolderWatcher::collect() const {
    const std::scoped_lock lock(impl_->mutex);

    std::vector<FolderActivity> result;
    result.reserve(impl_->activity.size());

    for (const auto& [folder, entry] : impl_->activity) {
        result.push_back(FolderActivity{
            .folder = folder,
            .event_count = entry.event_count,
            .first_seen = entry.first_seen,
            .last_seen = entry.last_seen,
            .distinct_files = entry.files.size(),
        });
    }
    return result;
}

std::vector<FolderActivity> FolderWatcher::drain() {
    auto result = collect();

    const std::scoped_lock lock(impl_->mutex);
    impl_->activity.clear();

    return result;
}

}
