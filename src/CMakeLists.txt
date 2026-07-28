add_subdirectory(core)
add_subdirectory(collectors)
add_subdirectory(scanner)
add_subdirectory(commands)
add_subdirectory(storage)
add_subdirectory(ui)

qt_add_executable(zelo WIN32
    main.cpp
)

target_link_libraries(zelo PRIVATE
    Qt6::Widgets
    zelo_ui
    zelo_storage
    spdlog::spdlog
    zelo_warnings
    zelo_static_runtime
)
