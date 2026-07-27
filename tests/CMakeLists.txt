# Testes não linkam zelo_ui de propósito: mantê-los sem Qt evita depender das DLLs
# do Qt no PATH ao rodar ctest.
add_executable(zelo_tests
    smoke/modules_smoke_test.cpp
    core/protected_paths_test.cpp
    core/risk_classifier_test.cpp
    core/confidence_test.cpp
    core/health_score_test.cpp
    core/recommendation_test.cpp
    core/rules_test.cpp
)

target_link_libraries(zelo_tests PRIVATE
    zelo_core
    zelo_collectors
    zelo_scanner
    zelo_commands
    zelo_storage
    zelo_warnings
    zelo_static_runtime
    Catch2::Catch2WithMain
)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(zelo_tests)
