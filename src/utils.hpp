
#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstdio>
#include <filesystem>
#include <chrono>
#include <memory>

#define CONCAT(x, y) x##y

namespace fs = std::filesystem;
namespace tim = std::chrono; // can't be 'time' unfortunately

using uint = unsigned int;

#ifdef __clang__
#define PUSH_WARNINGS \
_Pragma("clang diagnostic push")
#define IGNORE_WFORMAT_SECURITY \
_Pragma("clang diagnostic ignored \"-Wformat-security\"")
#define POP_WARNINGS \
_Pragma("clang diagnostic pop")
#else
#define PUSH_WARNINGS
#define IGNORE_WFORMAT_SECURITY
#define POP_WARNINGS
#endif

extern std::FILE* LOGFILE;

// Print red error message.
template <typename ...Args>
void file_PERROR(std::FILE* stream, const char* fmt, Args... args)
{
PUSH_WARNINGS
IGNORE_WFORMAT_SECURITY
    std::fputs("\033[1;31mError:\033[0m ", stream);
    std::fprintf(stream, fmt, args...);
    std::fputs("\n", stream);
POP_WARNINGS
}

template <typename ...Args>
int ERROR(const char* fmt, Args... args) 
{
    if (LOGFILE) {
        file_PERROR(LOGFILE, fmt, args...);
        std::fflush(LOGFILE);
    }
    file_PERROR(stderr, fmt, args...);
    return -1;
}

// Print yellow warning message.
template <typename ...Args>
void file_PWARNING(std::FILE* stream, const char* fmt, Args... args)
{
PUSH_WARNINGS
IGNORE_WFORMAT_SECURITY
    std::fputs("\033[1;33mWarning:\033[0m ", stream);
    std::fprintf(stream, fmt, args...);
    std::fputs("\n", stream);
POP_WARNINGS
}

template <typename ...Args>
void WARNING(const char* fmt, Args... args) 
{
    if (LOGFILE) {
        file_PWARNING(LOGFILE, fmt, args...);
        std::fflush(LOGFILE);
    }
    file_PWARNING(stderr, fmt, args...);
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
#ifdef _MSC_VER
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