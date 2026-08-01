add_library(cleaner_storage STATIC
    storage.cpp
    include/storage/storage.hpp
    json_codec.cpp
    include/storage/json_codec.hpp
    history_store.cpp
    include/storage/history_store.hpp
    logging.cpp
    include/storage/logging.hpp
    quarantine_store.cpp
    include/storage/quarantine_store.hpp
    cloud_release.cpp
    include/storage/cloud_release.hpp
    cleanup_service.cpp
    include/storage/cleanup_service.hpp
)

target_include_directories(cleaner_storage
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cleaner_storage
    PUBLIC
        cleaner_core
        nlohmann_json::nlohmann_json
    PRIVATE
        cleaner_scanner
        spdlog::spdlog
        cleaner_warnings
)
