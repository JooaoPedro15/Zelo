qt_add_library(cleaner_ui STATIC
    ui.cpp
    include/ui/ui.hpp
    presentation.cpp
    include/ui/presentation.hpp
    main_window.cpp
    include/ui/main_window.hpp
)

target_include_directories(cleaner_ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cleaner_ui
    PUBLIC
        Qt6::Widgets
        cleaner_core
        # O cabecalho da janela declara o observador de pastas, entao quem
        # inclui a janela precisa enxergar o modulo.
        cleaner_monitor
    PRIVATE
        cleaner_collectors
        cleaner_commands
        cleaner_storage
        spdlog::spdlog
        cleaner_warnings
)
