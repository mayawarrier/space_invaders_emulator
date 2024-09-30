
#include <cstdlib>
#include <charconv>

#include <SDL_main.h>
#include <cxxopts.hpp>

#ifdef _WIN32
#include "win32.hpp"
#endif
#include "emu.hpp"


#define LOGFILE_PATH "spaceinvaders.log"

std::FILE* LOGFILE;
bool CONSOLE_HAS_COLORS = false;


int do_main(int argc, char* argv[])
{
    cxxopts::Options opts("spaceinvaders", "1978 Space Invaders arcade cabinet emulator.");
    opts.add_options()
        ("h,help", "Show usage.")
        ("ini", "Config file path.", cxxopts::value<std::string>()->default_value("spaceinvaders.ini"), "<file>");

    cxxopts::ParseResult args;
    try {
        args = opts.parse(argc, argv);
    }
    catch (std::exception& e) {
        return logERROR(e.what());
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
    scopedFILE logfile = SAFE_FOPENA(LOGFILE_PATH, "w");
    if (!logfile)
    {
        const char err_msg[] = "Could not open log file " LOGFILE_PATH;
        // show a message box, since at this point there is no logfile
        // (and on Windows no console either).
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", err_msg, NULL);
        return logERROR(err_msg);
    }

    LOGFILE = logfile.get();

#ifdef _WIN32
    bool new_con = win32_recreate_console();
    CONSOLE_HAS_COLORS = win32_enable_console_colors();
#endif
    int ret = do_main(argc, argv);

#ifdef _WIN32
    if (new_con) {
        // allow user to read console before it quits
        std::system("pause");
    }
#endif
    return ret;
}
