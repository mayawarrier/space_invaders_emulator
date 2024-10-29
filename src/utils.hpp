
#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstdio>
#include <algorithm>
#include <filesystem>
#include <charconv>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <optional>
#include <initializer_list>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/val.h>
#endif

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


constexpr bool is_emscripten()
{
#ifdef __EMSCRIPTEN__
    return true;
#else
    return false;
#endif
}

int log_init();

// Raw log function.
void log_write(std::FILE* stream, 
    const char* msg, bool endline = false);

void logERROR(const char* fmt, ...);
void logWARNING(const char* fmt, ...);
void logMESSAGE(const char* fmt, ...);


using scopedFILE = std::unique_ptr<std::FILE, int(*)(std::FILE*)>;

#define SAFE_FOPENA(fname, mode) scopedFILE(std::fopen(fname, mode), std::fclose)
#if defined(_MSC_VER) || defined(__MINGW32__)
#define SAFE_FOPEN(fname, mode) scopedFILE(::_wfopen(fname, CONCAT(L, mode)), std::fclose)
#else
#define SAFE_FOPEN(fname, mode) SAFE_FOPENA(fname, mode)
#endif

// Read entire file.
bool read_file(const fs::path& path, std::unique_ptr<char[]>& filedata, size_t& filesize);


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

template <std::unsigned_integral T>
constexpr T saturating_addu(T lhs, T rhs)
{
    T res = lhs + rhs;
    if (res < lhs) {
        res = T(-1);
    }
    return res;
}

template <std::unsigned_integral T>
constexpr T saturating_subu(T lhs, T rhs)
{
    T res = lhs - rhs;
    if (res > lhs) {
        res = 0;
    }
    return res;
}

template <typename = void>
struct ws_lut {
    static constexpr uint8_t lut[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
};

template <typename T>
constexpr uint8_t ws_lut<T>::lut[];

// Fast whitespace check, C-locale
constexpr bool is_ws(char c) {
    return ws_lut<>::lut[static_cast<uint8_t>(c)];
}

template <typename T>
bool parse_num(std::string_view str, T& val)
{
    const char* beg = str.data();
    const char* end = str.data() + str.length();
    // skip whitespace
    while (beg != end && is_ws(*beg)) { beg++; }

    auto res = std::from_chars(beg, end, val);
    return res.ec == std::errc();
}

// sorta replacement for C++26 operator+(string, string_view) 
inline std::string concat_sv_to_str(
    std::initializer_list<std::string_view> list)
{
    std::string ret;
    for (auto elem : list) {
        ret += elem;
    }
    return ret;
}

#ifdef __EMSCRIPTEN__

struct inireader
{
    inireader() {
        m_storage = emscripten::val::global("localStorage");
    }

    const char* path_cstr() const { return "inireader"; }

    std::optional<std::string> get_value(std::string_view section, std::string_view key)
    {
        emscripten::val value = get_emcc_value(section, key);
        if (value.isNull()) {
            return {};
        }
        return value.as<std::string>();
    }

    template <typename T> requires std::is_arithmetic_v<T>
    std::optional<T> get_num(std::string_view section, std::string_view key)
    {
        T ret;
        emscripten::val value = get_emcc_value(section, key);
        if (value.isNull() || !parse_num(value.as<std::string>(), ret)) {
            return {};
        }
        return ret;
    }
private:
    emscripten::val get_emcc_value(std::string_view section, std::string_view key)
    {
        return m_storage.call<emscripten::val>("getItem", 
            concat_sv_to_str({ section, "_", key}));
    }
private:
    emscripten::val m_storage;
};

struct iniwriter
{
    iniwriter() {
        m_storage = emscripten::val::global("localStorage");
    }

    bool flush() { return true; }

    bool write_section(std::string_view name) { 
        m_section = name; 
        return true; 
    }

    bool write_keyvalue(std::string_view key, std::string_view value)
    {
        m_storage.call<void>("setItem",
            concat_sv_to_str({ m_section, "_", key }), std::string(value));
        return true;
    }
private:
    emscripten::val m_storage;
    std::string m_section;
};

#else

struct inireader
{
    inireader(const fs::path& path);

    bool ok() const { return m_ok; }

    const char* path_cstr() const { return m_pathstr.c_str(); }

    std::optional<std::string> get_value(std::string_view section, std::string_view key)
    {
        auto str = get_value_sv(section, key);
        if (str.data()) {
            return {};
        }
        return std::string(str);
    }

    template <typename T> requires std::is_arithmetic_v<T>
    std::optional<T> get_num(std::string_view section, std::string_view key)
    {
        T value;
        auto str = get_value_sv(section, key);
        if (!str.data() || !parse_num(str, value)) {
            return {};
        }
        return value;
    }
private:
    // not null-terminated!
    std::string_view get_value_sv(std::string_view section, std::string_view key);

private:
    using section_t = std::unordered_map<std::string_view, std::string_view>;
    using map_t = std::unordered_map<std::string_view, section_t>;

    map_t m_map;
    std::string m_pathstr;
    std::unique_ptr<char[]> m_filebuf;
    bool m_ok;
};

struct iniwriter
{
    iniwriter(const fs::path& path) :
        m_file(SAFE_FOPEN(path.c_str(), "wb")),
        m_pathstr(path.string()),
        m_ok(false)
    {
        if (!m_file) {
            logERROR("Could not open file %s", m_pathstr.c_str());
            return;
        }
        m_ok = true;
    }

    bool ok() const { return m_ok; }

    bool write_section(std::string_view name)
    {
        return write_str(concat_sv_to_str({ "[", name, "]\n" }));
    }

    bool write_keyvalue(std::string_view key, std::string_view value)
    {
        return write_str(concat_sv_to_str({ key, " = ", value, "\n" }));
    }

    bool flush() { return std::fflush(m_file.get()) == 0; }

private:
    bool write_str(const std::string& str)
    {
        if (std::fwrite(str.c_str(), 1, str.length(), m_file.get()) != str.length()) {
            logERROR("Could not write to file %s", m_pathstr.c_str());
            return false;
        }
        return true;
    }

private:
    scopedFILE m_file;
    std::string m_pathstr;
    bool m_ok;
};

#endif

#endif