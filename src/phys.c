#include "phys.h"
#include "vec.h"
#include <math.h>
#include <stdio.h>
#include <string.h> // memset

static inline float fvec3_len2(fvec3 v) {
   return VX(v)*VX(v) + VY(v)*VY(v) + VZ(v)*VZ(v);
}
static inline fvec3 fvec3_add(fvec3 a, fvec3 b) {
   fvec3 r = { VX(a)+VX(b), VY(a)+VY(b), VZ(a)+VZ(b) };
   return r;
}
static inline fvec3 fvec3_scale(fvec3 a, float s) {
   fvec3 r = { VX(a)*s, VY(a)*s, VZ(a)*s };
   return r;
}

typedef struct { fvec3 target; float G; } AttractData;
static void attract_force(Affector* a, Body *b, float dt) {
   (void)dt;
   AttractData *ad = (AttractData*)a->userdata;
   fvec3 d = (fvec3){ ad->target.x - b->pos.x,
                      ad->target.y - b->pos.y,
                      ad->target.z - b->pos.z };
   // bodyx_add_force(bx, scale(d, ad->G));
}

Body body_create(fvec3 pos, fvec3 vel, float rot, uint8_t flags) {
   Body b;
   b.pos = pos;
   b.vel = vel;
   b.rot = rot;
   b.flags = flags;

   if ((flags & BODY_FLAG_POSSESSED) && !(flags & BODY_FLAG_POSSESSABLE)) {
      b.flags &= (uint8_t)~BODY_FLAG_POSSESSED;
   }
   return b;
}

void body_set_pos(Body *b, fvec3 pos) { if (b) b->pos = pos; }
void body_set_vel(Body *b, fvec3 vel) { if (b) b->vel = vel; }
void body_set_rot(Body *b, float rot) { if (b) b->rot = rot; }

bool body_possess(Body *b) {
   if (!b) return false;
   if (!body_has(b, BODY_FLAG_POSSESSABLE)) return false;
   body_flags_set(b, BODY_FLAG_POSSESSED);
   return true;
}
void body_unpossess(Body *b) {
   if (!b) return;
   body_flags_clear(b, BODY_FLAG_POSSESSED);
}

void body_set_grounded(Body *b, bool grounded) {
   if (!b) return;
   if (grounded) body_flags_set(b, BODY_FLAG_GROUNDED);
   else body_flags_clear(b, BODY_FLAG_GROUNDED);
}

bool body_is_moving(const Body *b, float epsilon) {
   if (!b) return false;
   float e2 = epsilon > 0.f ? epsilon*epsilon : 0.f;
   return fvec3_len2(b->vel) > e2;
}

void body_integrate(Body *b, float dt) {
   if (!b) return;
   // kinematic step: x += v * dt
   b->pos = fvec3_add(b->pos, fvec3_scale(b->vel, dt));
}

// DEBUG
static int body_to_string(const Body *b, char *buf, int buf_size) {
   if (!b) return 0;
   return snprintf(
        buf, (size_t)buf_size,
        "Body{pos=(%.3f,%.3f,%.3f), vel=(%.3f,%.3f,%.3f), rot=%.3f, flags=0x%02X%s%s%s}",
        (double)VX(b->pos), (double)VY(b->pos), (double)VZ(b->pos),
        (double)VX(b->vel), (double)VY(b->vel), (double)VZ(b->vel),
        (double)b->rot,
        b->flags,
        body_has(b, BODY_FLAG_POSSESSABLE) ? " possessable" : "",
        body_has(b, BODY_FLAG_POSSESSED)   ? " possessed"   : "",
        body_has(b, BODY_FLAG_GROUNDED)    ? " grounded"    : ""
   );
}

// STEP
void step_bodies(Body **list, int count, WorldParams wp, float dt) {
   for (int i = 0; i < count; ++i) {
      Body *b = list[i];

      // clear forces, add gravity
      b->force_accum = (fvec3){0,0,0};
      if (b->inv_mass > 0.f) {
         fvec3 gravity_force = fvec3_scale(wp.gravity, b->mass);
         b->force_accum = fvec3_add(b->force_accum, gravity_force);
      }

      // apply affectors
      for (int j = 0; j < b->aff_count; ++j) {
         Affector *a = b->affs[j];
         if (a->apply_force) a->apply_force(a, b, dt);
         if (a->tick_state)  a->tick_state(a, b, dt);
      }

      // drag
      b->vel = fvec3_scale(b->vel, 1.0f / (1.0f + wp.lin_damp * dt));

      // integrate v from F
      fvec3 acc = (b->inv_mass > 0.f)
         ? fvec3_scale(b->force_accum, b->inv_mass)
         : (fvec3){0,0,0};

      b->vel = fvec3_add(b->vel, fvec3_scale(acc, dt));

      // integrate x from v (semi-implicit)
      b->pos = fvec3_add(b->pos, fvec3_scale(b->vel, dt));
   }

   // TODO: collision detection / resolution
}

