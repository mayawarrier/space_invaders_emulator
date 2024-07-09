
#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstdio>
#include <filesystem>
#include <chrono>
#include <memory>

namespace fs = std::filesystem;
namespace tim = std::chrono; // can't be 'time' unfortunately

using uint = unsigned int;

#define CONCAT(x, y) x##y

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

// Print red error message.
template <typename ...Args>
void pERROR(const char* fmt, Args... args)
{
PUSH_WARNINGS
IGNORE_WFORMAT_SECURITY
    std::fputs("\033[1;31mError:\033[0m ", stderr);
    std::fprintf(stderr, fmt, args...);
    std::fputs("\n", stderr);
POP_WARNINGS
}

// Print yellow warning message.
template <typename ...Args>
void pWARNING(const char* fmt, Args... args)
{
PUSH_WARNINGS
IGNORE_WFORMAT_SECURITY
    std::fputs("\033[1;33mWarning:\033[0m ", stderr);
    std::fprintf(stderr, fmt, args...);
    std::fputs("\n", stderr);
POP_WARNINGS
}

template <typename ...Args>
int mERROR(const char* fmt, Args... args)
{
    pERROR(fmt, args...);
    return -1;
}

using scopedFILE = std::unique_ptr<std::FILE, int(*)(std::FILE*)>;

#define SAFE_FOPENA(fname, mode) scopedFILE(std::fopen(fname, mode), std::fclose)
#ifdef _MSC_VER
#define SAFE_FOPEN(fname, mode) scopedFILE(::_wfopen(fname, CONCAT(L, mode)), std::fclose)
#else
#define SAFE_FOPEN(fname, mode) SAFE_FOPENA(fname, mode)
#endif

// Convert fs::path to a const char*.
#ifdef _MSC_VER
#define DECL_PATH_TO_BSTR(path) \
auto CONCAT(path, _stdstr) = path.string(); \
const char* CONCAT(path, _str) = CONCAT(path, _stdstr).c_str();
#else
#define DECL_PATH_TO_BSTR(path) \
const char* CONCAT(path, _str) = path.c_str();
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