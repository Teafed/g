#ifndef SCENES_SETTINGS_H
#define SCENES_SETTINGS_H

#include "scene.h"

// settings scene: fullscreen toggle, volume slider, resolution choice,
// BACK action (also bound to B). pushed by the main menu's SETTINGS option.
//
// settings state is currently scene-local; persisting to disk and wiring
// fullscreen/resolution into liren are TODO.
Scene* settings_scene_create(void);

#endif
