cmake_minimum_required(VERSION 3.15)
project(spaceinvaders)

include(FetchContent)

# Fix DOWNLOAD_EXTRACT_TIMESTAMP warning in CMake >= 3.24
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

set(SOURCES 
    "src/i8080/i8080.hpp" 
    "src/i8080/i8080_opcodes.h"
    "src/i8080/i8080.cpp" 
    "src/utils.hpp"
    "src/utils.cpp"
    "src/emu.hpp"
    "src/emu.cpp"
    "src/gui.hpp"
    "src/gui.cpp"
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
    TARGET spaceinvaders PRE_BUILD # pre_build for emscripten
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/data
        ${CMAKE_BINARY_DIR}/data
    VERBATIM
)

add_custom_command(
    TARGET spaceinvaders PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/src/spaceinvaders.ini
        ${CMAKE_BINARY_DIR}/spaceinvaders.ini
    VERBATIM
)

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, default to Release")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

# -------------------------------- cxxopts --------------------------------
FetchContent_Declare(cxxopts
    GIT_REPOSITORY  https://github.com/jarro2783/cxxopts
    GIT_TAG         origin/master)
FetchContent_MakeAvailable(cxxopts)

target_link_libraries(spaceinvaders PRIVATE cxxopts)

# ---------------------------------- SDL ----------------------------------

# Nightmare to get working. Here are some resources:
# https://wiki.libsdl.org/SDL2/README/cmake
# https://discourse.libsdl.org/t/how-is-sdl2-supposed-to-be-used-with-cmake/31275
# https://stackoverflow.com/questions/29191855
# https://emscripten.org/docs/compiling/Building-Projects.html

if (EMSCRIPTEN)
    # --use-port or target_compile_options() do not appear to work
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_SDL=2 -s USE_SDL_MIXER=2")

    # todo: remove this later
    set_target_properties(spaceinvaders PROPERTIES SUFFIX ".html")
    # todo: more reasonable memory limit?
    set_target_properties(spaceinvaders PROPERTIES LINK_FLAGS "\
        -s ASSERTIONS=1\
        -s ALLOW_MEMORY_GROWTH=1\
        -s MAXIMUM_MEMORY=4GB\
        --preload-file data/\
        --preload-file spaceinvaders.ini")
else()

    # Allow user to provide their own SDL2 builds to avoid
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
                        Downloading SDL2 source...")

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

        # unused
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
endif()

# -------------------------------- imgui --------------------------------

# https://github.com/ocornut/imgui/wiki/Getting-Started#compilinglinking
# https://github.com/ocornut/imgui/discussions/5970

set(FETCH_IMGUI_VERSION "v1.91.0")

FetchContent_Declare(imgui_ext
    URL https://github.com/ocornut/imgui/archive/refs/tags/${FETCH_IMGUI_VERSION}.zip
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(imgui_ext)

file(GLOB IMGUI_SOURCES 
    ${imgui_ext_SOURCE_DIR}/*.cpp
    ${imgui_ext_SOURCE_DIR}/*.h)

list(APPEND IMGUI_SOURCES
    ${imgui_ext_SOURCE_DIR}/misc/cpp/imgui_stdlib.h
    ${imgui_ext_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp)

if (CMAKE_GENERATOR MATCHES "Visual Studio")
    list(APPEND IMGUI_SOURCES
        ${imgui_ext_SOURCE_DIR}/misc/debuggers/imgui.natvis
        ${imgui_ext_SOURCE_DIR}/misc/debuggers/imgui.natstepfilter)
endif()

set(IMGUI_BACKEND_SOURCES 
    ${imgui_ext_SOURCE_DIR}/backends/imgui_impl_sdl2.h
    ${imgui_ext_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_ext_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.h
    ${imgui_ext_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp)

add_library(imgui ${IMGUI_SOURCES} ${IMGUI_BACKEND_SOURCES})

target_include_directories(imgui PUBLIC 
    ${imgui_ext_SOURCE_DIR} 
    ${imgui_ext_SOURCE_DIR}/backends
    ${imgui_ext_SOURCE_DIR}/misc/cpp
)
if (NOT EMSCRIPTEN)
    target_link_libraries(imgui PUBLIC ${TARGET_NAME_SDL2} ${TARGET_NAME_SDL2_MIXER})
endif()

target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
if (NOT CMAKE_BUILD_TYPE MATCHES "Debug")
    target_compile_definitions(imgui PUBLIC 
        IMGUI_DISABLE_DEMO_WINDOWS 
        IMGUI_DISABLE_DEBUG_TOOLS)
endif()

target_link_libraries(spaceinvaders PRIVATE imgui)

# -----------------------------------------------------------------------
