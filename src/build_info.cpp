
// These includes force CMake to relink the executable when 
// any of the following change:
// 1) the build number,
// 2) embedded assets (emscripten only) 
// 3) pre-js (emscripten only)
// CMake is unable to detect these changes otherwise.

#if __has_include("build_num.h")
#include "build_num.h"
#endif

#if __has_include("emcc_relink.h")
#include "emcc_relink.h"
#endif

#define STR(a) #a
#define XSTR(a) STR(a)

#include <cstring>

const char* build_num()
{
#if defined(BUILD_NUM)
    const char* value = XSTR(BUILD_NUM);
    return std::strcmp(value, "0") == 0 ? nullptr : value;
#else
    return nullptr;
#endif
}