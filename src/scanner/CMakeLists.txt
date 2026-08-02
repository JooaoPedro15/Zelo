add_library(cleaner_scanner STATIC
    scanner.cpp
    include/scanner/scanner.hpp
    storage_scanner.cpp
    space_survey.cpp
    include/scanner/space_survey.hpp
    include/scanner/storage_scanner.hpp
    volume_geometry.cpp
    include/scanner/volume_geometry.hpp
)

target_include_directories(cleaner_scanner
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cleaner_scanner
    PUBLIC
        cleaner_core
    PRIVATE
        cleaner_warnings
)
