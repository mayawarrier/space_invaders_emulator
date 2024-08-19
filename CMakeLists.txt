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

# -------------------------------- cxxopts --------------------------------
FetchContent_Declare(cxxopts
    GIT_REPOSITORY  https://github.com/jarro2783/cxxopts
    GIT_TAG         origin/master)
FetchContent_MakeAvailable(cxxopts)

target_link_libraries(spaceinvaders PRIVATE cxxopts)


# ---------------------------------- SDL ----------------------------------
# Absolute nightmare to get working. Here are some resources:
# https://wiki.libsdl.org/SDL2/README/cmake
# https://discourse.libsdl.org/t/how-is-sdl2-supposed-to-be-used-with-cmake/31275
# https://stackoverflow.com/questions/29191855

# Allow user to provide their own SDL2 DLLs to avoid
# installing or building from source
list(APPEND CMAKE_PREFIX_PATH "./")

find_package(SDL2 CONFIG COMPONENTS SDL2)
find_package(SDL2 CONFIG COMPONENTS SDL2main)
find_package(SDL2_mixer CONFIG COMPONENTS SDL2_mixer)

# if the first try above fails, try again using pkg-config
if (NOT TARGET SDL2::SDL2 OR NOT TARGET SDL2_mixer::SDL2_mixer)
    find_package(PkgConfig)
    if (PKG_CONFIG_FOUND)
        pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
        pkg_check_modules(SDL2_Mixer REQUIRED IMPORTED_TARGET SDL2_mixer)
    endif()
endif()

if (TARGET SDL2::SDL2 AND TARGET SDL2_mixer::SDL2_mixer)
    set(FOUND_SDL2_CMAKE_CONFIG ON)
    message(STATUS "Found SDL2 via CMake config")
elseif (TARGET PkgConfig::SDL2 AND TARGET PkgConfig::SDL2_Mixer)
    set(FOUND_SDL2_PKGCONFIG ON)
    message(STATUS "Found SDL2 via PkgConfig")
endif()

if (NOT FOUND_SDL2_CMAKE_CONFIG AND NOT FOUND_SDL2_PKGCONFIG)
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

    # unused, causes build issues
    set(SDL2MIXER_OPUS OFF)
    set(SDL2MIXER_FLAC OFF)
    set(SDL2MIXER_MOD OFF)
    set(SDL2MIXER_MIDI OFF)
    set(SDL2MIXER_WAVPACK OFF)

    # v2.6 onwards supports CMake
    FetchContent_Declare(SDL2_mixer
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer
        GIT_TAG        ${FETCH_SDL2_MIXER_VERSION})
    FetchContent_MakeAvailable(SDL2_mixer)

    set(FOUND_SDL2_CMAKE_CONFIG ON)
endif()

if (FOUND_SDL2_CMAKE_CONFIG)
    if(TARGET SDL2::SDL2main)
        target_link_libraries(spaceinvaders PRIVATE SDL2::SDL2main)
    endif()
    target_link_libraries(spaceinvaders PRIVATE SDL2::SDL2)
    target_link_libraries(spaceinvaders PRIVATE SDL2_mixer::SDL2_mixer)

    set(TARGET_NAME_SDL2 "SDL2::SDL2")
    set(TARGET_NAME_SDL2_MIXER "SDL2_mixer::SDL2_mixer")
else()
    target_link_libraries(spaceinvaders PRIVATE PkgConfig::SDL2)
    target_link_libraries(spaceinvaders PRIVATE PkgConfig::SDL2_Mixer)

    set(TARGET_NAME_SDL2 "PkgConfig::SDL2")
    set(TARGET_NAME_SDL2_MIXER "PkgConfig::SDL2_Mixer")
endif()

# https://stackoverflow.com/questions/20433308/
if (WIN32)
    add_custom_command(
        TARGET spaceinvaders POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different 
            "$<TARGET_FILE:${TARGET_NAME_SDL2}>" 
            "$<TARGET_FILE_DIR:spaceinvaders>"
        VERBATIM
    )
    add_custom_command(
        TARGET spaceinvaders POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different 
            "$<TARGET_FILE:${TARGET_NAME_SDL2_MIXER}>" 
            "$<TARGET_FILE_DIR:spaceinvaders>"
        VERBATIM
    )
endif()

# -------------------------------------------------------------------------
