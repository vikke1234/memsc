set(CMAKE_INCLUDE_CURRENT_DIR ON)
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

