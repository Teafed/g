// liren_internal.h
#ifndef LIREN_INTERNAL_H
#define LIREN_INTERNAL_H

#include "liren/liren.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// local short-name typedefs (kept inside liren so we don't depend on
// the host project's def.h)
typedef uint8_t  ui8;
typedef uint16_t ui16;
typedef uint32_t ui32;
typedef uint64_t ui64;
typedef int8_t   si8;
typedef int16_t  si16;
typedef int32_t  si32;
typedef int64_t  si64;

// constants - maybe decouple some of these later?
#define PALETTE_SIZE        36
#define PALETTE_TRANSPARENT 35
#define GAME_WIDTH_VGA      640
#define GAME_HEIGHT_VGA     480
#define GAME_WIDTH_FWVGA    854
#define GAME_HEIGHT_FWVGA   480
#define RESIZE_DELAY_MS     150

typedef enum {
   SYS_CURRENT_FPS = 0,
   SYS_AVG_FPS,
   SYS_LAYER_COUNT,
   SYS_MAX
} SystemData;

typedef struct {
   LR_LayerHandle handle;

   bool can_draw_outside_viewport;
   bool visible;
   ui8  opacity;   // 255 = fully opaque
   ui8  size;      // pixel size, 1 = 1:1, 2 = double, etc.

   // backend-specific data for this layer
   void* backend_layer;
} Layer;

// internal font: pixel buffer is liren-owned (copied on create)
typedef struct {
   LR_FontHandle handle;
   int width, height;
   int glyph_w, glyph_h;
   int ascii_start;
   int glyph_count;
   ui8* pixels;     // owned, palette indices
} Font;

// internal sprite: pixel buffer is liren-owned (copied on create)
typedef struct {
   LR_SpriteHandle handle;
   int width, height;
   int frame_w, frame_h;
   int frame_count;
   ui8* pixels;     // owned, palette indices
} Sprite;

// sentinel value passed to backend->blit_indexed when no tint should be applied.
// 0xFF is safe because PALETTE_SIZE is 36.
#define LIREN_TINT_NONE 0xFF

// vtable
typedef struct {
   // initialize backend_data and any platform resources (window, surfaces, etc)
   bool (*init)(LiRen* r, const LR_Config* cfg);

   // clean up backend resources
   void (*shutdown)(LiRen* r);

   // called when logical viewport / resize mode changes
   // or when window is resized by platform layer
   void (*resize)(LiRen* r, int window_w, int window_h);

   // frame lifecycle hooks
   void (*begin_frame)(LiRen* r);
   void (*present)(LiRen* r);

   // layer lifecycle
   bool (*create_layer)(LiRen* r, Layer* layer);
   void (*destroy_layer)(LiRen* r, Layer* layer);

   // drawing primitives
   void (*clear_layer)(LiRen* r, Layer* layer, ui8 color_index);
   void (*fill_rect)(LiRen* r, Layer* layer,
                     const LR_Rect* rect, ui8 color_index);

   // blit a sub-rect of an indexed pixel buffer onto a layer.
   //   src_pixels        - source buffer (palette indices)
   //   src_w, src_h      - source buffer dimensions
   //   src_x, src_y      - top-left of sub-rect in source
   //   src_rect_w, src_rect_h - size of sub-rect (and destination)
   //   dst_x, dst_y      - top-left destination on the layer
   //   tint_color_index  - if LIREN_TINT_NONE, blit as-is. otherwise, every
   //                       non-transparent source pixel becomes tint_color_index.
   //   transparent_index - palette index that should be skipped on copy.
   void (*blit_indexed)(LiRen* r, Layer* layer,
                        const ui8* src_pixels, int src_w, int src_h,
                        int src_x, int src_y,
                        int src_rect_w, int src_rect_h,
                        int dst_x, int dst_y,
                        ui8 tint_color_index, ui8 transparent_index);

   // composited framebuffer access for lr_get_framebuffer()
   void (*update_framebuffer_view)(LiRen* r, LR_Framebuffer* out);
} BackendOps;

// internal state
struct LiRen {
   bool initialized;

   // config / logical viewport
   float                scale_factor;
   LR_DisplayResolution display_res;
   LR_ResizeMode        resize_mode;
   LR_WindowMode        window_mode;

   int logical_width;
   int logical_height;

   // palette (ARGB, hardcoded rn, look into this)
   ui32 palette[PALETTE_SIZE];
   ui8 clear_color_index;
   ui8 transparent_color_index;

   // layers
   Layer*         layers;
   ui32           layer_count;
   ui32           layer_capacity;
   LR_LayerHandle next_layer_handle;

   // fonts
   Font*           fonts;
   ui32            font_count;
   ui32            font_capacity;
   LR_FontHandle   next_font_handle;

   // sprites
   Sprite*         sprites;
   ui32            sprite_count;
   ui32            sprite_capacity;
   LR_SpriteHandle next_sprite_handle;

   // system overlay layer
   LR_LayerHandle   system_layer_handle;
   bool             system_layer_flags[SYS_MAX];

   // backend interface and data
   const BackendOps* backend;
   void*             backend_data;

   // cached framebuffer
   LR_Framebuffer framebuffer;

   // logging callback
   void (*log_fn)(void* user, const char* msg);
   void* log_user_data;
};

// initialize hardcoded palette
void liren_init_palette(LiRen* r);

// find a layer by handle (used by core drawing functions)
Layer* liren_find_layer(LiRen* r, LR_LayerHandle handle);

// ensure layer capacity >= requested
// returns false on allocation failure
bool liren_ensure_layer_capacity(LiRen* r, ui32 needed);

// asset lookup helpers
Font*   liren_find_font(LiRen* r, LR_FontHandle handle);
Sprite* liren_find_sprite(LiRen* r, LR_SpriteHandle handle);

// destroy all loaded assets (called from lr_destroy)
void liren_destroy_all_assets(LiRen* r);

// logging helper
static inline void liren_log(LiRen* r, const char* msg) {
   if (r && r->log_fn) r->log_fn(r->log_user_data, msg);
}

#endif
