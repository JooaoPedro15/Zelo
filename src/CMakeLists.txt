add_subdirectory(core)
add_subdirectory(collectors)
add_subdirectory(scanner)
add_subdirectory(commands)
add_subdirectory(storage)
add_subdirectory(ui)

# Sem WIN32: o executavel precisa de console para `--simular-limpeza` imprimir a
# lista. O Windows so cria janela para aplicativo GUI quando ele a pede, entao a
# interface continua abrindo normalmente.
qt_add_executable(zelo
    main.cpp
)

target_link_libraries(zelo PRIVATE
    Qt6::Widgets
    zelo_ui
    zelo_collectors
    zelo_storage
    spdlog::spdlog
    zelo_warnings
    zelo_static_runtime
)

# `cmake --install build/release --prefix <pasta>` produz uma pasta que roda em
# qualquer Windows, sem Qt instalado. O runtime do MinGW ja e estatico
# (zelo_static_runtime), entao o windeployqt so precisa trazer o Qt.
install(TARGETS zelo
    RUNTIME DESTINATION .
)

get_target_property(zelo_windeployqt Qt6::windeployqt IMPORTED_LOCATION)
get_filename_component(zelo_qt_bin_dir "${zelo_windeployqt}" DIRECTORY)

# O proprio windeployqt e compilado com MinGW e precisa da libstdc++ do
# toolchain, que nao mora junto do Qt.
get_filename_component(zelo_compiler_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)

# Chamada direta em vez do script gerado pelo Qt: assim o PATH usado fica sob
# controle e a falha vira mensagem legivel em vez de "violacao de acesso".
#
# O windeployqt carrega as proprias DLLs do Qt pelo PATH. Havendo outro MinGW na
# maquina, ele encontra a libstdc++ errada e morre sem dizer o motivo. Colocar o
# Qt na frente resolve, sem depender do que o usuario tem configurado.
install(CODE "
    set(ENV{PATH} \"${zelo_qt_bin_dir};${zelo_compiler_bin_dir};\$ENV{PATH}\")

    execute_process(
        COMMAND \"${zelo_windeployqt}\"
                --release
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                \"\${CMAKE_INSTALL_PREFIX}/zelo.exe\"
        RESULT_VARIABLE zelo_deploy_result
        OUTPUT_VARIABLE zelo_deploy_output
        ERROR_VARIABLE zelo_deploy_error
    )

    if(NOT zelo_deploy_result EQUAL 0)
        message(FATAL_ERROR \"windeployqt falhou (\${zelo_deploy_result}): \${zelo_deploy_error}\")
    endif()

    message(STATUS \"Qt copiado para \${CMAKE_INSTALL_PREFIX}\")
")
