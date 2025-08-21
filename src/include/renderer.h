#ifndef RENDERER_H
#define RENDERER_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#define Rect SDL_Rect
#define WINDOW_TITLE "teafeds cool game"

typedef uint32_t LayerHandle;
#define INVALID_LAYER 0
#define PALETTE_SIZE 36
#define PALETTE_TRANSPARENT 35

#define GAME_WIDTH_VGA 640
#define GAME_HEIGHT_VGA 480
#define GAME_WIDTH_FWVGA 854
#define GAME_HEIGHT_FWVGA 480

#define RESIZE_DELAY 150 // ms

typedef enum {
   RES_VGA,             // 640x480 (4:3)
   RES_FWVGA            // 854x480 (approximately 16:9)
} DisplayResolution;

typedef enum {
   RESIZE_FIT,          // game area scales to fit window
   RESIZE_FIXED,        // game area stays fixed size, centered
} ResizeMode;

typedef enum {
   DISPLAY_WINDOWED,
   DISPLAY_BORDERLESS,
   DISPLAY_FULLSCREEN
} DisplayMode;

typedef enum {
   SYS_CURRENT_FPS,  // -> timing_get_current_fps()
   SYS_AVG_FPS,      // -> timing_get_performance_info()
   SYS_LAYER_COUNT,  // -> g_renderer
   SYS_MAX
} SystemData;

typedef struct {
   LayerHandle handle;
   bool can_draw_outside_viewport;
   bool visible;
   uint8_t opacity;     // 255 = fully opaque
   uint8_t size;        // 2 = default
   SDL_Surface* surface;

   // TODO: other kind of float scaling based on perspective
   // TODO: group layers - layers can be part of multiple groups, and groups can have properties that affect all
} Layer;

// typedef struct {
   // 
// } LayerGroup;

#include "file.h"
typedef struct {
   bool initialized;
   SDL_Window* window;
   SDL_Surface* window_surface;     // window's surface for blitting
   SDL_Surface* composite_surface;  // truecolor surface, composite of all layers
   
   DisplayResolution display_resolution;
   float scale_factor;
   Rect unit_map;    // w,h based on DisplayResolution
                     // x,y for offset
   Rect window_map;  // w,h based on current window dims
                     // x,y for offset
   
   ResizeMode resize_mode;
   bool resize_in_progress;         // only renders frozen composite frame until resizing is done
   uint32_t resize_start_time;      // set if resize happens
   uint32_t resize_delay_ms;
      
   DisplayMode display_mode;
   int last_windowed_width, last_windowed_height; // (window coords) updated when fullscreening
   
   uint8_t clear_color_index;
   uint8_t transparent_color_index;
   
   Layer* layers;                   // 8-bit indexed surfaces to be compiled on composite_surface
   int layer_count;
   int layer_capacity;
   LayerHandle next_layer_handle;
   LayerHandle system_layer_handle;
   bool system_layer_data[SYS_MAX];

   FontArray font_array;
   SpriteArray sprite_array;
} RendererState;

// TODO: palette("red-ivwy") returns uint8_t 11
static const uint32_t palette[PALETTE_SIZE] = {
   // teaf-v5
   0xFEFEFDFF,  // 0  - mono-white
   0xAAAAAAFF,  // 1  - mono-lgrey
   0x777777FF,  // 2  - mono-grey
   0x494949FF,  // 3  - mono-dgrey
   0x1E1E1EFF,  // 4  - mono-black
   0xE8BE96FF,  // 5  - orange-cream
   0xEAAA6DFF,  // 6  - orange-teaf
   0xF08629FF,  // 7  - orange-normal
   0x9B5A3CFF,  // 8  - brown-light
   0x5B3523FF,  // 9  - brown-dark
   0xE8896DFF,  // 10 - red-piggy
   0xAD2E35FF,  // 11 - red-ivwy
   0x7E2023FF,  // 12 - red-normal
   0x4C1313FF,  // 13 - red-dark
   0xB3DBD8FF,  // 14 - teal-cream
   0x57DCD7FF,  // 15 - teal-frankie
   0x27917FFF,  // 16 - teal-ocean
   0x022D25FF,  // 17 - teal-dark
   0xA2CAD8FF,  // 18 - blue-light
   0x6494B0FF,  // 19 - blue-sky
   0x5D5D6BFF,  // 20 - blue-concrete
   0xF5ABB9FF,  // 21 - pink-cream
   0xD69699FF,  // 22 - pink-craige
   0xF46BA9FF,  // 23 - pink-deep
   0xCDACE0FF,  // 24 - purple-cream
   0x9B5AC3FF,  // 25 - purple-katja
   0x7041A3FF,  // 26 - purple-normal
   0x442882FF,  // 27 - purple-dark
   0xA6B18EFF,  // 28 - green-cloudy
   0x4E631EFF,  // 29 - green-jacket
   0x394816FF,  // 30 - green-dirty
   0x242D0EFF,  // 31 - green-dark
   0xF0FD93FF,  // 32 - yellow-light
   0xD9E074FF,  // 33 - yellow-moon
   0xC7E336FF,  // 34 - yellow-neon
   0xFFFFFFFF,  // transparent
};

// core functions
bool renderer_init(float scale_factor);
void renderer_cleanup(void);
void renderer_clear(void);
void renderer_present(void); // call every frame
void renderer_handle_window_event(SDL_Event* event); // call every frame

// config options
void renderer_set_display_resolution(DisplayResolution res);
void renderer_set_display_mode(DisplayMode mode);
void renderer_set_resize_mode(ResizeMode mode);
void renderer_set_clear_color(uint8_t color_index);

// layer management
/* defaults: size = 2, visible = true, opacity = 255 */
LayerHandle renderer_create_layer(bool can_draw_outside);
void renderer_destroy_layer(LayerHandle handle);
void renderer_set_layer_draw_outside(LayerHandle handle, bool can_draw);
void renderer_set_layer_visible(LayerHandle handle, bool visible);
void renderer_set_layer_opacity(LayerHandle handle, uint8_t opacity);
void renderer_set_layer_size(LayerHandle handle, uint8_t size);

// drawing functions
void renderer_blit_masked(LayerHandle handle, ImageData* source, Rect src_rect, int dest_x, int dest_y, uint8_t color_index);
void renderer_draw_pixel(LayerHandle handle, int x, int y, uint8_t color_index);
void renderer_draw_rect(LayerHandle handle, Rect rect, uint8_t color_index);
void renderer_draw_rect_raw(Rect rect, uint8_t color_index);
void renderer_draw_fill(LayerHandle handle, uint8_t color_index);
#include "file.h"
void renderer_draw_char(LayerHandle handle, FontType font_type, char c, int x, int y, uint8_t color_index);
void renderer_draw_string(LayerHandle handle, FontType font_type, const char* str, int x, int y, uint8_t color_index);

// system layer
void renderer_toggle_system_data(SystemData data, bool display);
void renderer_draw_system_quit(uint8_t duration_held);

// utility functions
SDL_Surface* renderer_get_layer_surface(LayerHandle handle);
void renderer_get_window_dims(int* width, int* height);
void renderer_get_dims(int* width, int* height);
void renderer_get_dims_full(int* width, int* height);
bool renderer_is_in_viewport(int x, int y);
const RendererState* renderer_get_debug_state(void);

#endif