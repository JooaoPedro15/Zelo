qt_add_library(zelo_ui STATIC
    ui.cpp
    include/ui/ui.hpp
)

target_include_directories(zelo_ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(zelo_ui
    PRIVATE
        Qt6::Widgets
        zelo_warnings
)
