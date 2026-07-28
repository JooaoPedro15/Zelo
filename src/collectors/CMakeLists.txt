add_library(zelo_collectors STATIC
    collectors.cpp
    include/collectors/collectors.hpp
    system_paths.cpp
    include/collectors/system_paths.hpp
    volume_collector.cpp
    include/collectors/volume_collector.hpp
    startup_collector.cpp
    include/collectors/startup_collector.hpp
    temporary_files_collector.cpp
    include/collectors/temporary_files_collector.hpp
    snapshot_collector.cpp
    include/collectors/snapshot_collector.hpp
    detail/text.cpp
    include/collectors/detail/text.hpp
)

target_include_directories(zelo_collectors
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(zelo_collectors
    PUBLIC
        zelo_core
    PRIVATE
        zelo_scanner
        zelo_warnings
)
