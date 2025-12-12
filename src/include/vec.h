// vec.h

#pragma once
#include "def.h"

static inline float fvec3_len2(fvec3 v) {
    return v.x*v.x + v.y*v.y + v.z*v.z;
}

static inline fvec3 fvec3_add(fvec3 a, fvec3 b) {
    fvec3 r = { a.x + b.x, a.y + b.y, a.z + b.z };
    return r;
}

static inline fvec3 fvec3_sub(fvec3 a, fvec3 b) {
    fvec3 r = { a.x - b.x, a.y - b.y, a.z - b.z };
    return r;
}

static inline fvec3 fvec3_scale(fvec3 a, float s) {
    fvec3 r = { a.x * s, a.y * s, a.z * s };
    return r;
}
