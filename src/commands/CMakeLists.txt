add_library(cleaner_commands STATIC
    commands.cpp
    command_catalog.cpp
    command_runner.cpp
    include/commands/commands.hpp
    include/commands/command_catalog.hpp
    include/commands/command_runner.hpp
)

target_include_directories(cleaner_commands
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cleaner_commands
    PRIVATE
        cleaner_warnings
)
