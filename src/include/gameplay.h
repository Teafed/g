// gameplay.h

#pragma once
#include "def.h"
#include <stdbool.h>

typedef struct InputDevice InputDevice;
typedef struct Body Body; // from phys.h

typedef struct {
   char* id;
   InputDevice* device;
} Soul;

typedef struct {
   float creation_time;
   Soul soul;
   Body* body;
} Spirit;

// soul
Soul soul_create(const char* id, InputDevice* device);

// spirit
Spirit spirit_create(float creation_time, Soul soul, Body* body);
