
#include "utils.hpp"
#include <cstdarg>
#include <SDL.h>

#ifdef _WIN32
#include "win32.hpp"

#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>

#elif defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))
#include <unistd.h>
    #if defined(_POSIX_VERSION) && _POSIX_VERSION >= 200112L
    #define HAS_POSIX_2001 1
    #endif
#endif


#ifdef HAS_POSIX_2001
static bool posix_has_term_colors()
{
    if (!isatty(STDOUT_FILENO) || !isatty(STDERR_FILENO))
        return false;

    const char* term = getenv("TERM");
    if (!term) {
        return false;
    } 

    const char* color_terms[] = {
        "xterm",
        "xterm-color",
        "xterm-256color",
        "screen",
        "linux"
    };
    for (int i = 0; i < SDL_arraysize(color_terms); ++i) {
        if (std::string_view(term).find(color_terms[i]) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}
#endif

#define LOGFILE_PATH "spaceinvaders.log"

static scopedFILE LOGFILE(nullptr, nullptr);
static bool LOG_COLOR_CONSOLE = false;

int log_init()
{
#ifndef __EMSCRIPTEN__
    LOGFILE = SAFE_FOPENA(LOGFILE_PATH, "w");
    if (!LOGFILE) {
        // can't log an error, show a message box
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, 
            "Error", "Could not open log file " LOGFILE_PATH, NULL);
        return -1;
    }
#endif

#ifdef _WIN32
    LOG_COLOR_CONSOLE = win32_enable_console_colors();
#elif defined(HAS_POSIX_2001)
    LOG_COLOR_CONSOLE = posix_has_term_colors();
#endif
    return 0;
}

std::FILE* logfile() { return LOGFILE.get(); }


static inline void do_log(std::FILE* stream, 
    const char* prefix, const char* fmt, std::va_list vlist)
{
PUSH_WARNINGS
IGNORE_WFORMAT_SECURITY
    if (prefix) {
        std::fputs(prefix, stream);
    }
    std::vfprintf(stream, fmt, vlist);
    std::fputs("\n", stream);
POP_WARNINGS
}

#ifdef __EMSCRIPTEN__
#define GEN_EMCC_LOG(flags, fmt)          \
do {                                      \
    char buf[256];                        \
                                          \
    std::va_list vlist;                   \
    va_start(vlist, fmt);                 \
    std::vsnprintf(buf, 256, fmt, vlist); \
    va_end(vlist);                        \
                                          \
    emscripten_log(flags, buf);           \
} while(0)
#endif

#define GEN_LOG(stream, fmt, prefix, prefix_color)  \
do {                                                \
    std::va_list vlist;                             \
                                                    \
    va_start(vlist, fmt);                           \
    do_log(LOGFILE.get(), prefix, fmt, vlist);      \
    va_end(vlist);                                  \
    std::fflush(LOGFILE.get());                     \
                                                    \
    va_start(vlist, fmt);                           \
    do_log(stream, LOG_COLOR_CONSOLE ?              \
        prefix_color : prefix, fmt, vlist);         \
    va_end(vlist);                                  \
} while(0)


void logERROR(const char* fmt, ...)
{
#ifdef __EMSCRIPTEN__
    GEN_EMCC_LOG(EM_LOG_ERROR, fmt);
#else
    GEN_LOG(stderr, fmt, "Error: ", "\033[1;31mError:\033[0m ");
#endif
}

void logWARNING(const char* fmt, ...)
{
#ifdef __EMSCRIPTEN__
    GEN_EMCC_LOG(EM_LOG_WARN, fmt);
#else
    GEN_LOG(stderr, fmt, "Warning: ", "\033[1;33mWarning:\033[0m ");
#endif
}

void logMESSAGE(const char* fmt, ...)
{
#ifdef __EMSCRIPTEN__
    GEN_EMCC_LOG(EM_LOG_CONSOLE, fmt);
#else
    GEN_LOG(stdout, fmt, nullptr, nullptr);
#endif
}

static std::string_view trim(std::string_view str)
{
    const char* beg = str.data();
    const char* end = str.data() + str.length();

    while (beg != end && is_ws(*beg)) { ++beg; }
    while (end != beg && is_ws(*(end - 1))) { --end; }
  
    return { beg, end };
}

template <typename TString>
static bool extract_str(std::string_view& src, TString& str, char delim)
{
    if (src.size() == 0) { return false; }

    size_t i = src.find(delim);
    size_t endpos = (i != src.npos) ? i : src.size();
    size_t nread = (i != src.npos) ? i + 1 : src.size(); // skip delim

    str = trim({ src.data(), endpos });
    src.remove_prefix(nread);
    return true;
}

template <typename TString>
static bool getline_v(std::string_view& src, TString& line)
{
    return extract_str(src, line, '\n');
}

void ini::set_value_internal(std::string_view section,
    std::string_view key, std::string_view value)
{
    auto sectitr = m_map.insert({ 
        std::string(section), 
        mapsection(m_map.size()) }).first;

    auto& sectentries = sectitr->second.entries;
    auto keyitr = sectentries.insert({ 
        std::string(key),
        mapsection::value(sectentries.size()) }).first;

    keyitr->second.valuestr = value;
}

int ini::read()
{
    auto filesize = size_t(fs::file_size(m_path));
    auto filebuf = std::make_unique<char[]>(filesize);
    {
        scopedFILE file = SAFE_FOPEN(m_path.c_str(), "rb");
        if (!file) {
            logERROR("Could not open file %s", path_str().c_str());
            return -1;
        }
        if (std::fread(filebuf.get(), 1, filesize, file.get()) != filesize) {
            logERROR("Could not read from file %s", path_str().c_str());
            return -1;
        }
        logMESSAGE("Read INI file");
    }

    int lineno = 1;
    std::string_view line, section;
    std::string_view filedata(filebuf.get(), filesize);

    while (getline_v(filedata, line))
    {
        if (line.empty()) {
            continue;
        }
        if (line.starts_with('[') && line.ends_with(']')) {
            section = line.substr(1, line.length() - 2);
        } 
        else {
            std::string_view key, value;
            if (!extract_str(line, key, '=') || key.empty() ||
                !extract_str(line, value, '\n') || value.empty()) {
                logERROR("%s: Invalid entry on line %d", path_str().c_str(), lineno);
                return -1;
            }
            set_value_internal(section, key, value);
        }            
        lineno++;
    }

    return 0;
}

int ini::write()
{
    struct vecsection
    {
        using kv = std::pair<std::string_view, std::string_view>;
        std::string_view name;
        std::vector<kv> entries;
    };
    std::vector<vecsection> data(m_map.size());

    for (const auto& [sectname, sect] : m_map)
    {
        auto& vecsect = data[sect.index];
        vecsect = { .name = sectname };
        vecsect.entries.resize(sect.entries.size());

        for (const auto& [key, value] : sect.entries) {
            vecsect.entries[value.index] = { key, value.valuestr };
        }
    }

    std::string out;
    for (int i = 0; i < data.size(); ++i)
    {
        auto& section = data[i].name;
        out += "["; out += section; out += "]\n";
    
        for (int j = 0; j < data[i].entries.size(); ++j)
        {
            auto& entry = data[i].entries[j];
            out += entry.first;
            out += " = ";
            out += entry.second;
            out += "\n";
        }
        out += "\n";
    }

    scopedFILE file = SAFE_FOPEN(m_path.c_str(), "wb");
    if (!file) {
        logERROR("Could not open file %s", path_str().c_str());
        return -1;
    }
    if (std::fwrite(out.c_str(), 1, out.length(), file.get()) != out.length()) {
        logERROR("Could not write to file %s", path_str().c_str());
        return -1;
    }

    return 0;
}
