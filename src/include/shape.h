// shape.h
#pragma once
#include "def.h"

typedef enum {
    SHAPE_SPHERE,
    SHAPE_BOX,
    SHAPE_CAPSULE,
    SHAPE_CONVEX_HULL,
    SHAPE_MESH,
    SHAPE_SDF,       // user-provided function pointer
    SHAPE_COMPOUND   // array of child shapes with transforms
} ShapeType;

typedef struct Shape Shape;

typedef float (*SdfFn)(fvec3 p, const void *userdata);  // <0 inside, 0 surface, >0 outside

typedef struct {
    Shape* children;
    ui32 count;
    // TODO: local transforms parallel array, etc.
} ShapeCompound;

struct Shape {
    ShapeType type;
    union {
        struct { float r; } sphere;
        struct { fvec3 half_extents; } box;
        struct { float r; float half_h; } capsule;
        struct { fvec3 *pts; ui32 n; } hull;
        struct { ui32 *indices; fvec3 *verts; ui32 ni, nv; } mesh;
        struct { SdfFn fn; const void *userdata; } sdf;
        ShapeCompound compound;
    };
};
