cmake_minimum_required(VERSION 3.16)

project(memsc LANGUAGES CXX)

if (WIN32 OR APPLE)
    message(FATAL_ERROR "MemSC is only supported on Linux currently")
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)
find_package(Threads REQUIRED)

add_executable(memsc
    main.cpp
    mainwindow.cpp
    mainwindow.ui
    ProcessMemory.cpp
    maps.cpp
    ui/MapsDialog.cpp
    ui/PidDialog.cpp
    ui/Settings.cpp
)

target_precompile_headers(memsc PRIVATE
    ui/MapsDialog.h
    ui/PidDialog.h
    ui/Settings.h
    mainwindow.h
)
target_compile_options(memsc PRIVATE
        -Wall
        -Wconversion
        -march=native
        -falign-functions=64
        -fno-omit-frame-pointer
)

target_link_options(memsc PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        $<$<CONFIG:Release>:-flto=auto -funroll-loops>
)

target_link_libraries(memsc PRIVATE Qt6::Widgets Qt6::Core Qt6::Gui
                                    Threads::Threads)

install(TARGETS memsc RUNTIME DESTINATION bin)
