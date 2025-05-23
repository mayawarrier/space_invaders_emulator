
#ifndef WIN32_HPP
#define WIN32_HPP

#include <cstdint>

// If launched from a console, spawn a new console.
//
// Due to a Windows limitation, it is impossible for GUI apps to correctly
// print to the console that launched the app (see https://stackoverflow.com/questions/493536).
// Compiling as a console app will always spawn a console at startup, which is undesirable.
//
bool win32_recreate_console() noexcept;

// Try to enable console colors.
// (only supported on some versions of Windows 10 and later).
// Returns true if successful.
bool win32_enable_console_colors() noexcept;

// Sleep using a high resolution timer if possible, otherwise just Sleep().
// (Highres timer is only supported on Windows 10 1803 and later).
void win32_sleep_ns(uint64_t ns) noexcept;

#endif