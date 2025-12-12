// affectors.h

#pragma once
#include "def.h"
#include <stdbool.h>

typedef struct Body Body; // from phys.h

typedef enum {
    AFF_FORCE_FIELD,     // F(x,v,t) → force
    AFF_CONSTRAINT,      // projects positions/velocities
    AFF_IMPULSE,         // one-shot kicks
    AFF_STATE_MACHINE    // can toggle flags / modes
} AffType;

typedef struct Affector {
    AffType type;
    void* userdata;

    void (*apply_force)(struct Affector*, Body* b, float dt);
    void (*apply_constraint)(struct Affector*, Body* b, float dt);
    void (*apply_impulse)(struct Affector*, Body* b);
    void (*tick_state)(struct Affector*, Body* b, float dt);
} Affector;
