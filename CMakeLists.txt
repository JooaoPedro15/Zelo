cmake_minimum_required(VERSION 3.25)

project(cleaner
    VERSION 0.1.0
    DESCRIPTION "Cleaner - Windows Care Assistant"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Sql)
qt_standard_project_setup(REQUIRES 6.8)

# Warnings rígidos só para os targets do projeto; dependências FetchContent ficam de fora.
add_library(cleaner_warnings INTERFACE)
target_compile_options(cleaner_warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Werror

    # Os modelos usam designated initializers e todo campo tem valor padrao,
    # entao omitir um campo e intencional e nunca deixa lixo na memoria.
    -Wno-missing-field-initializers
)

# Runtime MinGW estático nos executáveis: a máquina tem um MinGW 6.3 antigo no PATH
# cujas DLLs (libstdc++-6.dll) seriam resolvidas no lugar das do toolchain (0xc0000139).
add_library(cleaner_static_runtime INTERFACE)
if(MINGW)
    target_link_options(cleaner_static_runtime INTERFACE -static)
endif()

include(cmake/Dependencies.cmake)

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
