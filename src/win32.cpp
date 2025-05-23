
#include <cstdio>
#include <cstdlib>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "utils.hpp"
#include "win32.hpp"


static std::string err_to_str(DWORD ecode) noexcept
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

static void log_lasterror(const char* fn_name) noexcept
{
    DWORD err = GetLastError();
    logERROR("%s(), error %lu: %s", 
        fn_name, err, err_to_str(err).c_str());
}

bool win32_recreate_console() noexcept
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

        // Reopen C IO streams so printf() etc work.
        // No need to reopen C++ IO streams, since on 
        // Windows they are implemented using C IO
        if (!std::freopen("CONIN$", "r", stdin) ||
            !std::freopen("CONOUT$", "w", stdout) ||
            !std::freopen("CONOUT$", "w", stderr))
        {
            logERROR("Failed to reopen C IO streams\n");
            return false;
        }
 
        return true;
    }
    else {
        // if failed to attach, either there is no console 
        // (GUI launch), or the program was compiled as a
        // console app (in which case there's nothing to do).
        return false;
    }
}

static bool set_color_mode(DWORD nstdhandle) noexcept
{
    HANDLE hnd = GetStdHandle(nstdhandle);
    if (hnd == INVALID_HANDLE_VALUE || hnd == NULL) {
        return false;
    }

    DWORD con_mode = 0;
    if (!GetConsoleMode(hnd, &con_mode)) {
        log_lasterror("GetConsoleMode");
        return false;
    }
    con_mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hnd, con_mode)) {
        log_lasterror("SetConsoleMode");
        return false;
    }
    return true;
}

bool win32_enable_console_colors() noexcept
{
    if (!set_color_mode(STD_OUTPUT_HANDLE)) {
        return false;
    }
    if (!set_color_mode(STD_ERROR_HANDLE)) {
        return false;
    }
    return true;
}

#ifdef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION

static HANDLE HIGHRES_TIMER = NULL;

void win32_sleep_ns(uint64_t ns) noexcept
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
