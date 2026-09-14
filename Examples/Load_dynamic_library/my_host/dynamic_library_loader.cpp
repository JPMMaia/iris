#include "dynamic_library_loader.h"

#if defined(_WIN32)

#include <windows.h>

extern "C" void* dynamic_library_open(char const* const path)
{
    return reinterpret_cast<void*>(LoadLibraryA(path));
}

extern "C" void* dynamic_library_symbol(void* const library, char const* const name)
{
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(library), name));
}

extern "C" void dynamic_library_close(void* const library)
{
    FreeLibrary(static_cast<HMODULE>(library));
}

#else

#include <dlfcn.h>

extern "C" void* dynamic_library_open(char const* const path)
{
    return dlopen(path, RTLD_NOW);
}

extern "C" void* dynamic_library_symbol(void* const library, char const* const name)
{
    return dlsym(library, name);
}

extern "C" void dynamic_library_close(void* const library)
{
    dlclose(library);
}

#endif
