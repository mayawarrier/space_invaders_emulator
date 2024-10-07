
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
    int e = log_init();
    if (e != 0) { return e; }

    cxxopts::Options opts("spaceinvaders", "1978 Space Invaders arcade cabinet emulator.");
    opts.add_options()
        ("h,help", "Show usage.")
        ("ini", "Config file path.", cxxopts::value<std::string>()->default_value("spaceinvaders.ini"), "<file>");

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

    emu emu(args["ini"].as<std::string>());
    if (!emu.ok()) {
        return -1;
    }

    // Start!
    emu.run();

    return 0;
}

int main(int argc, char* argv[])
{
#ifdef _WIN32
    bool pause_at_exit = win32_recreate_console();
#endif
    int e = do_main(argc, argv);
#ifdef _WIN32
    if (pause_at_exit) {
        // allow user to read console before it quits
        std::system("pause");
    }
#endif
    return e;
}

