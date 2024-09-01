
#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstdio>
#include <filesystem>
#include <chrono>
#include <memory>
#include <thread>

#define CONCAT(x, y) x##y
#define STR(a) #a
#define XSTR(a) STR(a)

#ifdef __clang__
#define PUSH_WARNINGS _Pragma("clang diagnostic push")
#define POP_WARNINGS  _Pragma("clang diagnostic pop")
#define IGNORE_WFORMAT_SECURITY _Pragma("clang diagnostic ignored \"-Wformat-security\"")

#else
#define PUSH_WARNINGS
#define POP_WARNINGS
#define IGNORE_WFORMAT_SECURITY
#endif

namespace fs = std::filesystem;
namespace tim = std::chrono;

using clk = tim::steady_clock; // 100ns resolution on Windows
using uint = unsigned int;


// todo: move logging to its own file
extern std::FILE* LOGFILE;
extern bool CONSOLE_HAS_COLORS;

#define ERROR_PREFIX_COLRD "\033[1;31mError:\033[0m "
#define ERROR_PREFIX "Error: "

#define WARNING_PREFIX_COLRD "\033[1;33mWarning:\033[0m "
#define WARNING_PREFIX "Warning: "


template <typename ...Args>
void fput_msg(std::FILE* stream, const char* prefix, const char* fmt, Args... args)
{
PUSH_WARNINGS
IGNORE_WFORMAT_SECURITY
    std::fputs(prefix, stream);
    std::fprintf(stream, fmt, args...);
    std::fputs("\n", stream);
POP_WARNINGS
}

template <typename ...Args>
int ERROR(const char* fmt, Args... args) 
{
    if (LOGFILE) {
        fput_msg(LOGFILE, ERROR_PREFIX, fmt, args...);
        std::fflush(LOGFILE);
    }
    fput_msg(stderr, CONSOLE_HAS_COLORS ? 
        ERROR_PREFIX_COLRD : ERROR_PREFIX, fmt, args...);    
    return -1;
}

template <typename ...Args>
void WARNING(const char* fmt, Args... args) 
{
    if (LOGFILE) {
        fput_msg(LOGFILE, WARNING_PREFIX, fmt, args...);
        std::fflush(LOGFILE);
    }
    fput_msg(stderr, CONSOLE_HAS_COLORS ?
        WARNING_PREFIX_COLRD : WARNING_PREFIX, fmt, args...);
}

template <typename ...Args>
void MESSAGE(const char* fmt, Args... args) 
{
PUSH_WARNINGS
IGNORE_WFORMAT_SECURITY
    if (LOGFILE) {
        std::fprintf(LOGFILE, fmt, args...);
        std::fputs("\n", LOGFILE);
        std::fflush(LOGFILE);
    }
    std::printf(fmt, args...);
    std::printf("\n");
POP_WARNINGS
}

using scopedFILE = std::unique_ptr<std::FILE, int(*)(std::FILE*)>;

#define SAFE_FOPENA(fname, mode) scopedFILE(std::fopen(fname, mode), std::fclose)
#if defined(_MSC_VER) || defined(__MINGW32__)
#define SAFE_FOPEN(fname, mode) scopedFILE(::_wfopen(fname, CONCAT(L, mode)), std::fclose)
#else
#define SAFE_FOPEN(fname, mode) SAFE_FOPENA(fname, mode)
#endif

// this has good codegen
template <typename T>
inline void set_bit(T* ptr, int bit, bool val)
{
    *ptr = (*ptr & ~(0x1 << bit)) | (val << bit);
}

template <typename T>
inline bool get_bit(T word, int bit)
{
    return (word & (0x1 << bit)) != 0;
}

// More accurate than this_thread::sleep_for(), especially for MINGW
template <typename T>
void sleep_for(T tsleep)
{
    static constexpr uint64_t us_per_s = 1000000;
    static constexpr uint32_t wake_interval_ms = 3;

    static const uint64_t perfctr_freq = SDL_GetPerformanceFrequency();

    if (perfctr_freq < us_per_s) [[unlikely]] {
        SDL_Delay(uint32_t(tim::round<tim::milliseconds>(tsleep).count()));
        return;
    }

    const auto tbeg = clk::now();
    const auto tend = tbeg + tsleep;

    auto tcur = tbeg;
    auto trem = tend - tbeg;
    while (tcur < tend)
    {
        if (trem > tim::milliseconds(wake_interval_ms)) {
            SDL_Delay(wake_interval_ms);
        }
        else {
            auto trem_us = tim::round<tim::microseconds>(trem);

            uint64_t cur_ctr = SDL_GetPerformanceCounter();
            uint64_t target_ctr = cur_ctr + trem_us.count() * perfctr_freq / us_per_s;

            while (cur_ctr < target_ctr) {
                cur_ctr = SDL_GetPerformanceCounter();
            }
        }
        tcur = clk::now();
        trem = tend - tcur;
    }
}

#endif