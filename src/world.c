// world.c
#include "world.h"

World world_create_2d(svec2 origin, svec2 bounds) {
   World r;
   r.dim = ROOM_DIM_2D;
   r.dim2.origin2 = origin;
   r.dim2.bounds2 = bounds;
   return r;
}

World world_create_3d(svec3 origin, svec3 bounds) {
   World r;
   r.dim = ROOM_DIM_3D;
   r.dim3.origin3 = origin;
   r.dim3.bounds3 = bounds;
   return r;
}

bool world_contains_point(const World* r, fvec3 p) {
   if (!r) return false;
   if (r->dim == ROOM_DIM_2D) {
      float x = VX(p), y = VY(p);
      float ox = VX(r->dim2.origin2), oy = VY(r->dim2.origin2);
      float bx = VX(r->dim2.bounds2), by = VY(r->dim2.bounds2);
      return (x >= ox && y >= oy && x <= bx && y <= by);
   } else {
      float x = VX(p), y = VY(p), z = VZ(p);
      float ox = VX(r->dim3.origin3), oy = VY(r->dim3.origin3), oz = VZ(r->dim3.origin3);
      float bx = VX(r->dim3.bounds3), by = VY(r->dim3.bounds3), bz = VZ(r->dim3.bounds3);
      return (x >= ox && y >= oy && z >= oz && x <= bx && y <= by && z <= bz);
   }
}

// DEBUG
static int world_to_string(const World* r, char* buf, int buf_size) {
   if (!r) return 0;
   if (r->dim == ROOM_DIM_2D) {
      return snprintf(
         buf, (size_t)buf_size,
         "World2D{origin=(%d,%d), bounds=(%d,%d)}",
         (int)VX(r->dim2.origin2), (int)VY(r->dim2.origin2),
         (int)VX(r->dim2.bounds2), (int)VY(r->dim2.bounds2)
      );
   } else {
      return snprintf(
         buf, (size_t)buf_size,
         "World3D{origin=(%d,%d,%d), bounds=(%d,%d,%d)}",
         (int)VX(r->dim3.origin3), (int)VY(r->dim3.origin3), (int)VZ(r->dim3.origin3),
         (int)VX(r->dim3.bounds3), (int)VY(r->dim3.bounds3), (int)VZ(r->dim3.bounds3)
      );
   }
}