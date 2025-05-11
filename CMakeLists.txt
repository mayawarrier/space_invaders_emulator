cmake_minimum_required(VERSION 3.15)
project(space-invaders-emu)

include(FetchContent)

# Fix DOWNLOAD_EXTRACT_TIMESTAMP warning in CMake >= 3.24
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, default to Release")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

set(SOURCES 
    "src/build_info.cpp"
    "src/i8080/i8080_opcodes.hpp"
    "src/i8080/i8080.hpp" 
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

add_executable(spaceinvaders "${SOURCES}")

set_property(TARGET spaceinvaders PROPERTY CXX_STANDARD 20) 
set_property(TARGET spaceinvaders PROPERTY CXX_STANDARD_REQUIRED ON)

target_compile_definitions(spaceinvaders PRIVATE
    CC_NAME=${CMAKE_CXX_COMPILER_ID}
    CC_VERSION=${CMAKE_CXX_COMPILER_VERSION}
    $<$<BOOL:${CMAKE_CXX_SIMULATE_ID}>:CC_SIMNAME=${CMAKE_CXX_SIMULATE_ID}>
    OS_NAME=${CMAKE_SYSTEM_NAME}
    OS_VERSION=${CMAKE_SYSTEM_VERSION})

# allow fopen, etc.
if (WIN32) 
    target_compile_definitions(spaceinvaders PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if (EMSCRIPTEN)
    # reduces binary size. not necessary for other platforms
    target_compile_options(spaceinvaders PUBLIC -fno-rtti -fno-exceptions)
    target_compile_definitions(spaceinvaders PRIVATE DISABLE_EXCEPTIONS_AND_RTTI)
endif()

# remove console
if (MINGW)
    target_link_options(spaceinvaders PRIVATE "-Wl,-subsystem,windows")
elseif (WIN32)
    target_link_options(spaceinvaders PRIVATE "/SUBSYSTEM:WINDOWS")
endif()

if (EMSCRIPTEN)
    target_link_options(spaceinvaders PRIVATE
        "-sALLOW_MEMORY_GROWTH=1"
        "-sEXPORTED_RUNTIME_METHODS=ccall")
endif()

if (EMSCRIPTEN)
    set(EMBED_BINNAME "embed_assets")
    set(PREJS_BINNAME "index-prejs.js")

    set(EMBED_SRCDIR "${CMAKE_SOURCE_DIR}/assets")
    set(EMBED_BINDIR "${CMAKE_BINARY_DIR}/${EMBED_BINNAME}")
    set(PREJS_SRCFILE "${CMAKE_SOURCE_DIR}/web/src/index-prejs.js")
    set(PREJS_BINFILE "${CMAKE_BINARY_DIR}/${PREJS_BINNAME}")

    target_link_options(spaceinvaders PRIVATE
        "--embed-file=${EMBED_BINNAME}/@assets/"
        "--pre-js=${PREJS_BINNAME}")

    # dummy file, touched to force a relink when assets/pre-js change
    set(RELINK_FILE "${CMAKE_BINARY_DIR}/emcc_relink.h")

    add_custom_command(
        OUTPUT "${RELINK_FILE}" "${EMBED_BINDIR}" "${PREJS_BINFILE}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${EMBED_SRCDIR}" "${EMBED_BINDIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy "${PREJS_SRCFILE}" "${PREJS_BINFILE}"
        COMMAND "${CMAKE_COMMAND}" -E touch "${RELINK_FILE}"
        DEPENDS "${EMBED_SRCDIR}" "${PREJS_SRCFILE}"
        VERBATIM)

    add_custom_target(relink_files DEPENDS "${RELINK_FILE}" "${EMBED_BINDIR}" "${PREJS_BINFILE}")

    # Output directories are not cleaned by default. Ninja clean fails without this
    # https://stackoverflow.com/questions/53734146
    set_property(TARGET relink_files APPEND PROPERTY ADDITIONAL_CLEAN_FILES "${EMBED_BINDIR}")

    add_dependencies(spaceinvaders relink_files)
    target_include_directories(spaceinvaders PRIVATE "${CMAKE_BINARY_DIR}")

    set(ASSET_SRCDIR "${CMAKE_SOURCE_DIR}/web/assets")
    set(ASSET_BINDIR "${CMAKE_BINARY_DIR}/assets")
    set(WEB_SRCFILE "${CMAKE_SOURCE_DIR}/web/src/index.html")
    set(WEB_BINFILE "${CMAKE_BINARY_DIR}/index.html")

    add_custom_command(
        OUTPUT "${ASSET_BINDIR}" "${WEB_BINFILE}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${ASSET_SRCDIR}" "${ASSET_BINDIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy "${WEB_SRCFILE}" "${WEB_BINFILE}"
        DEPENDS "${ASSET_SRCDIR}" "${WEB_SRCFILE}"
        VERBATIM)

    add_custom_target(web_files DEPENDS "${ASSET_BINDIR}" "${WEB_BINFILE}")
    # See note above about cleaning output directories.
    set_property(TARGET relink_files APPEND PROPERTY ADDITIONAL_CLEAN_FILES "${ASSET_BINDIR}")

    add_dependencies(spaceinvaders web_files)

else()
    set(ASSET_SRCDIR "${CMAKE_SOURCE_DIR}/assets")
    set(ASSET_BINDIR "${CMAKE_BINARY_DIR}/assets")

    add_custom_command(
        OUTPUT "${ASSET_BINDIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${ASSET_SRCDIR}" "${ASSET_BINDIR}"
        DEPENDS "${ASSET_SRCDIR}"
        VERBATIM)

    add_custom_target(emu_assets DEPENDS ${ASSET_BINDIR})
    # See note above about cleaning output directories.
    set_property(TARGET emu_assets APPEND PROPERTY ADDITIONAL_CLEAN_FILES "${ASSET_BINDIR}") 

    add_dependencies(spaceinvaders emu_assets)
endif()

set(BUILDNUM_FILE "${CMAKE_BINARY_DIR}/build_num.h")
set(BUILDNUM_SCRIPT "${CMAKE_BINARY_DIR}/gen_buildnum.cmake")

find_program(GIT_EXECUTABLE git)

if (NOT GIT_EXECUTABLE)
    message(WARNING "Could not find Git. Build number will not be generated.\
        If CMake was run before installing Git, clear the CMake cache and re-run.")
    file(WRITE "${BUILDNUM_FILE}" "#define BUILD_NUM 0")

else()
    file(GENERATE OUTPUT "${BUILDNUM_SCRIPT}" CONTENT "
    file(WRITE \"${BUILDNUM_FILE}\" \"#define BUILD_NUM \")

    execute_process(
        COMMAND \"${GIT_EXECUTABLE}\" rev-parse --short HEAD
        RESULT_VARIABLE GIT_RESULT
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    if(GIT_RESULT EQUAL 0)
        file(APPEND \"${BUILDNUM_FILE}\" \"\${GIT_HASH}\\n\")
    else()
        message(WARNING \"git rev-parse failed. Build number will not be generated.\")
        file(APPEND \"${BUILDNUM_FILE}\" \"0\\n\")
    endif()
    ")

    # todo: this is always out of date!
    add_custom_target(buildnum
        BYPRODUCTS "${BUILDNUM_FILE}"
        COMMAND "${CMAKE_COMMAND}" -P "${BUILDNUM_SCRIPT}"
        VERBATIM)

    add_dependencies(spaceinvaders buildnum)
    target_include_directories(spaceinvaders PRIVATE "${CMAKE_BINARY_DIR}")
endif()

# this license stuff is mostly paranoid, but it's a good exercise.
set(EXT_LICENSES_FILENAME "THIRD_PARTY_LICENSES.txt")
set(EXT_LICENSES_BINFILE "${CMAKE_BINARY_DIR}/${EXT_LICENSES_FILENAME}")

file(WRITE ${EXT_LICENSES_BINFILE} "Third Party Licenses\n\n")

function(add_license HEADER LICENSE_LOCATION)
    file(READ "${LICENSE_LOCATION}" LICENSE_CONTENT)  
    file(APPEND "${EXT_LICENSES_BINFILE}" "------------------------------------------\n")
    file(APPEND "${EXT_LICENSES_BINFILE}" "${HEADER}\n")  
    file(APPEND "${EXT_LICENSES_BINFILE}" "------------------------------------------\n\n")
    file(APPEND "${EXT_LICENSES_BINFILE}" "${LICENSE_CONTENT}\n\n")
endfunction()

function(find_add_license HEADER PACKAGE_DIR)
    if (EXISTS "${PACKAGE_DIR}/LICENSE")
        add_license("${HEADER}" "${PACKAGE_DIR}/LICENSE")
    elseif (EXISTS "${PACKAGE_DIR}/COPYING")
        add_license("${HEADER}" "${PACKAGE_DIR}/COPYING")
    elseif (EXISTS "${PACKAGE_DIR}/LICENSE.txt")
        add_license("${HEADER}" "${PACKAGE_DIR}/LICENSE.txt")
    elseif (EXISTS "${PACKAGE_DIR}/COPYING.txt")
        add_license("${HEADER}" "${PACKAGE_DIR}/COPYING.txt")
    else()
        message(WARNING "Could not find license for ${HEADER} in ${PACKAGE_DIR}")
    endif()
endfunction()

if (EMSCRIPTEN AND NOT DEFINED EMSCRIPTEN_ROOT_PATH)
    message(WARNING "EMSCRIPTEN_ROOT_PATH not set.\
        Cannot locate emscripten or SDL2 port license files without it.")
endif()

function(get_emcc_port_version PORT_NAME OUT_PORT_VERSION)
    set(${OUT_PORT_VERSION} "" PARENT_SCOPE)

    set(EMCC_PORT_PYFILE "${EMSCRIPTEN_ROOT_PATH}/tools/ports/${PORT_NAME}.py")
    if (NOT EXISTS "${EMCC_PORT_PYFILE}")
        message(WARNING "Could not find port file ${EMCC_PORT_PYFILE}")
        return()
    endif()

    file(READ "${EMCC_PORT_PYFILE}" PYFILE_CONTENTS)
    string(REGEX MATCH "TAG *= *['\"]([^'\"]*)['\"]" _ ${PYFILE_CONTENTS})
    if (NOT CMAKE_MATCH_1)
        message(WARNING "Could not find ${PORT_NAME} version in ${EMCC_PORT_PYFILE}")
        return()
    endif()

    set(${OUT_PORT_VERSION} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

function(download_check_file URL FILE)
    file(DOWNLOAD "${URL}" "${FILE}" STATUS DOWNLOAD_STATUS)

    list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
    if(NOT STATUS_CODE EQUAL 0)
        message(FATAL_ERROR "Failed to download ${URL}")
    endif()
endfunction()

# ------------------------------ emscripten -------------------------------

if (EMSCRIPTEN)
    if (DEFINED EMSCRIPTEN_ROOT_PATH)
        add_license("emscripten ${EMSCRIPTEN_VERSION}" 
            "${EMSCRIPTEN_ROOT_PATH}/LICENSE")
        add_license("musl" 
            "${EMSCRIPTEN_ROOT_PATH}/system/lib/libc/musl/COPYRIGHT")
    endif()
endif()

# -------------------------------- cxxopts --------------------------------

if (NOT EMSCRIPTEN)
    set(FETCH_CXXOPTS_VERSION "v3.2.0")

    FetchContent_Declare(cxxopts
        GIT_REPOSITORY  https://github.com/jarro2783/cxxopts
        GIT_TAG         ${FETCH_CXXOPTS_VERSION})
    FetchContent_MakeAvailable(cxxopts)

    add_license("cxxopts ${FETCH_CXXOPTS_VERSION}" 
        "${cxxopts_SOURCE_DIR}/LICENSE")

    target_link_libraries(spaceinvaders PRIVATE cxxopts)
endif()

# ---------------------------------- SDL ----------------------------------

# Configuring SDL2 properly is a nightmare. See:
# https://wiki.libsdl.org/SDL2/README/cmake
# https://discourse.libsdl.org/t/how-is-sdl2-supposed-to-be-used-with-cmake/31275
# https://stackoverflow.com/questions/29191855
# https://emscripten.org/docs/compiling/Building-Projects.html

if (EMSCRIPTEN)
    # --use-port does not work?
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_SDL=2 -s USE_SDL_MIXER=2")

    if (DEFINED EMSCRIPTEN_ROOT_PATH)
        set(LIBSDL_REPO_ROOT "https://raw.githubusercontent.com/libsdl-org/SDL")
        set(LIBSDL_MIXER_REPO_ROOT "https://raw.githubusercontent.com/libsdl-org/SDL_mixer")

        get_emcc_port_version("sdl2" PORT_SDL2_VERSION)
        get_emcc_port_version("sdl2_mixer" PORT_SDL2_MIXER_VERSION)

        if (PORT_SDL2_VERSION)
            download_check_file(
                "${LIBSDL_REPO_ROOT}/${PORT_SDL2_VERSION}/LICENSE.txt"
                "${CMAKE_BINARY_DIR}/SDL2_LICENSE")
            add_license("SDL2" 
                "${CMAKE_BINARY_DIR}/SDL2_LICENSE")
        else()
            message(WARNING "Could not download SDL2 license!")
        endif()
        
        if (PORT_SDL2_MIXER_VERSION)
            download_check_file(
                "${LIBSDL_MIXER_REPO_ROOT}/${PORT_SDL2_MIXER_VERSION}/LICENSE.txt"
                "${CMAKE_BINARY_DIR}/SDL2_mixer_LICENSE")
            add_license("SDL2_mixer" 
                "${CMAKE_BINARY_DIR}/SDL2_mixer_LICENSE")
        else()
             message(WARNING "Could not download SDL2_mixer license!")
        endif()
    endif()

else()
    set(ALLOW_SDL2_SRCBUILD_HELP 
        "If SDL2/SDL2_Mixer libraries cannot be found, build them from source.")

    if (WIN32)
        option(ALLOW_SDL2_SRCBUILD ${ALLOW_SDL2_SRCBUILD_HELP} ON)
    else()
        option(ALLOW_SDL2_SRCBUILD ${ALLOW_SDL2_SRCBUILD_HELP} OFF)
    endif()

    if (WIN32)
        # Allow user to provide their own SDL (via SDL2-devel-VC.zip)
        # to avoid building from source.
        list(APPEND CMAKE_PREFIX_PATH "./")
    endif()

    # Try the usual way.
    find_package(SDL2 CONFIG COMPONENTS SDL2)
    find_package(SDL2 CONFIG COMPONENTS SDL2main)
    find_package(SDL2_mixer CONFIG COMPONENTS SDL2_mixer)

    # Try again with pkg-config.
    if (NOT TARGET SDL2::SDL2 OR NOT TARGET SDL2_mixer::SDL2_mixer)
        find_package(PkgConfig)
        if (PKG_CONFIG_FOUND)
            pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
            pkg_check_modules(SDL2_Mixer REQUIRED IMPORTED_TARGET SDL2_mixer)
        endif()
    endif()

    if (TARGET SDL2::SDL2 AND TARGET SDL2_mixer::SDL2_mixer)
        set(FOUND_SDL2_FINDPKG ON)
        message(STATUS "Found SDL2 via find_package()")
    elseif (TARGET PkgConfig::SDL2 AND TARGET PkgConfig::SDL2_Mixer)
        set(FOUND_SDL2_PKGCONFIG ON)
        message(STATUS "Found SDL2 via pkg-config")
    endif()

    # If SDL2 libraries cannot be found, build from source.
    if (NOT FOUND_SDL2_FINDPKG AND NOT FOUND_SDL2_PKGCONFIG)
        if (ALLOW_SDL2_SRCBUILD)
            message(WARNING "Could not find SDL2 libraries and/or config files.\
                SDL2 libraries will be built from source.")
        else()
            message(FATAL_ERROR "Could not find SDL2 libraries and/or config files.\
                Set ALLOW_SDL2_SRCBUILD to allow building from source.")
        endif()

        if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            # clang-cl bug, https://github.com/libsdl-org/SDL/issues/6214
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

        add_license("SDL2 ${FETCH_SDL2_VERSION}" 
            "${SDL2_SOURCE_DIR}/LICENSE.txt")

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

        add_license("SDL2_mixer ${FETCH_SDL2_MIXER_VERSION}" 
            "${SDL2_mixer_SOURCE_DIR}/LICENSE.txt")

        set(FETCHED_SDL2_SRC ON)
        set(FOUND_SDL2_FINDPKG ON)
    endif()

    if (NOT FOUND_SDL2_FINDPKG AND NOT FOUND_SDL2_PKGCONFIG)
        message(FATAL_ERROR "Config var not set?")
    endif()

    if (FOUND_SDL2_FINDPKG)
        set(TARGET_NAME_SDL2 "SDL2::SDL2")
        set(TARGET_NAME_SDL2_MIXER "SDL2_mixer::SDL2_mixer")
    else()
        set(TARGET_NAME_SDL2 "PkgConfig::SDL2")
        set(TARGET_NAME_SDL2_MIXER "PkgConfig::SDL2_Mixer")
    endif()

    if (FOUND_SDL2_FINDPKG AND TARGET SDL2::SDL2main)
        target_link_libraries(spaceinvaders PRIVATE SDL2::SDL2main)
    endif()

    target_link_libraries(spaceinvaders PRIVATE 
        ${TARGET_NAME_SDL2} ${TARGET_NAME_SDL2_MIXER})

    # https://stackoverflow.com/questions/20433308/
    if (WIN32)
        add_custom_command(
            TARGET spaceinvaders 
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different 
                "$<TARGET_FILE:${TARGET_NAME_SDL2}>" 
                "$<TARGET_FILE_DIR:spaceinvaders>"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different 
                "$<TARGET_FILE:${TARGET_NAME_SDL2_MIXER}>" 
                "$<TARGET_FILE_DIR:spaceinvaders>"
            VERBATIM)

        set_property(TARGET spaceinvaders 
            APPEND PROPERTY ADDITIONAL_CLEAN_FILES 
            "$<TARGET_FILE_DIR:spaceinvaders>/$<TARGET_FILE_NAME:${TARGET_NAME_SDL2}>"
            "$<TARGET_FILE_DIR:spaceinvaders>/$<TARGET_FILE_NAME:${TARGET_NAME_SDL2_MIXER}>")

        # since the DLLs are re-distributed, need to add the licenses as well
        if (NOT FETCHED_SDL2_SRC)
            if (FOUND_SDL2_FINDPKG)
                if (DEFINED SDL2_PREFIX)
                    set(PREFIX_DIR_SDL2 "${SDL2_PREFIX}")
                elseif (DEFINED SDL2_DIR)
                    set(PREFIX_DIR_SDL2 "${SDL2_DIR}/..")
                endif()

                find_add_license("SDL2" "${PREFIX_DIR_SDL2}")
                find_add_license("SDL2_mixer" "${SDL2_mixer_DIR}/..")
            else()
                message(WARNING "Unimplemented: cannot find \
                    SDL2 licenses on Windows when using pkg-config.")
            endif()
        endif()
    endif()
endif()

# ----------------------------- CascadiaCode ------------------------------

add_license("CascadiaCode 2407.24" "${CMAKE_SOURCE_DIR}/third_party/LICENSE_CascadiaCode.txt")

# -------------------------------- imgui --------------------------------

# https://github.com/ocornut/imgui/wiki/Getting-Started#compilinglinking
# https://github.com/ocornut/imgui/discussions/5970

set(FETCH_IMGUI_VERSION "v1.91.0")

FetchContent_Declare(imgui_ext
    URL https://github.com/ocornut/imgui/archive/refs/tags/${FETCH_IMGUI_VERSION}.zip
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(imgui_ext)

file(GLOB IMGUI_SOURCES 
    "${imgui_ext_SOURCE_DIR}/*.cpp"
    "${imgui_ext_SOURCE_DIR}/*.h")

if (CMAKE_GENERATOR MATCHES "Visual Studio")
    list(APPEND IMGUI_SOURCES
        "${imgui_ext_SOURCE_DIR}/misc/debuggers/imgui.natvis"
        "${imgui_ext_SOURCE_DIR}/misc/debuggers/imgui.natstepfilter")
endif()

set(IMGUI_BACKEND_SOURCES 
    "${imgui_ext_SOURCE_DIR}/backends/imgui_impl_sdl2.h"
    "${imgui_ext_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp"
    "${imgui_ext_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.h"
    "${imgui_ext_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp")

add_library(imgui "${IMGUI_SOURCES}" "${IMGUI_BACKEND_SOURCES}")

target_include_directories(imgui PUBLIC 
    "${imgui_ext_SOURCE_DIR}"
    "${imgui_ext_SOURCE_DIR}/backends")

target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)

if (NOT CMAKE_BUILD_TYPE MATCHES "Debug")
    target_compile_definitions(imgui PUBLIC 
        IMGUI_DISABLE_DEMO_WINDOWS 
        IMGUI_DISABLE_DEBUG_TOOLS)
endif()

if (NOT EMSCRIPTEN)
    target_link_libraries(imgui PUBLIC ${TARGET_NAME_SDL2})
endif()

if (EMSCRIPTEN AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(imgui PUBLIC "-Wno-nontrivial-memaccess")
endif()

add_license("imgui ${FETCH_IMGUI_VERSION}"
    "${imgui_ext_SOURCE_DIR}/LICENSE.txt")

target_link_libraries(spaceinvaders PRIVATE imgui)

# ---------------------------- Install ----------------------------------

if (WIN32)
    install(TARGETS spaceinvaders
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})

    install(FILES 
        "$<TARGET_FILE:${TARGET_NAME_SDL2}>"
        "$<TARGET_FILE:${TARGET_NAME_SDL2_MIXER}>"
        "${CMAKE_SOURCE_DIR}/LICENSE.txt"
        "${EXT_LICENSES_BINFILE}"
        DESTINATION ${CMAKE_INSTALL_PREFIX})

    install(DIRECTORY ${CMAKE_BINARY_DIR}/assets
        DESTINATION ${CMAKE_INSTALL_PREFIX})

elseif (EMSCRIPTEN)
    install(FILES
        "$<TARGET_FILE_DIR:spaceinvaders>/spaceinvaders.js"
        "$<TARGET_FILE_DIR:spaceinvaders>/spaceinvaders.wasm"
        "${CMAKE_BINARY_DIR}/index.html"
        "${CMAKE_SOURCE_DIR}/LICENSE.txt"
        "${EXT_LICENSES_BINFILE}"
        DESTINATION "${CMAKE_INSTALL_PREFIX}")

    install(DIRECTORY ${CMAKE_BINARY_DIR}/assets
        DESTINATION ${CMAKE_INSTALL_PREFIX})

else()
    include(GNUInstallDirs)

    if (FETCHED_SDL2_SRC)
        set(PRIV_LIBDIR ${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME}/lib)
        set_target_properties(spaceinvaders PROPERTIES INSTALL_RPATH ${PRIV_LIBDIR})

        install(FILES 
            "$<TARGET_FILE:${TARGET_NAME_SDL2}>"
            "$<TARGET_FILE:${TARGET_NAME_SDL2_MIXER}>"
            DESTINATION ${PRIV_LIBDIR})
    endif()

    install(TARGETS spaceinvaders 
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    install(FILES 
        ${CMAKE_SOURCE_DIR}/LICENSE 
        ${EXT_LICENSES_BINFILE}
        DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()