// gameplay.c
#include "gameplay.h"

Soul soul_create(const char *id, InputDevice *device)
   { Soul s = { id, device }; return s; }

Spirit spirit_create(float creation_time, Soul soul, Body* body)
   { Spirit s = { creation_time, soul, body }; return s; }