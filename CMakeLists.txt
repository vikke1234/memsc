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

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
# Update submodules as needed
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endif()

find_package(Qt6 COMPONENTS Widgets REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenMP REQUIRED)

set(CAPSTONE_BUILD_SHARED_LIBS ON)
set(CAPSTONE_X86_REDUCE ON)
# Add extra architectures as need arises
set(CAPSTONE_ARCHITECTURE_DEFAULT OFF)
set(CAPSTONE_X86_SUPPORT ON)
add_subdirectory(3rdparty/capstone)
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
