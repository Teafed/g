#ifndef LIREN_BACKEND_SDL_H
#define LIREN_BACKEND_SDL_H

#include "liren_internal.h"
#include <SDL2/SDL.h>

extern const BackendOps LIREN_BACKEND_SDL;

// translate SDL_Event into lr_* calls
void liren_sdl_handle_event(LiRen* r, const SDL_Event* e);

#endif
