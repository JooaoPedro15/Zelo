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
    PRIVATE
        zelo_collectors
        zelo_storage
        spdlog::spdlog
        zelo_warnings
)
