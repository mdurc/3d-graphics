#ifndef _LIB_TYPES_H
#define _LIB_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h> // for size_t

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

typedef struct ivec2 { i32 x, y; }    iv2;
typedef struct ivec3 { i32 x, y, z; } iv3;
typedef struct ivec4 { i32 x, y, z, w; } iv4;
typedef struct fvec2 { f32 x, y; }    fv2;
typedef struct fvec3 { f32 x, y, z; } fv3;
typedef struct fvec4 { f32 x, y, z, w; } fv4;

#endif
