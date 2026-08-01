qt_add_library(zelo_ui STATIC
    ui.cpp
    include/ui/ui.hpp
    presentation.cpp
    include/ui/presentation.hpp
    main_window.cpp
    include/ui/main_window.hpp
)

target_include_directories(zelo_ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(zelo_ui
    PUBLIC
        Qt6::Widgets
        zelo_core
        # O cabecalho da janela declara o observador de pastas, entao quem
        # inclui a janela precisa enxergar o modulo.
        zelo_monitor
    PRIVATE
        zelo_collectors
        zelo_commands
        zelo_storage
        spdlog::spdlog
        zelo_warnings
)
