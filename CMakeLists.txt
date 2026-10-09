cmake_minimum_required(VERSION 3.16)
project(bc3_visor VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(bc3_visor
    src/main.cpp
    src/Presupuesto.cpp
    src/ModeloPresupuesto.cpp
    src/MainWindow.cpp
    src/GastoReal.cpp
)
target_link_libraries(bc3_visor PRIVATE Qt6::Widgets)
if (WIN32)
    set_target_properties(bc3_visor PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
if (APPLE)
    # Necesario para que macdeployqt pueda empaquetar un .app / .dmg
    set_target_properties(bc3_visor PROPERTIES MACOSX_BUNDLE TRUE)
endif()

# Prueba de humo: el binario arranca (sin pantalla) y responde a --help.
enable_testing()
if (NOT WIN32) # en Windows el .exe es de ventana y --help abriría un diálogo
    add_test(NAME arranque_ayuda COMMAND bc3_visor --help)
    set_tests_properties(arranque_ayuda PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
