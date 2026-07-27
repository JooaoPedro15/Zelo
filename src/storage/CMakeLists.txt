add_library(zelo_storage STATIC
    storage.cpp
    include/storage/storage.hpp
)

target_include_directories(zelo_storage
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(zelo_storage
    PRIVATE
        zelo_warnings
)
