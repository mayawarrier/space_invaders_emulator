
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
#include <map>

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


// todo: move to utils.cpp
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

template <typename = void>
struct wslut {
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
constexpr uint8_t wslut<T>::lut[];

// Fast whitespace check, C-locale
constexpr bool is_ws(char c) {
    return wslut<>::lut[static_cast<uint8_t>(c)];
}

template <typename T>
bool try_parse_num(std::string_view str, T& val)
{
    const char* beg = str.data();
    const char* end = str.data() + str.length();
    // skip whitespace
    while (beg != end && is_ws(*beg)) { beg++; }

    auto res = std::from_chars(beg, end, val);
    return res.ec == std::errc();
}


struct ini
{
    ini() = default;
    ini(const fs::path& path) : 
        m_path(path)
    {}

    int read();
    int write();

    const fs::path& path() const { return m_path; }
    std::string path_str() const { return m_path.string(); }

    std::string_view get_value(const char* section, const char* key)
    {
        auto sectitr = m_map.find(section);
        if (sectitr != m_map.end()) {
            auto valueitr = sectitr->second.find(key);
            if (valueitr != sectitr->second.end()) {
                return valueitr->second;
            }
        }
        return {};
    }

    std::string_view get_value_or_dflt(
        const char* section, const char* key, std::string_view dflt_value)
    {
        auto ret = get_value(section, key);
        return ret.data() ? ret : dflt_value;
    }

    template <typename T, 
        typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    bool get_num_or_dflt(
        const char* section, const char* key, T dflt_value, T& value)
    {
        auto str = get_value(section, key);
        if (!str.data()) {
            value = dflt_value;
            return true;
        }
        else if (try_parse_num(str, value)) {
            return true;
        }
        else return false;
    }

    void set_value(const char* section,
        const char* key, std::string_view value)
    {
        set_value_internal(section, key, value);
    }

private:
    struct hash {
        std::size_t operator()(const std::string& s) const noexcept {
            return std::hash<std::string_view>{}({ s.begin() + (s.find('_') + 1), s.end() });
        }
    };
    struct cmp {
        bool operator()(const std::string& a, const std::string& b) const {
            return a.compare(a.find('_') + 1, a.npos, b, b.find('_') + 1, b.npos) == 0;
        }
    };
    using section_t = std::unordered_map<std::string, std::string, hash, cmp>;
    using map_t = std::unordered_map<std::string, section_t, hash, cmp>;

    void set_value_internal(const std::string& section,
        const std::string& key, std::string_view value);

    map_t m_map;
    fs::path m_path;
};

#endif