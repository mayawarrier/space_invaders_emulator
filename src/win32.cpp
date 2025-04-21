
#include <cstdio>
#include <cstdlib>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "utils.hpp"
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

    if (ret.ends_with("\r\n")) {
        ret.erase(ret.size() - 2, 2);
    }
    else if (ret.ends_with("\n")) {
        ret.pop_back();
    }
    return ret;
}

static void log_lasterror(const char* fn_name)
{
    DWORD err = GetLastError();
    logERROR("%s(), error %u: %s", 
        fn_name, (unsigned)err, err_to_str(err).c_str());
}

bool win32_recreate_console()
{
    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        if (!FreeConsole()) {
            log_lasterror("FreeConsole");
            return false;
        }
        if (!AllocConsole()) {
            log_lasterror("AllocConsole");
            return false;
        }
        
        // Reopen C IO streams so printf() etc work
        if (!std::freopen("CONIN$", "r", stdin) ||
            !std::freopen("CONOUT$", "w", stdout) ||
            !std::freopen("CONOUT$", "w", stderr))
        {
            logERROR("Failed to reopen C IO streams\n");
            return false;
        }
        // No need to reopen C++ IO streams, since on 
        // Windows they are implemented using C IO
    
        logMESSAGE("Created new Win32 console\n");
        return true;
    }
    else {
        // if failed to attach, either there is no console 
        // (GUI launch), or the program was compiled as a
        // console app (in which case a usable console 
        // already exists)
        return false;
    }
}

bool win32_enable_console_colors()
{
    // sufficient for both stdout and stderr
    HANDLE hnd_out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hnd_out == INVALID_HANDLE_VALUE || hnd_out == NULL) {
        return false;
    }

    DWORD con_mode = 0;
    if (!GetConsoleMode(hnd_out, &con_mode)) {
        log_lasterror("GetConsoleMode");
        return false;
    }
    con_mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hnd_out, con_mode)) {
        log_lasterror("SetConsoleMode");
        return false;
    }

    return true;
}

#ifdef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION

static thread_local HANDLE HIGHRES_TIMER = NULL;

void win32_sleep_ns(uint64_t ns)
{
    if (!HIGHRES_TIMER) 
    {
        HIGHRES_TIMER = CreateWaitableTimerExW(NULL, NULL, 
            CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!HIGHRES_TIMER) {
            log_lasterror("CreateWaitableTimerExW");
            HIGHRES_TIMER = INVALID_HANDLE_VALUE; // do not try again
        }
    }

    if (HIGHRES_TIMER == INVALID_HANDLE_VALUE) {
        Sleep(DWORD(ns / NS_PER_MS));
    }
    else {
        LARGE_INTEGER tsleep;
        tsleep.QuadPart = -((LONGLONG)ns / 100);

        if (!SetWaitableTimerEx(HIGHRES_TIMER, &tsleep, 0, NULL, NULL, NULL, 0)) 
        {
            log_lasterror("SetWaitableTimerEx");
            CloseHandle(HIGHRES_TIMER);
            HIGHRES_TIMER = INVALID_HANDLE_VALUE; // do not try again

            Sleep(DWORD(ns / NS_PER_MS));
            return;
        }

        WaitForSingleObject(HIGHRES_TIMER, INFINITE);
    }
}

#else
void win32_sleep_ns(uint64_t ns) { Sleep(DWORD(ns / NS_PER_MS)); }
#endif
