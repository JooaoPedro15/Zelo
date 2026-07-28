add_library(zelo_storage STATIC
    storage.cpp
    include/storage/storage.hpp
    json_codec.cpp
    include/storage/json_codec.hpp
    history_store.cpp
    include/storage/history_store.hpp
)

target_include_directories(zelo_storage
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(zelo_storage
    PUBLIC
        zelo_core
        nlohmann_json::nlohmann_json
    PRIVATE
        zelo_warnings
)
