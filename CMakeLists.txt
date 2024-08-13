cmake_minimum_required(VERSION 3.15)
project(spaceinvaders)

include(FetchContent)

set(SOURCES 
    "src/i8080/i8080.cpp" 
    "src/i8080/i8080.hpp" 
    "src/i8080/i8080_opcodes.h"
    "src/utils.hpp"
    "src/emu.hpp"
    "src/emu.cpp"
    "src/main.cpp")

if (WIN32)
    list(APPEND SOURCES 
        "src/win32.hpp"
        "src/win32.cpp")
endif()

add_executable(spaceinvaders ${SOURCES})
# decays if C++20 is not available
set_property(TARGET spaceinvaders PROPERTY CXX_STANDARD 20) 

if (WIN32)
    # remove console
    target_link_options(spaceinvaders PRIVATE "/SUBSYSTEM:WINDOWS")
    # allow fopen, etc.
    target_compile_definitions(spaceinvaders PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()


# ----------- cxxopts -----------
FetchContent_Declare(cxxopts
    GIT_REPOSITORY  https://github.com/jarro2783/cxxopts
    GIT_TAG         origin/master)
FetchContent_MakeAvailable(cxxopts)

target_link_libraries(spaceinvaders PRIVATE cxxopts)


# ------------- SDL -------------
# see https://wiki.libsdl.org/SDL2/README/cmake

# allow building without installing SDL2
list(APPEND CMAKE_PREFIX_PATH "./libs/")

find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)
find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2main)
find_package(SDL2_mixer REQUIRED CONFIG COMPONENTS SDL2_mixer)

if(TARGET SDL2::SDL2main)
    target_link_libraries(spaceinvaders PRIVATE SDL2::SDL2main)
endif()
target_link_libraries(spaceinvaders PRIVATE SDL2::SDL2)
target_link_libraries(spaceinvaders PRIVATE SDL2_mixer::SDL2_mixer)

if (WIN32)
    # copy DLLs to build folder
    add_custom_command(
        TARGET spaceinvaders POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different 
            "$<TARGET_FILE:SDL2::SDL2>" 
            "$<TARGET_FILE_DIR:spaceinvaders>"
        VERBATIM
    )
    add_custom_command(
        TARGET spaceinvaders POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different 
            "$<TARGET_FILE:SDL2_mixer::SDL2_mixer>" 
            "$<TARGET_FILE_DIR:spaceinvaders>"
        VERBATIM
    )
endif()

# -------------------------------

add_custom_command(
    TARGET spaceinvaders POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/data
        ${CMAKE_BINARY_DIR}/data
    VERBATIM
)