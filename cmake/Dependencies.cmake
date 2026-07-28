include(FetchContent)

FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    SYSTEM
)

FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
    SYSTEM
)

FetchContent_Declare(catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    SYSTEM
)

FetchContent_MakeAvailable(nlohmann_json spdlog catch2)

# Caminhos de arquivo em wide. Sem isto o spdlog recebe o caminho como string
# estreita, e um perfil com acento — "C:\Users\João" — vira lixo ao passar pelo
# CRT, que le os bytes UTF-8 como ANSI.
if(WIN32)
    target_compile_definitions(spdlog PUBLIC SPDLOG_WCHAR_FILENAMES SPDLOG_WCHAR_TO_UTF8_SUPPORT)
endif()
