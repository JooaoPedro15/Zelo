add_library(zelo_collectors STATIC
    collectors.cpp
    include/collectors/collectors.hpp
)

target_include_directories(zelo_collectors
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(zelo_collectors
    PRIVATE
        zelo_warnings
)
