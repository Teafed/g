#ifndef LIREN_H
#define LIREN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct LiRen LiRen;
typedef uint32_t LR_LayerHandle;
typedef uint32_t LR_FontHandle;
typedef uint32_t LR_SpriteHandle;
#define LR_INVALID_LAYER  0
#define LR_INVALID_FONT   0
#define LR_INVALID_SPRITE 0

typedef struct {
   int x, y, w, h;
} LR_Rect;

typedef enum {
   LR_RES_VGA,          // 640x480
   LR_RES_FWVGA,        // 854x480
} LR_DisplayResolution;

typedef enum {
   LR_RESIZE_FIT,       // scale to fill window while maintaining aspect
   LR_RESIZE_FIXED,     // fixed-size viewport, letterbox inside window
} LR_ResizeMode;

typedef enum {
   LR_WINDOW_WINDOWED,
   LR_WINDOW_BORDERLESS,
   LR_WINDOW_FULLSCREEN,
} LR_WindowMode;

typedef enum {
    LR_FMT_UNKNOWN = 0,
    LR_FMT_ARGB8888 = 1,   // 32-bit - A is highest byte, B is lowest
} LR_PixelFormat;

typedef struct {
    int width, height;
    void* pixels;
    int pitch;
    LR_PixelFormat format;
} LR_Framebuffer;

// CONFIG
typedef struct {
   float scale_factor;
   LR_DisplayResolution resolution;
   LR_ResizeMode resize_mode;
   LR_WindowMode window_mode;

   // callbacks
   void (*log_fn)(void* user, const char* msg);
   void* log_user_data;
} LR_Config;

// lifecycle
LiRen* lr_create(const LR_Config* cfg);
void lr_destroy(LiRen* r);

// frame control
void lr_begin_frame(LiRen* r);
void lr_end_frame(LiRen* r);
void lr_set_clear_color(LiRen* r, uint8_t color_index);
void lr_clear(LiRen* r);

// layers
LR_LayerHandle lr_layer_create(LiRen* r, bool can_draw_outside);
void lr_layer_destroy(LiRen* r, LR_LayerHandle layer);
void lr_layer_set_visible(LiRen* r, LR_LayerHandle layer, bool visible);
void lr_layer_set_opacity(LiRen* r, LR_LayerHandle layer, uint8_t opacity);
void lr_layer_set_size(LiRen* r, LR_LayerHandle layer, uint8_t size);

// drawing
void lr_draw_rect(LiRen* r, LR_LayerHandle layer,
                  LR_Rect rect, uint8_t color_index);
void lr_draw_fill(LiRen* r, LR_LayerHandle layer,
                  uint8_t color_index);
void lr_draw_pixel(LiRen* r, LR_LayerHandle layer,
                   int x, int y, uint8_t color_index);

// fonts (bitmap, monochrome, fixed-grid)
//
// pixels are palette indices, row-major, top-left origin.
// any pixel equal to the renderer's transparent index is treated as transparent.
// the buffer is copied internally; the caller may free it after create.
typedef struct {
   int width, height;            // sheet dimensions in pixels
   int glyph_w, glyph_h;         // size of one glyph
   int ascii_start;              // ascii code of first glyph (e.g. 32 for space)
   int glyph_count;              // total glyphs in the sheet
   const uint8_t* pixels;        // sheet pixels (palette indices)
} LR_BitmapFontDesc;

LR_FontHandle lr_font_create_from_pixels(LiRen* r, const LR_BitmapFontDesc* desc);
void lr_font_destroy(LiRen* r, LR_FontHandle font);

// draws text with all non-transparent glyph pixels replaced by color_index.
void lr_draw_text(LiRen* r, LR_LayerHandle layer, LR_FontHandle font,
                  int x, int y, uint8_t color_index, const char* text);

// sprites (single-frame or animated, fixed-frame sheet)
//
// frames are arranged in row-major order in the sheet. frame_count = 1
// means a static sprite. caller computes frame_index per draw.
typedef struct {
   int width, height;            // sheet dimensions in pixels
   int frame_w, frame_h;         // size of one frame
   int frame_count;              // 1 for static; >1 for animations
   const uint8_t* pixels;        // sheet pixels (palette indices)
} LR_SpriteDesc;

LR_SpriteHandle lr_sprite_create_from_pixels(LiRen* r, const LR_SpriteDesc* desc);
void lr_sprite_destroy(LiRen* r, LR_SpriteHandle sprite);

// draws the given frame of the sprite. frame_index is clamped to [0, frame_count).
void lr_draw_sprite(LiRen* r, LR_LayerHandle layer, LR_SpriteHandle sprite,
                    int x, int y, int frame_index);

// utility
void lr_enable_system_overlay(LiRen* r);
void lr_get_viewport_size(LiRen* r, int* width, int* height);
bool lr_point_in_viewport(LiRen* r, int x, int y);

// LiRen get/set
LR_DisplayResolution lr_get_display_resolution(LiRen* r);
void lr_set_display_resolution(LiRen* r, LR_DisplayResolution res);
LR_ResizeMode lr_get_resize_mode(LiRen* r);
void lr_set_resize_mode(LiRen* r, LR_ResizeMode resize_mode);
LR_WindowMode lr_get_window_mode(LiRen* r);
void lr_set_window_mode(LiRen* r, LR_WindowMode window_mode);

// framebuffer - pointer is valid until lr_begin_frame or lr_destroy
const LR_Framebuffer* lr_get_framebuffer(LiRen* r);

// palette access. returns a pointer to liren's palette array (size = *out_size).
// each entry is 0xRRGGBBAA. lifetime is tied to LiRen*; do not modify or free.
// returns NULL if r is NULL.
const uint32_t* lr_get_palette(LiRen* r, int* out_size);
uint8_t lr_get_transparent_index(LiRen* r);

// window events
void lr_on_window_resized(LiRen* r, int w, int h);

#ifdef __cplusplus
}
#endif

#endif
