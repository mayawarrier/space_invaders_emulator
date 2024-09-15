
#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstdio>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <memory>

#define CONCAT(x, y) x##y
#define STR(a) #a
#define XSTR(a) STR(a)

#define NS_PER_MS 1000000
#define NS_PER_US 1000
#define US_PER_MS 1000
#define US_PER_S  1000000

#ifdef __clang__
#define PUSH_WARNINGS _Pragma("clang diagnostic push")
#define POP_WARNINGS  _Pragma("clang diagnostic pop")
#define IGNORE_WFORMAT_SECURITY \
_Pragma("clang diagnostic ignored \"-Wformat-security\"")

#elif defined(__GNUC__)
#define PUSH_WARNINGS _Pragma("GCC diagnostic push")
#define POP_WARNINGS  _Pragma("GCC diagnostic pop")
#define IGNORE_WFORMAT_SECURITY \
_Pragma("GCC diagnostic ignored \"-Wformat-security\"")

#else
#define PUSH_WARNINGS
#define POP_WARNINGS
#define IGNORE_WFORMAT_SECURITY
#endif


namespace fs = std::filesystem;
namespace tim = std::chrono;

using clk = tim::steady_clock;
using uint = unsigned int;


// todo: move logging to its own file?
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
int logERROR(const char* fmt, Args... args) 
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
void logWARNING(const char* fmt, Args... args) 
{
    if (LOGFILE) {
        fput_msg(LOGFILE, WARNING_PREFIX, fmt, args...);
        std::fflush(LOGFILE);
    }
    fput_msg(stderr, CONSOLE_HAS_COLORS ?
        WARNING_PREFIX_COLRD : WARNING_PREFIX, fmt, args...);
}

template <typename ...Args>
void logMESSAGE(const char* fmt, Args... args) 
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

#endif