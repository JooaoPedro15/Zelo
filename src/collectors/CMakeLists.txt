add_library(zelo_collectors STATIC
    collectors.cpp
    include/collectors/collectors.hpp
    system_paths.cpp
    include/collectors/system_paths.hpp
)

target_include_directories(zelo_collectors
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(zelo_collectors
    PUBLIC
        zelo_core
    PRIVATE
        zelo_warnings
)
