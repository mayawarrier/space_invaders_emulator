
#include <cstdlib>
#include <exception>

#include <SDL_main.h>
#include <cxxopts.hpp>

#include "emu.hpp"

#ifdef _WIN32
#include "win32.hpp"
#endif

static int do_main(int argc, char* argv[])
{
    cxxopts::Options opts("spaceinvaders", "1978 Space Invaders arcade cabinet emulator.");
    opts.add_options()
        ("h,help", "Show usage.")
#ifndef __EMSCRIPTEN__
        ("i,inifile", "Path to config file.",
            cxxopts::value<std::string>()->default_value("./spaceinvaders.ini"), "<file>")
        ("r,romdir", "Path to directory containing ROM/audio files.",
            cxxopts::value<std::string>()->default_value("./data"), "<dir>")
        ("w,windowed", "Launch in windowed mode.")
#endif
        ("no-ui", "Disable emulator UI (menu/settings/help panels etc.)");
        
    cxxopts::ParseResult args;
    try {
        args = opts.parse(argc, argv);
    }
    catch (std::exception& e) {
        logERROR(e.what());
        return -1;
    }

    if (args["help"].as<bool>()) 
    {
        std::printf("%s\n", opts.help().c_str());
        emu::print_ini_help();        
        return 0;
    }

#ifdef __EMSCRIPTEN__
    emu emu("/data", !args["no-ui"].as<bool>());
#else
    emu emu(
        args["inifile"].as<std::string>(),
        args["romdir"].as<std::string>(),
        args["windowed"].as<bool>(),
        !args["no-ui"].as<bool>()
    );
#endif
    if (!emu.ok()) {
        return -1;
    }

    // Start!
    return emu.run();
}

int main(int argc, char* argv[])
{
    int e = log_init();
    if (e != 0) { return e; }

#ifdef _WIN32
    bool pause_at_exit = win32_recreate_console();
#endif
    e = do_main(argc, argv);

#ifdef _WIN32
    if (pause_at_exit) {
        // allow user to read console before it quits
        std::system("pause");
    }
#endif
    return e;
}
