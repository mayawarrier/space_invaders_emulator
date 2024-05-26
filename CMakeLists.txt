cmake_minimum_required(VERSION 3.15)
project(spaceinvaders)

list(APPEND CMAKE_PREFIX_PATH "./libs/")

find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)
find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2main)

add_executable(spaceinvaders "src/main.cpp" 
                             "src/i8080.cpp" 
                             "src/i8080.hpp" 
                             "src/i8080_opcodes.h")
# decays if c++20 is not supported
set_property(TARGET spaceinvaders PROPERTY CXX_STANDARD 20)

# Required by Windows GUI applications, link this first
if(TARGET SDL2::SDL2main)
    target_link_libraries(spaceinvaders PRIVATE SDL2::SDL2main)
endif()

target_link_libraries(spaceinvaders PRIVATE SDL2::SDL2)

if (WIN32)
    add_custom_command(
        TARGET spaceinvaders POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different 
                                    "$<TARGET_FILE:SDL2::SDL2>" 
                                    "$<TARGET_FILE_DIR:spaceinvaders>"
        VERBATIM
    )
endif()