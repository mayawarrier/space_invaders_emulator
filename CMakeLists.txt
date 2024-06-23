cmake_minimum_required(VERSION 3.15)
project(spaceinvaders)

add_executable(spaceinvaders "src/i8080/i8080.cpp" 
                             "src/i8080/i8080.hpp" 
                             "src/i8080/i8080_opcodes.h"
                             "src/utils.hpp"
                             "src/emu.hpp"
                             "src/emu.cpp"
                             "src/main.cpp")

set_property(TARGET spaceinvaders PROPERTY CXX_STANDARD 20)

# see https://wiki.libsdl.org/SDL2/README/cmake

list(APPEND CMAKE_PREFIX_PATH "./libs/")

find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)
find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2main)

if(TARGET SDL2::SDL2main)
    target_link_libraries(spaceinvaders PRIVATE SDL2::SDL2main)
endif()
target_link_libraries(spaceinvaders PRIVATE SDL2::SDL2)

if (WIN32)
    # remove console window
    #target_link_options(spaceinvaders PRIVATE "/SUBSYSTEM:WINDOWS")
    # allow fopen, etc.
    target_compile_definitions(spaceinvaders PRIVATE _CRT_SECURE_NO_WARNINGS)

    # assume SDL2 is not in the PATH in Windows
    add_custom_command(
        TARGET spaceinvaders POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different 
                                    "$<TARGET_FILE:SDL2::SDL2>" 
                                    "$<TARGET_FILE_DIR:spaceinvaders>"
        VERBATIM
    )
endif()