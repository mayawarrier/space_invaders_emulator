
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

#include <imgui.h>
#include <SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define CONCAT(x, y) x##y
#define STR(a) #a
#define XSTR(a) STR(a)

#define NS_PER_MS 1000000
#define NS_PER_US 1000
#define US_PER_MS 1000
#define US_PER_S  1000000

#define LOGFILE_NAME "spaceinvaders.log"

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

// this only works if NDEBUG is defined in Release mode
// (default for CMake)
constexpr bool is_debug()
{
#ifdef NDEBUG
    return false;
#else
    return true;
#endif
}

int log_init();

void logERROR(const char* fmt, ...);
void logWARNING(const char* fmt, ...);
void logMESSAGE(const char* fmt, ...);


using file_ptr = std::unique_ptr<std::FILE, int(*)(std::FILE*)>;

#define SAFE_FOPENA(fname, mode) file_ptr(std::fopen(fname, mode), std::fclose)

#if defined(_MSC_VER) || defined(__MINGW32__)
#define SAFE_FOPEN(fname, mode) file_ptr(::_wfopen(fname, CONCAT(L, mode)), std::fclose)
#else
#define SAFE_FOPEN(fname, mode) SAFE_FOPENA(fname, mode)
#endif

using malloc_ptr_t = std::unique_ptr<void, void(*)(void*)>;

inline malloc_ptr_t make_malloc_ptr(void* ptr)
{
    return { ptr, std::free };
}

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

constexpr SDL_Point sdl_ptadd(SDL_Point a, SDL_Point b)
{
    return { a.x + b.x, a.y + b.y };
}
constexpr SDL_Point sdl_ptsub(SDL_Point a, SDL_Point b)
{
    return { a.x - b.x, a.y - b.y };
}

#ifdef __EMSCRIPTEN__
const char* emcc_result_name(EMSCRIPTEN_RESULT result);
#endif

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

struct color
{
    uint8_t r, g, b, a;

    color() = default;

    constexpr color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) :
        r(r), g(g), b(b), a(a)
    {}

    constexpr color brighter(uint8_t amt) const
    {
        uint8_t rb = saturating_addu(r, amt);
        uint8_t gb = saturating_addu(g, amt);
        uint8_t bb = saturating_addu(b, amt);
        return color(rb, gb, bb, 255);
    }

    constexpr color darker(uint8_t amt) const
    {
        uint8_t rd = saturating_subu(r, amt);
        uint8_t gd = saturating_subu(g, amt);
        uint8_t bd = saturating_subu(b, amt);
        return color(rd, gd, bd, 255);
    }

    constexpr color alpha(float alpha) const {
        return color(r, g, b, uint8_t(alpha * 255));
    }

    constexpr color from_imcolor(ImU32 col) {
        return color(
            (col >> IM_COL32_R_SHIFT) & 0xFF,
            (col >> IM_COL32_G_SHIFT) & 0xFF,
            (col >> IM_COL32_B_SHIFT) & 0xFF,
            (col >> IM_COL32_A_SHIFT) & 0xFF
        );
    }

    constexpr ImU32 to_imcolor() const {
        return IM_COL32(r, g, b, a);
    }
};

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
inline std::string concat_sv(
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
    const char* path_cstr() const { return "localStorage"; }

    std::optional<std::string> get_string(std::string_view section, std::string_view key)
    {
        auto value = get_value(section, key);
        if (!value) {
            return {};
        }
        return std::string((char*)value.get());
    }

    template <typename T> requires std::is_arithmetic_v<T>
    std::optional<T> get_num(std::string_view section, std::string_view key)
    {
        T ret;
        auto value = get_value(section, key);
        if (!value || !parse_num((char*)value.get(), ret)) {
            return {};
        }
        return ret;
    }

private:
    malloc_ptr_t get_value(std::string_view section, std::string_view key)
    {
        auto actualkey = concat_sv({ section, "_", key });

        void* ptr = EM_ASM_PTR({
            var ret = localStorage.getItem(UTF8ToString($0));
            if (ret == null) {
                return 0;
            }
            return stringToNewUTF8(ret);
        }, actualkey.c_str());

        return make_malloc_ptr(ptr);
    }
};

struct iniwriter
{
    bool write_section(std::string_view name) { 
        m_section = name; 
        return true; 
    }

    bool write_keyvalue(std::string_view key, std::string_view value)
    {
        auto actualkey = concat_sv({ m_section, "_", key });
        
        EM_ASM({
            var key = UTF8ToString($0);
            var value = UTF8ToString($1, $2);
            localStorage.setItem(key, value);
        }, actualkey.c_str(), value.data(), value.length());

        return true;
    }

    bool flush() { return true; }

private:
    std::string m_section;
};

#else

struct inireader
{
    inireader(const fs::path& path);

    bool ok() const { return m_ok; }

    const char* path_cstr() const { return m_pathstr.c_str(); }

    std::optional<std::string> get_string(std::string_view section, std::string_view key)
    {
        auto str = get_value_sv(section, key);
        if (!str.data()) {
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
    std::string_view get_value_sv(std::string_view section, std::string_view key)
    {
        auto sectitr = m_map.find(section);
        if (sectitr != m_map.end())
        {
            auto& entries = sectitr->second;
            auto valueitr = entries.find(key);
            if (valueitr != entries.end()) {
                return valueitr->second;
            }
        }
        return {};
    }

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
        return write_str(concat_sv({ "[", name, "]\n" }));
    }

    bool write_keyvalue(std::string_view key, std::string_view value)
    {
        return write_str(concat_sv({ key, " = ", value, "\n" }));
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
    file_ptr m_file;
    std::string m_pathstr;
    bool m_ok;
};

#endif

#endif