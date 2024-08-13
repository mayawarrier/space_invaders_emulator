
#ifndef WIN32_HPP
#define WIN32_HPP

#include <cstdio>
#include <string>

//
// If launched from a console, spawn a new console.
//
// Due to a Windows limitation, it is impossible to correctly print to 
// the console that launched this program (see https://stackoverflow.com/questions/493536),
// unless the program is compiled as a console application. But this will always 
// spawn a console at startup, even if launched normally (eg. by dbl-clicking),
// which is undesirable.
//
bool win32_recreate_console(std::FILE* logfile);

#endif