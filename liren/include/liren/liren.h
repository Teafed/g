#ifndef LIREN_H
#define LIREN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct LiRen LiRen;
typedef uint32_t LR_LayerHandle;
#define LR_INVALID_LAYER 0

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

// window events
void lr_on_window_resized(LiRen* r, int w, int h);

#ifdef __cplusplus
}
#endif

#endif
