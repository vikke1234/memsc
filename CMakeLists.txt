cmake_minimum_required(VERSION 3.16)

project(memsc
    DESCRIPTION "A memory scanner for reverse engineers"
    LANGUAGES CXX
)

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
configure_file(
    packaging/memsc.desktop.in
    ${CMAKE_BINARY_DIR}/memsc.desktop
    @ONLY
)


install(TARGETS memsc)
install(FILES ${CMAKE_BINARY_DIR}/memsc.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(
  FILES ${CMAKE_SOURCE_DIR}/packaging/memsc-icon.png
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/
)


include(CPack)
