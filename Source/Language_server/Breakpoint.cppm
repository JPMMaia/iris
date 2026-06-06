module;

#if defined(_WIN32)
#include <windows.h>
#include <intrin.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#include <csignal>
#include <sys/ptrace.h>
#endif

export module iris.language_server.breakpoint;

namespace iris::language_server
{
    export inline void trigger_breakpoint()
    {
    #if defined(_WIN32)
        __debugbreak();
    #elif defined(__unix__) || defined(__APPLE__)
        raise(SIGTRAP);
    #endif
    }
}
