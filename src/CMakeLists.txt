add_subdirectory(core)
add_subdirectory(collectors)
add_subdirectory(scanner)
add_subdirectory(commands)
add_subdirectory(storage)
add_subdirectory(monitor)
add_subdirectory(ui)

# WIN32: abrir o programa nao deve mostrar uma janela de console preta ao lado
# da interface. Para `--simular-limpeza` funcionar no terminal, o programa se
# liga ao console de quem o chamou (ver attach_parent_console em main.cpp).
qt_add_executable(cleaner WIN32
    main.cpp
)

target_link_libraries(cleaner PRIVATE
    Qt6::Widgets
    cleaner_ui
    cleaner_collectors
    cleaner_storage
    spdlog::spdlog
    cleaner_warnings
    cleaner_static_runtime
)

# `cmake --install build/release --prefix <pasta>` produz uma pasta que roda em
# qualquer Windows, sem Qt instalado. O runtime do MinGW ja e estatico
# (cleaner_static_runtime), entao o windeployqt so precisa trazer o Qt.
install(TARGETS cleaner
    RUNTIME DESTINATION .
)

get_target_property(cleaner_windeployqt Qt6::windeployqt IMPORTED_LOCATION)
get_filename_component(cleaner_qt_bin_dir "${cleaner_windeployqt}" DIRECTORY)

# O proprio windeployqt e compilado com MinGW e precisa da libstdc++ do
# toolchain, que nao mora junto do Qt.
get_filename_component(cleaner_compiler_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)

# Chamada direta em vez do script gerado pelo Qt: assim o PATH usado fica sob
# controle e a falha vira mensagem legivel em vez de "violacao de acesso".
#
# O windeployqt carrega as proprias DLLs do Qt pelo PATH. Havendo outro MinGW na
# maquina, ele encontra a libstdc++ errada e morre sem dizer o motivo. Colocar o
# Qt na frente resolve, sem depender do que o usuario tem configurado.
install(CODE "
    set(ENV{PATH} \"${cleaner_qt_bin_dir};${cleaner_compiler_bin_dir};\$ENV{PATH}\")

    execute_process(
        COMMAND \"${cleaner_windeployqt}\"
                --release
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                \"\${CMAKE_INSTALL_PREFIX}/cleaner.exe\"
        RESULT_VARIABLE cleaner_deploy_result
        OUTPUT_VARIABLE cleaner_deploy_output
        ERROR_VARIABLE cleaner_deploy_error
    )

    if(NOT cleaner_deploy_result EQUAL 0)
        message(FATAL_ERROR \"windeployqt falhou (\${cleaner_deploy_result}): \${cleaner_deploy_error}\")
    endif()

    message(STATUS \"Qt copiado para \${CMAKE_INSTALL_PREFIX}\")
")
