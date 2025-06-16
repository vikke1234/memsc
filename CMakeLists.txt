cmake_minimum_required(VERSION 3.16)

project(memsc LANGUAGES CXX)

if (WIN32 OR APPLE)
    message(FATAL_ERROR "MemSC is only supported on Linux currently")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)
find_package(Threads REQUIRED)
add_subdirectory(src)
