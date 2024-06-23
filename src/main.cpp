


#include <cstdio>
#include <SDL.h>
#include <SDL_main.h>


#include "emu.hpp"
#include "utils.hpp"







int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::printf("Usage: spaceinvaders <rom-path>\n");
        return -1;
    }

    emulator emu(argv[1]);
    if (!emu.ok()) {
        return -1;
    }

    emu.run();

    //Get window surface
    //screenSurface = SDL_GetWindowSurface(window);
    //
    ////Fill the surface white
    //SDL_FillRect(screenSurface, NULL, SDL_MapRGB(screenSurface->format, 0xFF, 0xFF, 0xFF));

    //Update the surface
    //SDL_UpdateWindowSurface(window);

    // main loop

    // NEED TO DISASSEMBLE THE ROM TO FIGURE OUT ALL THE TIMING STUFF!! (HOW OFTEN/WHEN INPUTS ARE SAMPLED ETC ETC)

    
    
    

    return 0;
}