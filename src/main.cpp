
#include <cstdlib>
#ifndef DISABLE_EXCEPTIONS_AND_RTTI
#include <exception>
#endif

#include <SDL_main.h>

#ifndef __EMSCRIPTEN__
#ifdef DISABLE_EXCEPTIONS_AND_RTTI
    #define CXXOPTS_NO_RTTI 1
    #define CXXOPTS_NO_EXCEPTIONS 1
#endif
#include <cxxopts.hpp>
#endif

#ifdef _WIN32
#include "win32.hpp"
#endif

#include "emu.hpp"

#define BUG_REPORT_LINK "https://github.com/mayawarrier/space_invaders_emulator/issues/new"

static int do_main(int argc, char* argv[])
{
#ifdef __EMSCRIPTEN__
    emu emu("assets/");
#else
    cxxopts::Options opts("spaceinvaders", "1978 Space Invaders emulator.");
    opts.add_options()
        ("h,help", "Show this help message.")
        ("a,asset-dir", "Path to game assets (ROM/audio/fonts etc.)",
            cxxopts::value<std::string>()->default_value("assets/"), "<dir>")
        ("r,renderer", "Render backend to use. See SDL_HINT_RENDER_DRIVER. If not provided, "
            "will be determined automatically.", cxxopts::value<std::string>(), "<rend>")
        ("disable-menu", "Disable menu bar.");
        
    auto args = opts.parse(argc, argv);

    if (args["help"].as<bool>()) {
#ifdef _WIN32
        bool pause_console = win32_recreate_console();
#endif
        std::printf("%s\n", opts.help().c_str());

#ifdef _WIN32
        if (pause_console) {
            std::system("pause");
        }
#endif
        return 0;
    }

    emu emu(
        args["asset-dir"].as<std::string>(), 
        args["renderer"].count() == 0 ? "" : args["renderer"].as<std::string>(),
        !args["disable-menu"].as<bool>());
#endif

    if (!emu.ok()) {
        return -1;
    }
    // Start!
    return emu.run();
}

static int on_exit(int err, bool show_modal = true) noexcept
{
#ifdef __EMSCRIPTEN__
    // This triggers the error UI. onExit() doesn't seem to work
    if (err != 0) { std::abort(); }
#else
    if (err != 0 && show_modal) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", 
            "An unexpected error occurred.\n"
            "Please report this error at " BUG_REPORT_LINK ".\n"
            "Include the file '" LOGFILE_NAME "' in your report.\n", nullptr);
    }
#endif
    return err;
}

int main(int argc, char* argv[])
{
#ifndef DISABLE_EXCEPTIONS_AND_RTTI
    try {
#endif
        int err = log_init();
        if (err != 0) {
            return on_exit(err, false);
        }

        err = do_main(argc, argv);
        return on_exit(err);

#ifndef DISABLE_EXCEPTIONS_AND_RTTI
    } 
    catch (const std::exception& e) {
        logERROR("Exception: %s", e.what());
        return on_exit(-1);
    }
    catch (...) {
        logERROR("Unknown exception occurred");
        return on_exit(-1);
    }
#endif
}
