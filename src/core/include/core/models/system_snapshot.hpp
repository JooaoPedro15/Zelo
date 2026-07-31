#pragma once

#include "core/models/known_location.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zelo::core {

/// O que os coletores conseguiram observar em uma analise. As regras leem
/// daqui e nunca tocam no sistema — e por isso que elas rodam em teste sem
/// depender do Windows.
///
/// Todo campo tem um par `*_available`: dado ausente nao e o mesmo que dado
/// zerado. Sem essa distincao a analise concluiria "nenhum arquivo temporario"
/// quando na verdade nao conseguiu olhar.
struct VolumeInfo {
    std::string letter;
    std::uint64_t total_bytes = 0;
    std::uint64_t free_bytes = 0;
    bool is_system = false;

    [[nodiscard]] double free_ratio() const {
        return total_bytes == 0 ? 0.0 : static_cast<double>(free_bytes) / static_cast<double>(total_bytes);
    }
};

struct StartupItemInfo {
    std::string name;
    std::string publisher;
    std::string path;

    /// Antivirus, drivers, componentes de audio e video, ferramentas de
    /// hardware. O app nunca sugere desativar estes.
    bool essential = false;
};

struct TemporaryFilesInfo {
    bool available = false;
    std::uint64_t total_bytes = 0;
    std::size_t file_count = 0;
};

/// Falhas de um mesmo programa agrupadas. O usuario nao deve ver evento bruto:
/// o que importa e qual programa falhou, quantas vezes e desde quando.
struct AppFailureInfo {
    std::string application;

    /// Componente apontado como causa, quando o Windows registra. Costuma ser
    /// uma DLL, e ajuda a distinguir falha do proprio programa de falha de um
    /// driver ou biblioteca compartilhada.
    std::string faulting_module;

    std::size_t count = 0;
    std::string first_seen;
    std::string last_seen;
};

struct StabilityInfo {
    bool available = false;

    std::vector<AppFailureInfo> app_failures;

    /// Desligamentos e reinicializacoes que o Windows registrou como
    /// inesperados — queda de energia, travamento total, tela azul.
    std::size_t unexpected_shutdowns = 0;

    /// Janela observada, para a recomendacao poder dizer "nos ultimos N dias"
    /// em vez de dar um numero sem referencia.
    int window_days = 0;
};

struct UpdatesInfo {
    bool available = false;

    /// O Windows aplicou algo que so termina depois de reiniciar. Ate la o
    /// sistema fica num estado intermediario, que costuma explicar lentidao e
    /// falhas que somem sozinhas depois do reinicio.
    bool reboot_pending = false;

    /// De onde veio o indicio, para a recomendacao poder mostrar em vez de
    /// apenas afirmar.
    std::vector<std::string> reboot_reasons;
};

struct MemoryInfo {
    bool available = false;
    std::uint64_t total_bytes = 0;
    std::uint64_t available_bytes = 0;

    /// Quanto da memoria comprometida esta em uso, como o proprio Windows
    /// calcula. Uso alto nao e defeito: o Windows aproveita memoria livre como
    /// cache de proposito. So a falta de memoria disponivel indica aperto.
    int load_percent = 0;

    [[nodiscard]] double available_ratio() const {
        return total_bytes == 0 ? 0.0
                                : static_cast<double>(available_bytes) / static_cast<double>(total_bytes);
    }
};

struct SecurityInfo {
    bool available = false;

    bool antivirus_enabled = false;
    bool realtime_protection_enabled = false;

    /// Idade das definicoes, em dias. Definicao velha nao significa desprotegido,
    /// mas significa protegido contra ameacas de semanas atras.
    int signature_age_days = -1;

    /// Dias desde a ultima verificacao rapida. Negativo quando o Windows nao
    /// informa — em maquina recem-instalada, por exemplo.
    int last_quick_scan_age_days = -1;

    /// Nome do antivirus em uso, quando nao for o do proprio Windows. O Zelo nao
    /// substitui antivirus e nao opina sobre qual usar.
    std::string provider;
};

struct PhysicalDiskInfo {
    std::string model;

    /// SSD, HDD ou desconhecido, como o Windows classifica.
    std::string media_type;

    std::uint64_t size_bytes = 0;

    /// O que o Windows responde sobre a saude: "Healthy", "Warning" ou
    /// "Unhealthy". Um unico indicador dizendo que esta tudo bem nao autoriza
    /// afirmar que o disco esta bem — so que nada foi detectado.
    std::string health_status;

    /// Contadores de confiabilidade. Ficam negativos quando o driver nao os
    /// expoe, o que e comum em NVMe e em disco externo. Ausencia nao e zero.
    int read_errors = -1;
    int write_errors = -1;
    int temperature_celsius = -1;
    int wear_percent = -1;
};

/// Corrupcao na estrutura do sistema de arquivos de um volume, registrada pelo
/// proprio NTFS. E problema diferente de componente do Windows danificado: a
/// ferramenta aqui e o chkdsk, nao o sfc.
struct FilesystemCorruptionInfo {
    std::size_t event_count = 0;
    std::vector<std::string> affected_volumes;
    std::string last_seen;
};

struct DisksInfo {
    bool available = false;
    std::vector<PhysicalDiskInfo> disks;

    bool filesystem_events_available = false;
    FilesystemCorruptionInfo filesystem_corruption;
};

/// Sinais de que arquivos ou componentes do Windows podem estar danificados.
///
/// A deteccao e por leitura de eventos: o MVP nao executa sfc nem DISM, que
/// exigem administrador e demoram. O que o Zelo faz e reunir a evidencia de que
/// vale a pena executa-los, e explicar o que cada um faz.
struct IntegrityInfo {
    bool available = false;

    /// Eventos do servico de componentes indicando corrupcao detectada.
    std::size_t corruption_events = 0;

    /// Eventos de componente lado a lado com manifesto ou assembly invalido.
    std::size_t component_events = 0;

    std::string last_seen;
    int window_days = 0;
};

struct SystemSnapshot {
    bool volumes_available = false;
    std::vector<VolumeInfo> volumes;

    bool startup_available = false;
    std::vector<StartupItemInfo> startup_items;

    TemporaryFilesInfo temporary_files;
    StabilityInfo stability;
    UpdatesInfo updates;
    MemoryInfo memory;
    SecurityInfo security;
    DisksInfo disks;
    IntegrityInfo integrity;
    ReclaimableInfo reclaimable;
};

}
