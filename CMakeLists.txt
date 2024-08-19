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
set_property(TARGET spaceinvaders PROPERTY CXX_STANDARD 20) 

target_compile_definitions(spaceinvaders PRIVATE
    COMPILER_NAME=${CMAKE_CXX_COMPILER_ID}
    COMPILER_VERSION=${CMAKE_CXX_COMPILER_VERSION}
    OS_NAME=${CMAKE_SYSTEM_NAME}
    OS_VERSION=${CMAKE_SYSTEM_VERSION}
)
if (DEFINED ${CMAKE_CXX_SIMULATE_ID})
    target_compile_definitions(spaceinvaders PRIVATE 
        COMPILER_FRONTNAME=${CMAKE_CXX_SIMULATE_ID})
endif()

# allow fopen, etc.
if (WIN32) 
    target_compile_definitions(spaceinvaders PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# remove console
if (MINGW)
    target_link_options(spaceinvaders PRIVATE "-Wl,-subsystem,windows")
elseif (WIN32)
    target_link_options(spaceinvaders PRIVATE "/SUBSYSTEM:WINDOWS")
endif()

add_custom_command(
    TARGET spaceinvaders POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/data
        ${CMAKE_BINARY_DIR}/data
    VERBATIM
)

# ---------------------- cxxopts ----------------------
FetchContent_Declare(cxxopts
    GIT_REPOSITORY  https://github.com/jarro2783/cxxopts
    GIT_TAG         origin/master)
FetchContent_MakeAvailable(cxxopts)

target_link_libraries(spaceinvaders PRIVATE cxxopts)

# ------------------------ SDL ------------------------
# see https://wiki.libsdl.org/SDL2/README/cmake

# Allow user to provide their own SDL2 DLLs without 
# installing or building from source
list(APPEND CMAKE_PREFIX_PATH "./")

find_package(SDL2 CONFIG COMPONENTS SDL2)
find_package(SDL2 CONFIG COMPONENTS SDL2main)
find_package(SDL2_mixer CONFIG COMPONENTS SDL2_mixer)

if (NOT SDL2_SDL2_FOUND OR NOT SDL2_mixer_FOUND)
    message(WARNING "Could not find SDL2 DLLs and/or SDL2 CMake config files.\
                    Downloading and building SDL2 from source...")

    if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # clang-cl
        # https://github.com/libsdl-org/SDL/issues/6214
        set(FETCH_SDL2_VERSION "release-2.0.22")
        set(FETCH_SDL2_MIXER_VERSION "release-2.6.0")
    else()
        # latest version as of 08/24, tested.
        set(FETCH_SDL2_VERSION "release-2.30.3")
        set(FETCH_SDL2_MIXER_VERSION "release-2.8.0")
    endif()

    FetchContent_Declare(SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL
        GIT_TAG        ${FETCH_SDL2_VERSION})
    FetchContent_MakeAvailable(SDL2)

    if (MINGW)
        # unused. broken even after installing libraries
        set(SDL2MIXER_OPUS OFF)
        set(SDL2MIXER_FLAC OFF)
        set(SDL2MIXER_MOD OFF)
        set(SDL2MIXER_MIDI OFF)
        set(SDL2MIXER_WAVPACK OFF)
    endif()

    # v2.6 onwards supports CMake
    FetchContent_Declare(SDL2_mixer
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer
        GIT_TAG        ${FETCH_SDL2_MIXER_VERSION})
    FetchContent_MakeAvailable(SDL2_mixer)
endif()

if(TARGET SDL2::SDL2main)
    target_link_libraries(spaceinvaders PRIVATE SDL2::SDL2main)
endif()
target_link_libraries(spaceinvaders PRIVATE SDL2::SDL2)
target_link_libraries(spaceinvaders PRIVATE SDL2_mixer::SDL2_mixer)

# Make damn sure program can find SDL2 DLLs at runtime
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

# ------------------------------------------
