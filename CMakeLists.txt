project(spaceinvaders)


#option(SDL2_VENDORED "Use vendored SDL2" OFF)

#if(MYGAME_VENDORED)
#    add_subdirectory(vendored/sdl EXCLUDE_FROM_ALL)
#else()
#    # 1. Look for a SDL2 package, 2. look for the SDL2 component and 3. fail if none can be found
#    find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)
#    # 1. Look for a SDL2 package, 2. Look for the SDL2maincomponent and 3. DO NOT fail when SDL2main is not available
#    find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2main)
#endif()


add_executable(spaceinvaders "src/main.cpp" "src/i8080.cpp" "include/i8080.hpp" "include/i8080_opcodes.h")
target_include_directories(spaceinvaders PRIVATE include/)

# sets the highest available std <= C++20
#set_property(TARGET spaceinvaders PROPERTY CXX_STANDARD 20)
#
## SDL2::SDL2main may or may not be available. It is e.g. required by Windows GUI applications
#if(TARGET SDL2::SDL2main)
#    # It has an implicit dependency on SDL2 functions, so it MUST be added before SDL2::SDL2 (or SDL2::SDL2-static)
#    target_link_libraries(mygame PRIVATE SDL2::SDL2main)
#endif()
#
## Link to the actual SDL2 library. SDL2::SDL2 is the shared SDL library, SDL2::SDL2-static is the static SDL libarary.
#target_link_libraries(mygame PRIVATE SDL2::SDL2)