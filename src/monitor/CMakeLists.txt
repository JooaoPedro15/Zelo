add_library(zelo_monitor STATIC
    snapshot_store.cpp
    snapshot_taker.cpp
    growth_report.cpp
    action_log.cpp
    include/monitor/action_log.hpp
    include/monitor/growth_report.hpp
    include/monitor/snapshot.hpp
    include/monitor/snapshot_store.hpp
    include/monitor/snapshot_taker.hpp
)

target_include_directories(zelo_monitor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(zelo_monitor
    PUBLIC
        zelo_core
    PRIVATE
        # O driver SQLite ja vem no Qt instalado: nao entra dependencia nova.
        Qt6::Sql
        zelo_scanner
        spdlog::spdlog
        zelo_warnings
)
