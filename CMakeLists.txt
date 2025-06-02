cmake_minimum_required(VERSION 3.5)

project(memsc LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(Threads)

add_executable(memsc
  main.cpp
  mainwindow.cpp
  mainwindow.ui
  ProcessMemory.cpp
  maps.cpp
  )

target_compile_options(memsc PRIVATE
        -Wall
        -Wconversion
)
target_link_options(memsc PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address>
        $<$<CONFIG:Release>:-flto=auto>
)
target_link_libraries(memsc PRIVATE Qt5::Widgets ${CMAKE_THREAD_LIBS_INIT})
