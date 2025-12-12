#ifndef DEF_H
#define DEF_H

#define SAFE_FREE(p) do { free(p); (p) = NULL; } while(0)

#include <stdint.h>
typedef uint8_t   ui8;
typedef uint16_t  ui16;
typedef uint32_t  ui32;
typedef uint64_t  ui64;
typedef int8_t    si8;
typedef int16_t   si16;
typedef int32_t   si32;
typedef int64_t   si64;

typedef struct { ui32 x, y; }       uvec2;
typedef struct { si32 x, y; }       ivec2;
typedef struct { float x, y; }      fvec2;

typedef struct { ui32 x, y, z; }    uvec3;
typedef struct { si32 x, y, z; }    ivec3;
typedef struct { float x, y, z; }   fvec3;

#endif