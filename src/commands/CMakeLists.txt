add_library(zelo_commands STATIC
    commands.cpp
    include/commands/commands.hpp
)

target_include_directories(zelo_commands
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(zelo_commands
    PRIVATE
        zelo_warnings
)
