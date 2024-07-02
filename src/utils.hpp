
#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstdio>
#include <filesystem>
#include <chrono>
#include <memory>

namespace fs = std::filesystem;
// can't name this time unfortunately
namespace tim = std::chrono;

using uint = unsigned int;

#define CONCAT(x, y) x##y


// Print error message and return error code.
template <typename ...Args>
int ecERROR(int ec, const char* fmt, Args... args)
{
#ifdef __clang__
_Pragma("clang diagnostic push")
_Pragma("clang diagnostic ignored \"-Wformat-security\"")
#endif
    std::fputs("\033[1;31mError:\033[0m ", stderr);
    std::fprintf(stderr, fmt, args...);
    std::fputs("\n", stderr);
    return ec;
#ifdef __clang__
_Pragma("clang diagnostic pop")
#endif
}

template <typename ...Args>
int mERROR(const char* fmt, Args... args)
{
    return ecERROR(-1, fmt, args...);
}

template <typename ...Args>
void pWARNING(const char* fmt, Args... args)
{
#ifdef __clang__
    _Pragma("clang diagnostic push")
        _Pragma("clang diagnostic ignored \"-Wformat-security\"")
#endif
    std::fputs("\033[1;33mWarning:\033[0m ", stderr);
    std::fprintf(stderr, fmt, args...);
    std::fputs("\n", stderr);
#ifdef __clang__
    _Pragma("clang diagnostic pop")
#endif
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
#define DECL_UTF8PATH_STR(path) \
auto CONCAT(path, _stdstr) = path.string(); \
const char* CONCAT(path, _str) = CONCAT(path, _stdstr).c_str();
#else
#define DECL_UTF8PATH_STR(path) \
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