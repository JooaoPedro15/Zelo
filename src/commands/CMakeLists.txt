add_library(cleaner_commands STATIC
    commands.cpp
    command_catalog.cpp
    command_runner.cpp
    startup_control.cpp
    installer_cache.cpp
    include/commands/commands.hpp
    include/commands/command_catalog.hpp
    include/commands/command_runner.hpp
    include/commands/startup_control.hpp
    include/commands/installer_cache.hpp
)

target_include_directories(cleaner_commands
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cleaner_commands
    PUBLIC
        cleaner_core
    PRIVATE
        cleaner_warnings
)
