add_library(cleaner_monitor STATIC
    snapshot_store.cpp
    snapshot_taker.cpp
    growth_report.cpp
    growth_alerts.cpp
    action_log.cpp
    folder_watcher.cpp
    include/monitor/action_log.hpp
    include/monitor/folder_watcher.hpp
    include/monitor/growth_alerts.hpp
    include/monitor/growth_report.hpp
    include/monitor/snapshot.hpp
    include/monitor/snapshot_store.hpp
    include/monitor/snapshot_taker.hpp
)

target_include_directories(cleaner_monitor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cleaner_monitor
    PUBLIC
        cleaner_core
    PRIVATE
        # O driver SQLite ja vem no Qt instalado: nao entra dependencia nova.
        Qt6::Sql
        cleaner_scanner
        spdlog::spdlog
        cleaner_warnings
)
