// bridge the space-spirit divide

#ifndef PHYS_H
#define PHYS_H

#include "def.h"
#include "vec.h"
#include "shape.h"
#include <stdbool.h>

typedef struct Affector Affector;

// align a body property with nearby bodies
// typedef Harmony;

typedef enum BodyFlags : uint8_t {
   BODY_FLAG_POSSESSABLE = 1u << 0,
   BODY_FLAG_POSSESSED   = 1u << 1,
   BODY_FLAG_GROUNDED    = 1u << 2
} BodyFlags;

// physical manifestation
typedef struct {
   fvec3 vel;              // velocity
   fvec3 pos;              // position
   float rot;              // rotation (radians)

   ui8 flags;

   Shape *shape;
   float mass;
   float inv_mass;
   fvec3 force_accum;

   Affector **affs;
   int aff_count;
} Body;

typedef struct {
   fvec3 gravity;   // e.g., (0,-9.8,0)
   float lin_damp;  // simple drag
} WorldParams;

// body
Body body_create(fvec3 pos, fvec3 vel, float rot, uint8_t flags);
void body_set_pos(Body* b, fvec3 pos);
void body_set_vel(Body* b, fvec3 vel);
void body_set_rot(Body* b, float rot);
static inline void body_flags_set(Body* b, uint8_t mask)   { b->flags |=  mask; }
static inline void body_flags_clear(Body* b, uint8_t mask) { b->flags &= ~mask; }
static inline bool body_has(const Body* b, uint8_t mask)   { return (b->flags & mask) != 0; }

// possession
bool body_possess(Body* b);     // returns true if possession succeeded
void body_unpossess(Body* b);
void body_set_grounded(Body* b, bool grounded);
bool body_is_moving(const Body* b, float epsilon); // |vel| > epsilon

// integration helper
void body_integrate(Body* b, float dt);

// world step
void step_bodies(Body **list, int count, WorldParams wp, float dt);

// 1) read inputs -> write soul intents
// 2) convert intents to impulses/affectors (per body)
// 3) apply world params (gravity, wind, fields)
// 4) integrate + solve contacts
// 5) update state machines (cooldowns, morphs)
// 6) render / debug print

#endif