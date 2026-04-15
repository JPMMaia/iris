#ifndef HLANG_BUILTIN
#define HLANG_BUILTIN

#include <stdint.h>

struct Array_slice_Int8{ int8_t* data; uint64_t size; };
struct Array_slice_Int16{ int16_t* data; uint64_t size; };
struct Array_slice_Int32{ int32_t* data; uint64_t size; };
struct Array_slice_Int64{ int64_t* data; uint64_t size; };
struct Array_slice_Uint8{ uint8_t* data; uint64_t size; };
struct Array_slice_Uint16{ uint16_t* data; uint64_t size; };
struct Array_slice_Uint32{ uint32_t* data; uint64_t size; };
struct Array_slice_Uint64{ uint64_t* data; uint64_t size; };

#endif
