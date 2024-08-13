
#include <cstdio>
#include <cstdlib>

#include <Windows.h>

#include "win32.hpp"


static std::string err_to_str(DWORD ecode)
{
    LPSTR msg = NULL;
    DWORD msgsize = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        ecode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&msg,
        0, NULL);

    std::string ret(msg, msgsize);
    LocalFree(msg);
    return ret;
}

static int fprint_lasterr(std::FILE* logfile, const char* fn_name)
{
    DWORD err = GetLastError();
    return std::fprintf(logfile, "%s(), error %u: %s\n", 
        fn_name, (unsigned)err, err_to_str(err).c_str());
}

bool win32_recreate_console(std::FILE* logfile)
{
    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        if (!FreeConsole()) {
            fprint_lasterr(logfile, "FreeConsole");
            return false;
        }
        if (!AllocConsole()) {
            fprint_lasterr(logfile, "AllocConsole");
            return false;
        }

        // Reopen C IO streams so printf() etc work
        if (!std::freopen("CONIN$", "r", stdin) ||
            !std::freopen("CONOUT$", "w", stdout) ||
            !std::freopen("CONERR$", "w", stderr)) 
        {
            std::fprintf(logfile, "Failed to reopen C IO streams\n");
            return false;
        }
        // No need to reopen C++ IO streams, since on 
        // Windows they are implemented using C IO

        std::fprintf(logfile, "Created new Win32 console\n");
        return true;
    }
    else {
        // if failed to attach, either there is no console 
        // (GUI launch), or the program was compiled as a
        // console app (in which case we don't need a new console)
        return false;
    }
}
