add_library(zelo_scanner STATIC
    scanner.cpp
    include/scanner/scanner.hpp
    storage_scanner.cpp
    include/scanner/storage_scanner.hpp
    volume_geometry.cpp
    include/scanner/volume_geometry.hpp
)

target_include_directories(zelo_scanner
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(zelo_scanner
    PRIVATE
        zelo_warnings
)
