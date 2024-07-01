


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

    emulator emu(argv[1], 3);
    if (!emu.ok()) {
        return -1;
    }

    emu.run();

    return 0;
}