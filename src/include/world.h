// world.h

#pragma once
#include "def.h"
#include <stdbool.h>

typedef enum {
   WORLD_DIM_2D = 2,
   WORLD_DIM_3D = 3
} WorldDim;

typedef struct {
   float epoch;   // creation time
   WorldDim dim;

   union {
      struct {
         fvec2 origin2;
         fvec2 bounds2;  // max x,y
      } dim2;
      struct {
         fvec3 origin3;
         fvec3 bounds3;  // max x,y,z
      } dim3;
   };
} World;

World world_create_2d(fvec2 origin, fvec2 bounds);
World world_create_3d(fvec3 origin, fvec3 bounds);

bool world_contains_point(const World* r, fvec3 p); // z ignored for 2D
