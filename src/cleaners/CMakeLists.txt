add_library(cleaner_cleaners STATIC
    cleaner_engine.cpp
    available_cleaners.cpp
    include/cleaners/cleaner_engine.hpp
)

target_include_directories(cleaner_cleaners
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cleaner_cleaners
    PUBLIC
        cleaner_core
    PRIVATE
        spdlog::spdlog
        cleaner_warnings
)
