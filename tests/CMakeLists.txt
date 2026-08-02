add_executable(cleaner_tests
    qt_environment.cpp
    smoke/modules_smoke_test.cpp
    core/protected_paths_test.cpp
    core/risk_classifier_test.cpp
    core/confidence_test.cpp
    core/classifier_test.cpp
    core/health_score_test.cpp
    core/recommendation_test.cpp
    core/rules_test.cpp
    core/quick_analysis_test.cpp
    scanner/temporary_tree.cpp
    scanner/storage_scanner_test.cpp
    scanner/space_survey_test.cpp
    collectors/system_paths_test.cpp
    collectors/volume_collector_test.cpp
    collectors/startup_collector_test.cpp
    collectors/real_analysis_test.cpp
    collectors/event_log_parsing_test.cpp
    collectors/app_profiles_test.cpp
    collectors/running_apps_test.cpp
    collectors/cloud_folders_test.cpp
    collectors/stability_collector_test.cpp
    collectors/security_collector_test.cpp
    collectors/disk_collector_test.cpp
    collectors/wmi_test.cpp
    commands/official_commands_test.cpp
    commands/installer_cache_test.cpp
    storage/json_codec_test.cpp
    storage/history_store_test.cpp
    storage/logging_test.cpp
    storage/quarantine_store_test.cpp
    storage/cleanup_service_test.cpp
    monitor/snapshot_store_test.cpp
    monitor/snapshot_taker_test.cpp
    monitor/real_snapshot_test.cpp
    monitor/growth_report_test.cpp
    monitor/growth_alerts_test.cpp
    monitor/snapshot_flow_test.cpp
    monitor/action_log_test.cpp
    monitor/folder_watcher_test.cpp
    ui/presentation_test.cpp
)

target_link_libraries(cleaner_tests PRIVATE
    cleaner_core
    cleaner_collectors
    cleaner_scanner
    cleaner_commands
    cleaner_storage
    cleaner_ui
    cleaner_monitor
    cleaner_warnings
    spdlog::spdlog
    cleaner_static_runtime
    # Sem WithMain: os testes tem entrada propria, que cria a instancia de
    # aplicativo Qt exigida para carregar o driver de banco.
    Catch2::Catch2
    Qt6::Core
)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

# Os testes de apresentação usam QString e QColor, então o binário carrega as
# DLLs do Qt. DL_PATHS as coloca no PATH tanto na descoberta quanto na execução.
catch_discover_tests(cleaner_tests
    DL_PATHS "$<TARGET_FILE_DIR:Qt6::Core>"
)
