cmake_minimum_required(VERSION 3.5)

project(memsc LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(Threads)

add_executable(memsc
  main.cpp
  mainwindow.cpp
  mainwindow.h
  mainwindow.ui
  ProcessMemory.cpp
  ProcessMemory.h
  maps.cpp
  maps.h
  search_functions.cpp
  search_functions.h
  updater.cpp
  updater.h
)

target_compile_options(memsc PRIVATE -Wall -Wconversion)
target_link_libraries(memsc PRIVATE Qt5::Widgets ${CMAKE_THREAD_LIBS_INIT})
