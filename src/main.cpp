
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


int do_main(int argc, char* argv[])
{
    cxxopts::Options opts("spaceinvaders", "1978 Space Invaders arcade cabinet emulator.");
    opts.add_options()
        ("h,help", "Show usage.")
        ("c,ctrl", "Show control scheme.")
        ("r,romdir", "Directory containing Space Invaders ROM and audio files.", 
            cxxopts::value<std::string>()->default_value("data/"), "<dir>")
        ("s,switches", "Set the cabinet's DIP switches (0=off, 1=on). See README for how this affects game behavior.", 
            cxxopts::value<std::string>()->default_value("00000000"), "<bits>");
             
    cxxopts::ParseResult args;
    try {
        args = opts.parse(argc, argv);
    }
    catch (std::exception& e) {
        return ERROR(e.what());
    }

    if (args["help"].as<bool>()) {
        std::printf("%s", opts.help().c_str());
        return 0;
    }
    else if (args["ctrl"].as<bool>()) 
    {
        std::printf("Player 1 Left: %s\n", SDL_GetScancodeName(KEY_P1_LEFT));
        std::printf("Player 1 Right: %s\n", SDL_GetScancodeName(KEY_P1_RIGHT));
        std::printf("Player 1 Fire: %s\n", SDL_GetScancodeName(KEY_P1_FIRE));
        std::printf("\n");
        std::printf("Player 2 Left: %s\n", SDL_GetScancodeName(KEY_P2_LEFT));
        std::printf("Player 2 Right: %s\n", SDL_GetScancodeName(KEY_P2_RIGHT));
        std::printf("Player 2 Fire: %s\n", SDL_GetScancodeName(KEY_P2_FIRE));
        std::printf("\n");
        std::printf("Coin Slot: %s\n", SDL_GetScancodeName(KEY_CREDIT));
        std::printf("1-Player Start: %s\n", SDL_GetScancodeName(KEY_1P_START));
        std::printf("2-Player Start: %s\n", SDL_GetScancodeName(KEY_2P_START));
        std::printf("\n");
        return 0;
    }

    fs::path rom_dir = args["romdir"].as<std::string>();

    emulator emu(rom_dir, DEFAULT_RES_SCALEFAC);
    if (!emu.ok()) {
        return -1;
    }

    const char sw_errmsg[] = "Invalid switch values.";

    auto& swstr = args["switches"].as<std::string>();
    if (swstr.size() == 8)
    {
        const char* begin = swstr.data();
        const char* end = swstr.data() + swstr.size();

        uint8_t sw;
        auto res = std::from_chars(begin, end, sw, 2);
        if (res.ec != std::errc() || res.ptr != end) {
            return ERROR(sw_errmsg);
        }
        emu.set_switches(sw);
    }
    else return ERROR(sw_errmsg);

    // Start!
    emu.run();

    return 0;
}

int main(int argc, char* argv[])
{
    scopedFILE logfile = SAFE_FOPENA(LOGFILE_PATH, "w");
    if (!logfile) 
    {
        const char err_msg[] = "Could not open log file " LOGFILE_PATH "";
        // show a message box, since at this point there is no logfile
        // (and on Windows no console either).
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", err_msg, NULL);
        return ERROR(err_msg);
    }

    LOGFILE = logfile.get();

#ifdef _WIN32
    bool new_con = win32_recreate_console(LOGFILE);
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
