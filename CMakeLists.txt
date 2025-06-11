cmake_minimum_required(VERSION 3.16)

project(memsc LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(Threads REQUIRED)

add_executable(memsc
    main.cpp
    mainwindow.cpp
    mainwindow.ui
    ProcessMemory.cpp
    maps.cpp
    ui/MapsDialog.h
    ui/MapsDialog.cpp
	ui/PidDialog.cpp
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

target_link_libraries(memsc PRIVATE Threads::Threads Qt5::Widgets ${CMAKE_THREAD_LIBS_INIT})
