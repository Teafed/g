/* liren_backend_sdl.c
 * uses SDL2. creates window, 32-bit ARGB
 * TODO: draw outside surface, resize timer
 */

#include "liren_backend_sdl.h"
#include <stdlib.h>
typedef struct {
   SDL_Window* window;
   SDL_Surface* window_surface;
   SDL_Surface* composite_surface; // ARGB8888, logical size

   SDL_Rect unit_map;
   SDL_Rect window_map;

   bool resize_in_progress;
   ui32 resize_start_time;

   // palette in composite format for clears
   ui32 palette_mapped[PALETTE_SIZE];
} SDLBackendData;

static SDLBackendData* get_sdl(LiRen* r) {
   return (SDLBackendData*)r->backend_data;
}

static SDL_Surface *create_composite_surface(LiRen* r) {
   if (!r) return NULL;

   int w = r->logical_width;
   int h = r->logical_height;

   SDL_Surface* surface =
      SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);

   if (!surface) {
      liren_log(r, "[liren_sdl] failed to create composite surface");
      return NULL;
   }

   return surface;
}

static void map_palette_to_composite(LiRen* r, SDLBackendData* b) {
   if (!r || !b || !b->composite_surface) return;

   SDL_PixelFormat* fmt = b->composite_surface->format;
   for (int i = 0; i < PALETTE_SIZE; ++i) {
      ui32 c = r->palette[i];
      Uint8 r8 = (c >> 24) & 0xFF;
      Uint8 g8 = (c >> 16) & 0xFF;
      Uint8 b8 = (c >> 8)  & 0xFF;
      Uint8 a8 = (c >> 0)  & 0xFF;
      b->palette_mapped[i] = SDL_MapRGBA(fmt, r8, g8, b8, a8);
   }
}

static SDL_Surface* create_layer_surface(LiRen* r, bool can_draw_outside) {
   if (!r) return NULL;

   // can't draw outside surface for now
   int w = r->logical_width;
   int h = r->logical_height;

   SDL_Surface* surface = SDL_CreateRGBSurface(0, w, h, 8, 0, 0, 0, 0);
   if (!surface) {
      liren_log(r, "[liren_sdl] failed to create layer surface");
      return NULL;
   }

   SDL_Color colors[PALETTE_SIZE];
   for (int i = 0; i < PALETTE_SIZE; ++i) {
      ui32 c = r->palette[i];
      colors[i].r = (c >> 24) & 0xFF;
      colors[i].g = (c >> 16) & 0xFF;
      colors[i].b = (c >> 8)  & 0xFF;
      colors[i].a = 255;
   }

   SDL_SetPaletteColors(surface->format->palette, colors, 0, PALETTE_SIZE);
   SDL_SetColorKey(surface, SDL_TRUE, r->transparent_color_index);
   SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
   SDL_SetSurfaceAlphaMod(surface, 255);

   SDL_FillRect(surface, NULL, r->transparent_color_index);

   return surface;
}

// BACKEND OPS
static bool sdl_init(LiRen* r, const LR_Config* cfg) {
   (void)cfg; // no userdata?
   if (!r) return false;

   SDLBackendData* b = (SDLBackendData*)calloc(1, sizeof(SDLBackendData));
   if (!b) {
      liren_log(r, "[liren_sdl] out of memory for backend data");
      return false;
   }
   r->backend_data = b;

   int win_w = (int)(r->logical_width  * r->scale_factor);
   int win_h = (int)(r->logical_height * r->scale_factor);

   b->window = SDL_CreateWindow(
      "LiRen",
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      win_w,
      win_h,
      SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
   );

   if (!b->window) {
      liren_log(r, "[liren_sdl] failed to create window");
      free(b);
      r->backend_data = NULL;
      return false;
   }
   
   SDL_SetWindowMinimumSize(b->window, r->logical_width, r->logical_height);

   b->window_surface = SDL_GetWindowSurface(b->window);
   if (!b->window_surface) {
      liren_log(r, "liren_sdl: failed to get window surface");
      SDL_DestroyWindow(b->window);
      free(b);
      r->backend_data = NULL;
      return false;
   }

   b->composite_surface = create_composite_surface(r);
   if (!b->composite_surface) {
      SDL_DestroyWindow(b->window);
      free(b);
      r->backend_data = NULL;
      return false;
   }
   map_palette_to_composite(r, b);

   return true;
}


static void sdl_shutdown(LiRen* r) {
   if (!r) return;
   SDLBackendData* b = get_sdl(r);
   if (!b) return;

   if (b->composite_surface) {
      SDL_FreeSurface(b->composite_surface);
      b->composite_surface = NULL;
   }

   if (b->window) {
      SDL_DestroyWindow(b->window);
      b->window = NULL;
   }

   r->backend_data = NULL;
   free(b);
}

static void sdl_resize(LiRen* r, int window_w, int window_h) {
   (void)window_w;
   (void)window_h;
   if (!r) return;

   SDLBackendData* b = get_sdl(r);
   if (!b || !b->window) return;

   b->window_surface = SDL_GetWindowSurface(b->window);
   // scaled in present()
}

static void sdl_begin_frame(LiRen* r) {
   (void)r;
   // TODO: handle resize timing here?
}

static bool sdl_create_layer(LiRen* r, Layer* layer) {
   if (!r || !layer) return false;

   SDL_Surface* surface = create_layer_surface(r, layer->can_draw_outside_viewport);
   if (!surface) return false;

   layer->backend_layer = surface;
   return true;
}

static void sdl_destroy_layer(LiRen* r, Layer* layer) {
   (void)r;
   if (!layer) return;
   if (layer->backend_layer) {
      SDL_Surface* s = (SDL_Surface*)layer->backend_layer;
      SDL_FreeSurface(s);
      layer->backend_layer = NULL;
   }
}

static void sdl_clear_layer(LiRen* r, Layer* layer, ui8 color_index) {
   if (!r || !layer || !layer->backend_layer) return;

   SDL_Surface* surface = (SDL_Surface*)layer->backend_layer;
   SDL_FillRect(surface, NULL, color_index);
}

static void sdl_fill_rect(LiRen* r, Layer* layer,
                          const LR_Rect* rect, ui8 color_index) {
   if (!r || !layer || !layer->backend_layer) return;
   if (!rect) {
      sdl_clear_layer(r, layer, color_index);
      return;
   }

   SDL_Surface* surface = (SDL_Surface*)layer->backend_layer;

   SDL_Rect sdl_rect;
   sdl_rect.x = rect->x;
   sdl_rect.y = rect->y;
   sdl_rect.w = rect->w;
   sdl_rect.h = rect->h;

   SDL_FillRect(surface, &sdl_rect, color_index);
}

static void sdl_present(LiRen* r) {
   if (!r) return;

   SDLBackendData* b = get_sdl(r);
   if (!b || !b->window_surface || !b->composite_surface) return;

   // clear composite to clear_color_index
   ui32 clear = b->palette_mapped[r->clear_color_index];
   SDL_FillRect(b->composite_surface, NULL, clear);

   // composite all visible layers onto composite surface
   for (ui32 i = 0; i < r->layer_count; ++i) {
      Layer* layer = &r->layers[i];
      if (!layer->visible || !layer->backend_layer) continue;

      SDL_Surface* src = (SDL_Surface*)layer->backend_layer;
      SDL_SetSurfaceAlphaMod(src, layer->opacity);
      SDL_BlitSurface(src, NULL, b->composite_surface, NULL);
   }

   // scale composite to window surface and show
   SDL_BlitScaled(b->composite_surface, NULL, b->window_surface, NULL);
   SDL_UpdateWindowSurface(b->window);
}

static void sdl_update_framebuffer_view(LiRen* r, LR_Framebuffer* out) {
   if (!r || !out) return;
   SDLBackendData* b = get_sdl(r);
   if (!b || !b->composite_surface) return;

   out->width  = b->composite_surface->w;
   out->height = b->composite_surface->h;
   out->pixels = b->composite_surface->pixels;
   out->pitch  = b->composite_surface->pitch;
   out->format = LR_FMT_ARGB8888;
}

const BackendOps LIREN_BACKEND_SDL = {
   .init = sdl_init,
   .shutdown = sdl_shutdown,
   .resize = sdl_resize,
   .begin_frame = sdl_begin_frame,
   .present = sdl_present,
   .create_layer = sdl_create_layer,
   .destroy_layer = sdl_destroy_layer,
   .clear_layer = sdl_clear_layer,
   .fill_rect = sdl_fill_rect,
   .update_framebuffer_view = sdl_update_framebuffer_view
};
