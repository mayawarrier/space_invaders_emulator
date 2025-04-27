
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

#if defined(HAS_POSIX_2001) && !defined(__EMSCRIPTEN__)
static bool posix_has_term_colors()
{
    if (!isatty(STDOUT_FILENO) || !isatty(STDERR_FILENO)) {
        return false;
    }
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

static file_ptr LOGFILE(nullptr, nullptr);
static bool LOG_COLOR_CONSOLE = false;

int log_init()
{
#ifndef __EMSCRIPTEN__
    LOGFILE = SAFE_FOPENA(LOGFILE_NAME, "w");
    if (!LOGFILE) {
        // can't log an error, show a message box
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
            "Error", "Could not create log file", NULL);
        return -1;
    }
#endif
#ifdef _WIN32
    LOG_COLOR_CONSOLE = win32_enable_console_colors();
#elif defined(HAS_POSIX_2001) && !defined(__EMSCRIPTEN__) 
    LOG_COLOR_CONSOLE = posix_has_term_colors();
#endif
    return 0;
}

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

#else
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
#endif

void logERROR(const char* fmt, ...)
{
#ifdef __EMSCRIPTEN__
    GEN_EMCC_LOG(EM_LOG_CONSOLE | EM_LOG_ERROR, fmt);
#else
    GEN_LOG(stderr, fmt, "Error: ", "\033[1;31mError:\033[0m ");
#endif
}

void logWARNING(const char* fmt, ...)
{
#ifdef __EMSCRIPTEN__
    GEN_EMCC_LOG(EM_LOG_CONSOLE | EM_LOG_WARN, fmt);
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

#ifdef __EMSCRIPTEN__
const char* emcc_result_name(EMSCRIPTEN_RESULT result)
{
    switch (result) {
    case EMSCRIPTEN_RESULT_SUCCESS:             return "EMSCRIPTEN_RESULT_SUCCESS";
    case EMSCRIPTEN_RESULT_DEFERRED:            return "EMSCRIPTEN_RESULT_DEFERRED";
    case EMSCRIPTEN_RESULT_NOT_SUPPORTED:       return "EMSCRIPTEN_RESULT_NOT_SUPPORTED";
    case EMSCRIPTEN_RESULT_FAILED_NOT_DEFERRED: return "EMSCRIPTEN_RESULT_FAILED_NOT_DEFERRED";
    case EMSCRIPTEN_RESULT_INVALID_TARGET:      return "EMSCRIPTEN_RESULT_INVALID_TARGET";
    case EMSCRIPTEN_RESULT_UNKNOWN_TARGET:      return "EMSCRIPTEN_RESULT_UNKNOWN_TARGET";
    case EMSCRIPTEN_RESULT_FAILED:              return "EMSCRIPTEN_RESULT_FAILED";
    case EMSCRIPTEN_RESULT_NO_DATA:             return "EMSCRIPTEN_RESULT_NO_DATA";
    default: return "Unknown result code";
    }
}
#endif

#ifndef __EMSCRIPTEN__
static std::string_view trim(std::string_view str)
{
    const char* beg = str.data();
    const char* end = str.data() + str.length();

    while (beg != end && is_ws(*beg)) { ++beg; }
    while (end != beg && is_ws(*(end - 1))) { --end; }

    return { beg, end };
}

static bool extract_str(std::string_view& src, std::string_view& str, char delim)
{
    if (src.size() == 0) { return false; }

    size_t i = src.find(delim);
    size_t endpos = (i != src.npos) ? i : src.size();
    size_t nread = (i != src.npos) ? i + 1 : src.size(); // skip delim

    str = trim({ src.data(), endpos });
    src.remove_prefix(nread);
    return true;
}

static bool getline_v(std::string_view& src, std::string_view& line)
{
    return extract_str(src, line, '\n');
}

inireader::inireader(const fs::path& path) :
    m_pathstr(path.string()), m_ok(false)
{
    file_ptr file = SAFE_FOPEN(path.c_str(), "rb");
    if (!file) {
        logERROR("Could not open file %s", path.string().c_str());
        return;
    }
    size_t filesize = size_t(fs::file_size(path));
    m_filebuf = std::make_unique<char[]>(filesize);

    if (std::fread(m_filebuf.get(), 1, filesize, file.get()) != filesize) {
        logERROR("Could not read from file %s", path.string().c_str());
        return;
    }

    int lineno = 1;
    std::string_view line, section;
    std::string_view filedata(m_filebuf.get(), filesize);

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
                logERROR("%s: Invalid entry on line %d", path_cstr(), lineno);
                return;
            }

            m_map[section][key] = value;
        }
        lineno++;
    }

    m_ok = true;
}
#endif
