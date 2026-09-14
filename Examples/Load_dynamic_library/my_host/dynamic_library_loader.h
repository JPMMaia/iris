#ifndef DYNAMIC_LIBRARY_LOADER
#define DYNAMIC_LIBRARY_LOADER

#ifdef __cplusplus
extern "C" {
#endif

void* dynamic_library_open(char const* path);
void* dynamic_library_symbol(void* library, char const* name);
void dynamic_library_close(void* library);

#ifdef __cplusplus
}
#endif

#endif
