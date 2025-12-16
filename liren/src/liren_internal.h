// liren_internal.h
#ifndef LIREN_INTERNAL_H
#define LIREN_INTERNAL_H

#include "liren/liren.h"

// temporary - please decouple
#include "def.h"    // ui8, ui32, etc
#include "file.h"   // FontArray, SpriteArray
#include "timing.h" // timing_*
#include "debug.h"  // debug logging

#include <stdbool.h>
#include <stddef.h>

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

   // temporary! decouple later
   FontArray   font_array;
   SpriteArray sprite_array;
};

// initialize hardcoded palette
void liren_init_palette(LiRen* r);

// find a layer by handle (used by core drawing functions)
Layer* liren_find_layer(LiRen* r, LR_LayerHandle handle);

// ensure layer capacity >= requested
// returns false on allocation failure
bool liren_ensure_layer_capacity(LiRen* r, ui32 needed);

// logging helper
static inline void liren_log(LiRen* r, const char* msg) {
   if (r && r->log_fn) r->log_fn(r->log_user_data, msg);
}

#endif
