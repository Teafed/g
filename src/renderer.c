#include "renderer.h"
#include "timing.h"
#include "file.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>

static RendererState g_renderer = { 0 };

static void calculate_viewport(void);
static Layer* find_layer(LayerHandle handle);
SDL_Surface* create_surface(void);
void recreate_surface(SDL_Surface** surface);
void resize_all_surfaces(void);
static SDL_Color* get_palette_colors(void);
static void blit_masked_scaled(Layer* layer, ImageData* source, SDL_Rect src_rect,
                              SDL_Rect dest_screen_rect, uint8_t draw_color,
                              uint8_t size, int clip_left, int clip_top);
void d_print_renderer_dims(void); // TODO: MOVE!!!

// CORE FUNCTIONS
bool renderer_init(float scale_factor) {
   if (g_renderer.initialized) {
      d_err("the renderer is already initialized...");
      return false;
   }
   
   if (scale_factor < 1.0f || scale_factor > 100.0f) {
      d_err("scale factor must be >= 1.0... and not too big either");
      return false;
   }
   
   g_renderer.display_resolution = RES_VGA;
   g_renderer.game_coords = (SDL_Rect){ 0, 0, GAME_WIDTH_VGA, GAME_HEIGHT_VGA };
   
   g_renderer.screen = (SDL_Rect){ 0, 0, (int)(GAME_WIDTH_FWVGA * scale_factor), (int)(GAME_HEIGHT_FWVGA * scale_factor) };
   g_renderer.scale_factor = scale_factor;
   calculate_viewport();
   
   g_renderer.resize_mode = RESIZE_FIT;
   g_renderer.resize_in_progress = false;
   g_renderer.resize_start_time = timing_get_game_time_ms();
   g_renderer.resize_delay_ms = RESIZE_DELAY;
   
   g_renderer.display_mode = DISPLAY_WINDOWED;
   g_renderer.last_windowed_width = g_renderer.screen.w;
   g_renderer.last_windowed_height = g_renderer.screen.h;
   
   g_renderer.clear_color_index = 4; // mono-black
   g_renderer.transparent_color_index = PALETTE_TRANSPARENT;
   
   g_renderer.window = SDL_CreateWindow(
      "teafeds cool game",
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      g_renderer.screen.w,
      g_renderer.screen.h,
      SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
   );
   if (d_dne(g_renderer.window)) {
      renderer_cleanup();
      return false;
   }
   SDL_SetWindowMinimumSize(g_renderer.window, g_renderer.game_coords.w, g_renderer.game_coords.h);
   
   g_renderer.window_surface = SDL_GetWindowSurface(g_renderer.window);
   if (d_dne(g_renderer.window_surface)) {
      renderer_cleanup();
      return false;
   }

   g_renderer.composite_surface = SDL_CreateRGBSurfaceWithFormat(
                     0, g_renderer.screen.w, g_renderer.screen.h,
                     32, SDL_PIXELFORMAT_RGBA8888);
   if (d_dne(g_renderer.composite_surface)) {
      renderer_cleanup();
      return false;
   }

   g_renderer.layer_capacity = 16; // start with space for 16 layers
   g_renderer.layers = malloc(sizeof(Layer) * g_renderer.layer_capacity);
   if (d_dne(g_renderer.layers)) {
      renderer_cleanup();
      return false;
   }
   memset(g_renderer.layers, 0, sizeof(Layer) * g_renderer.layer_capacity);
   g_renderer.layer_count = 0;
   g_renderer.next_layer_handle = 1;
   
   if (file_load_sheets(&g_renderer.sprite_array, &g_renderer.font_array) == 0) {
      renderer_cleanup();
      return false;
   }
   
   g_renderer.initialized = true;
   d_logv(2, "screen = %s", d_name_rect(&g_renderer.screen));
   d_logv(2, "viewport = %s", d_name_rect(&g_renderer.viewport));
   
   // create bottom layer & system layer
   LayerHandle bg_layer = renderer_create_layer(true);
   if (bg_layer == INVALID_LAYER) {
      d_err("could not create background layer");
      renderer_cleanup();
      return false;
   }
   
   LayerHandle system_layer = renderer_create_layer(true);
   if (system_layer == INVALID_LAYER) {
      d_err("could not create background layer");
      renderer_cleanup();
      return false;
   }
   g_renderer.system_layer_handle = system_layer;
   renderer_set_layer_visible(g_renderer.system_layer_handle, false);
   renderer_set_layer_draw_outisde(g_renderer.system_layer_handle, true);
   
   return true;
}

void renderer_cleanup(void) {
   if (!g_renderer.initialized) return;

   // destroy layers
   if (g_renderer.layers) {
      while (g_renderer.layer_count > 0) {
         renderer_destroy_layer(g_renderer.layers[0].handle);
      }
      free(g_renderer.layers);
      g_renderer.layers = NULL;
      g_renderer.layer_capacity = 0;
   }

   // free surfaces
   if (g_renderer.composite_surface) {
      SDL_FreeSurface(g_renderer.composite_surface);
      g_renderer.composite_surface = NULL;
   }

   // destory the window
   if (g_renderer.window) {
      SDL_DestroyWindow(g_renderer.window);
      g_renderer.window = NULL;
   }
   
   memset(&g_renderer, 0, sizeof(RendererState));
   SDL_Log("I cleaned up the renderer");
}

void renderer_clear(void) {
   if (!g_renderer.initialized) return;
   SDL_FillRect(g_renderer.composite_surface, NULL, palette[g_renderer.clear_color_index]); // clear composite
   renderer_draw_fill(0, g_renderer.clear_color_index);
   for (int i = 1; i < g_renderer.layer_count; i++) {
      Layer* layer = &g_renderer.layers[i];
      if (!layer) continue;
      renderer_draw_fill(i, g_renderer.transparent_color_index);
   }
}

extern void scene_render(void);
void renderer_present(void) {
   if (!g_renderer.initialized) return;
   
   if (g_renderer.resize_in_progress) {
      uint32_t current_time = timing_get_game_time_ms();
      uint32_t time_since_resize = current_time - g_renderer.resize_start_time;
      
      if (time_since_resize >= g_renderer.resize_delay_ms) {
         d_log("resized, scale factor = %f", g_renderer.scale_factor);
         resize_all_surfaces();
         g_renderer.resize_in_progress = false;
      } else {
         // TODO: the stretch doesn't actually show...
         // stretch existing composite for now
         SDL_Rect stretch_rect = {0, 0, g_renderer.screen.w, g_renderer.screen.h};
         SDL_BlitScaled(g_renderer.composite_surface, NULL, 
                        g_renderer.window_surface, &stretch_rect);
         SDL_UpdateWindowSurface(g_renderer.window);
         return;
      }
   }
   
   scene_render(); // get all rendering calls from current scene
   
   // composite all visible layers
   for (int i = 0; i < g_renderer.layer_count; i++) {
      Layer* layer = &g_renderer.layers[i];
      if (!layer->visible || layer->handle == g_renderer.system_layer_handle) continue;

      if (layer->can_draw_outside_viewport) {
         SDL_SetSurfaceAlphaMod(layer->surface, layer->opacity);
         SDL_BlitSurface(layer->surface, NULL, g_renderer.composite_surface, NULL);
      } else {
         SDL_SetSurfaceAlphaMod(layer->surface, layer->opacity);
         SDL_BlitSurface(layer->surface, &g_renderer.viewport, g_renderer.composite_surface,  &g_renderer.viewport);
      }
   }

   // draw system layer
   Layer* system_layer = find_layer(g_renderer.system_layer_handle);
   if (system_layer->visible) {
      SDL_SetSurfaceAlphaMod(system_layer->surface, system_layer->opacity);
      SDL_BlitSurface(system_layer->surface, NULL, g_renderer.composite_surface, NULL);
   }
   
   SDL_BlitSurface(g_renderer.composite_surface, NULL, g_renderer.window_surface, NULL);
   SDL_UpdateWindowSurface(g_renderer.window);
}

void renderer_handle_window_event(SDL_Event* event) {
   if (event->type != SDL_WINDOWEVENT) return;
   
   switch (event->window.event) {
   case SDL_WINDOWEVENT_RESIZED:
      g_renderer.screen.w = event->window.data1;
      g_renderer.screen.h = event->window.data2;
      break;
   case SDL_WINDOWEVENT_SIZE_CHANGED:
      if (!g_renderer.resize_in_progress) {
         g_renderer.resize_in_progress = true;
         g_renderer.resize_start_time = timing_get_game_time_ms();
      } else {
         // subsequent resize events - just update the start time
         g_renderer.resize_start_time = timing_get_game_time_ms();
      }
      calculate_viewport();
      break;

   case SDL_WINDOWEVENT_EXPOSED:
      // hello
      break;
      
   case SDL_WINDOWEVENT_MINIMIZED:
      // maybe pause rendering
      break;
   
   case SDL_WINDOWEVENT_RESTORED:
      // resume if i do render pausing
      break;
   }
}

// CONFIG OPTIONS
void renderer_set_scale(float scale) {
   if (scale < 1.0f) {
      d_err("scale factor must be >= 1.0");
      return;
   }
   else if (scale > 100.0f) {
      d_err("okay... let's be normal about the scale factor......");
      return;
   }

   g_renderer.scale_factor = scale;
}

void renderer_set_display_resolution(DisplayResolution res) {
   if (!g_renderer.initialized) return;
   
   if ((res == RES_VGA || res == RES_FWVGA) && res != g_renderer.display_resolution) {
      g_renderer.display_resolution = res;
   }
}

void renderer_set_display_mode(DisplayMode mode) {
   if (!g_renderer.initialized || mode == g_renderer.display_mode) return;

   // going to fullscreen
   if (g_renderer.display_mode == DISPLAY_WINDOWED) {
      g_renderer.last_windowed_width = g_renderer.screen.w;
      g_renderer.last_windowed_height = g_renderer.screen.h;
   }
   
   switch (mode) {
   case DISPLAY_WINDOWED:
      SDL_SetWindowFullscreen(g_renderer.window, 0);
      // QUES: what does setwindowfullscreen do without this if?
      if (g_renderer.last_windowed_width > 0) {
         SDL_SetWindowSize(g_renderer.window, 
            g_renderer.last_windowed_width, 
            g_renderer.last_windowed_height);
      }
      break;
      
   case DISPLAY_BORDERLESS:
      SDL_SetWindowFullscreen(g_renderer.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
      break;
      
   case DISPLAY_FULLSCREEN:
      SDL_SetWindowFullscreen(g_renderer.window, SDL_WINDOW_FULLSCREEN);
      break;
   }
   
   g_renderer.display_mode = mode;
   calculate_viewport();
}

void renderer_set_resize_mode(ResizeMode mode) {
   g_renderer.resize_mode = mode;
}

void renderer_set_clear_color(uint8_t color_index) {
   g_renderer.clear_color_index = color_index;
}

// LAYER MANAGEMENT
LayerHandle renderer_create_layer(bool can_draw_outside) {
   if (g_renderer.layer_count >= g_renderer.layer_capacity) {
      int new_capacity = g_renderer.layer_capacity * 2;
      Layer* new_layers = realloc(g_renderer.layers, sizeof(Layer) * new_capacity);
      if (d_dne(new_layers)) {
         return INVALID_LAYER;
      }
      g_renderer.layers = new_layers;
      g_renderer.layer_capacity = new_capacity;
      memset(&g_renderer.layers[g_renderer.layer_count], 0, 
            sizeof(Layer) * (new_capacity - g_renderer.layer_count));
   }
   
   // find next available slot
   Layer* layer = &g_renderer.layers[g_renderer.layer_count];
   
   layer->surface = create_surface();
   if (d_dne(layer->surface)) return INVALID_LAYER;
   
   layer->handle = g_renderer.next_layer_handle++;
   layer->can_draw_outside_viewport = can_draw_outside;
   layer->size = 1;
   layer->visible = true;
   layer->opacity = 255;
      
   g_renderer.layer_count++;
   
   d_logv(2, "created layer %u", layer->handle);
   
   return layer->handle;
}

// TODO: take multiple layers as arguments
void renderer_destroy_layer(LayerHandle handle) {
   if (handle == INVALID_LAYER) return;
   
   // get layer
   int layer_index = -1;
   for (int i = 0; i < g_renderer.layer_count; i++) {
      if (g_renderer.layers[i].handle == handle) {
         layer_index = i;
         break;
      }
   }
   if (layer_index == -1) {
      d_log("attempted to destroy invalid layer %u", handle);
      return;
   }
   
   Layer* layer = &g_renderer.layers[layer_index];

   // free   
   if (layer->surface) {
      SDL_FreeSurface(layer->surface);
      layer->surface = NULL;
   }
   
   // remove from array, shift elements
   for (int i = layer_index; i < g_renderer.layer_count - 1; i++) {
      g_renderer.layers[i] = g_renderer.layers[i + 1];
   }
   g_renderer.layer_count--;
   
   memset(&g_renderer.layers[g_renderer.layer_count], 0, sizeof(Layer));
   
   d_logv(2, "destroyed layer %u", handle);
}

void renderer_set_layer_draw_outisde(LayerHandle handle, bool can_draw) {
   Layer* layer = find_layer(handle);
   if (layer && can_draw != layer->can_draw_outside_viewport) {
      layer->can_draw_outside_viewport = can_draw;
   }
}

bool renderer_get_layer_draw_outside(LayerHandle handle) {
   Layer* layer = find_layer(handle);
   if (layer) {
      return layer->can_draw_outside_viewport;
   }
   return false;
}

void renderer_set_layer_visible(LayerHandle handle, bool visible) {
   Layer* layer = find_layer(handle);
   if (layer && visible != layer->visible) {
      layer->visible = visible;
   }
}

bool renderer_get_layer_visible(LayerHandle handle) {
   Layer* layer = find_layer(handle);
   if (layer) {
      return layer->visible;
   }
   return false;
}

void renderer_set_layer_opacity(LayerHandle handle, uint8_t opacity) {
   Layer* layer = find_layer(handle);
   if (layer && opacity != layer->opacity) {
      layer->opacity = opacity;
   }
}

uint8_t renderer_get_layer_opacity(LayerHandle handle) {
   Layer* layer = find_layer(handle);
   if (layer) {
      return layer->opacity;
   }
   return 0;
}

void renderer_set_layer_size(LayerHandle handle, uint8_t size) {
   Layer* layer = find_layer(handle);
   if (layer && size != layer->size && size != 0) {
      layer->size = size;
   }
}

uint8_t renderer_get_layer_size(LayerHandle handle) {
   Layer* layer = find_layer(handle);
   if (layer) {
      return layer->visible;
   }
   return false;
}

// DRAWING FUNCTIONS
void renderer_blit_masked(LayerHandle handle, ImageData* source, SDL_Rect src_rect,
                          int dest_x, int dest_y, uint8_t draw_color) {
   Layer* layer = find_layer(handle);
   if (!layer || !layer->surface || !source) return;

   SDL_Rect dest_game_rect = {
      dest_x,
      dest_y,
      src_rect.w * layer->size,
      src_rect.h * layer->size
   };
   SDL_Rect dest_screen_rect = dest_game_rect;
   renderer_convert_game_to_screen(&dest_screen_rect);
   
   // bounds checking once
   if (dest_screen_rect.x >= layer->surface->w ||
       dest_screen_rect.y >= layer->surface->h ||
       dest_screen_rect.x + dest_screen_rect.w <= 0 ||
       dest_screen_rect.y + dest_screen_rect.h <= 0) {
      return;
   }
   
   // clamp to surface bounds
   int clip_left = dest_screen_rect.x < 0 ? -dest_screen_rect.x : 0;
   int clip_top = dest_screen_rect.y < 0 ? -dest_screen_rect.y : 0;
   int clip_right = (dest_screen_rect.x + dest_screen_rect.w > layer->surface->w) ?
                    (dest_screen_rect.x + dest_screen_rect.w - layer->surface->w) : 0;
   int clip_bottom = (dest_screen_rect.y + dest_screen_rect.h > layer->surface->h) ?
                     (dest_screen_rect.y + dest_screen_rect.h - layer->surface->h) : 0;
   
   // adjust destination rect
   dest_screen_rect.x += clip_left;
   dest_screen_rect.y += clip_top;
   dest_screen_rect.w -= (clip_left + clip_right);
   dest_screen_rect.h -= (clip_bottom + clip_top);
   
   if (dest_screen_rect.w <= 0 || dest_screen_rect.h <= 0) return;
   
   blit_masked_scaled(layer, source, src_rect, dest_screen_rect, draw_color,
                      layer->size, clip_left, clip_top);
}

void renderer_draw_pixel(LayerHandle handle, int x, int y, uint8_t color_index) {
   Layer* layer = find_layer(handle);
   if (!layer || !layer->surface || color_index >= PALETTE_SIZE) return;
   if (!layer->can_draw_outside_viewport) {
      if (x < 0 || x >= g_renderer.screen.w || y < 0 || y >= g_renderer.screen.h) {
         return;
      }
   }
   SDL_Rect pixel = { x, y, layer->size, layer->size };
   renderer_convert_game_to_screen(&pixel);
   SDL_FillRect(layer->surface, &pixel, color_index);
}

void renderer_draw_rect_raw(LayerHandle handle, SDL_Rect rect, uint8_t color_index) {
   Layer* layer = find_layer(handle);
   if (!layer || !layer->surface || color_index >= PALETTE_SIZE) return;
   
   SDL_FillRect(layer->surface, &rect, color_index);
}

void renderer_draw_fill(LayerHandle handle, uint8_t color_index) {
   Layer* layer = find_layer(handle);
   if (!layer || !layer->surface || color_index >= PALETTE_SIZE) return;
   
   SDL_FillRect(layer->surface, NULL, color_index);
}

void renderer_draw_rect(LayerHandle handle, SDL_Rect rect, uint8_t color_index) {
   Layer* layer = find_layer(handle);
   if (!layer || !layer->surface || color_index >= PALETTE_SIZE) return;
   
   renderer_convert_game_to_screen(&rect);
   SDL_FillRect(layer->surface, &rect, color_index);
}

void renderer_draw_char(LayerHandle handle, FontType font_type, char c, int x, int y, uint8_t color_index) {
   Layer* layer = find_layer(handle);
   Font* font = file_get_font(&g_renderer.font_array, font_type);
   if (!layer || !layer->surface || !font || color_index >= PALETTE_SIZE) return;
   if (c == ' ') return;

   int sheet_index = (int)c - font->ascii_start;
   if (sheet_index < 0 || sheet_index >= (font->image_w * font->image_h)) return;
   
   int tile_x = sheet_index % font->image_w;
   int tile_y = sheet_index / font->image_w;
   
   SDL_Rect src_rect = {
      tile_x * font->tile_w,
      tile_y * font->tile_h,
      font->tile_w,
      font->tile_h
   };
   
   // d_logv(3, "Char: %c, Index: %d, Tile: [%d, %d], SrcRect: [%d, %d, %d, %d]\n",
          // c, sheet_index, tile_x, tile_y,
          // src_rect.x, src_rect.y, src_rect.w, src_rect.h);
   
   renderer_blit_masked(handle, font->data, src_rect, x, y, color_index);
}

void renderer_draw_string(LayerHandle handle, FontType font_type, const char* str, int x, int y, uint8_t color_index) {
   Layer* layer = find_layer(handle);
   Font* font = file_get_font(&g_renderer.font_array, font_type);
   if (!layer || !layer->surface || !font || !str || color_index >= PALETTE_SIZE) return;
   
   x -= (font->tile_w * layer->size); // uhh to line it up cause i add again
   for (int i = 0; i < (int)strlen(str); i++) {
      if (str[i] == ' ') {
         x += (font->tile_w * layer->size);
         continue;
      }
   
      int sheet_index = (int)str[i] - font->ascii_start;
      if (sheet_index < 0 || sheet_index >= (font->image_w * font->image_h)) continue;

      int tile_x = sheet_index % font->image_w;
      int tile_y = sheet_index / font->image_w;

      SDL_Rect src_rect = {
         tile_x * font->tile_w,
         tile_y * font->tile_h,
         font->tile_w,
         font->tile_h
      };
      // d_logv(3, "Char: %c, Index: %d, Tile: [%d, %d], SrcRect: [%d, %d, %d, %d]\n",
          // str[i], sheet_index, tile_x, tile_y,
          // src_rect.x, src_rect.y, src_rect.w, src_rect.h);
      x += (font->tile_w * layer->size);
      renderer_blit_masked(handle, font->data, src_rect, x, y, color_index);
   }
}

// everything is messed up here
void renderer_draw_system_quit(uint8_t duration_held) {
   LayerHandle handle = g_renderer.system_layer_handle;
   Layer* layer = find_layer(handle);
   if (!layer || !layer->surface) return;

   if (duration_held == 0) { renderer_set_layer_visible(handle, false); return; }
   else if (!layer->visible) renderer_set_layer_visible(handle, true);

   uint8_t color = 0;
   if (duration_held < 64) color = 3;
   else if (duration_held < 128) color = 2;
   else if (duration_held < 192) color = 1;
   
   renderer_draw_string(handle, FONT_ACER_8_8, "Quitting game...", 1, 1, color);
   SDL_Rect asdf = {200, 200, 100, 100};
   renderer_draw_rect(handle, asdf, 11);
}

// UTILITY FUNCTIONS
SDL_Surface* renderer_get_layer_surface(LayerHandle handle) {
   Layer* layer = find_layer(handle);
   return layer ? layer->surface : NULL;
}

void renderer_convert_game_to_screen(SDL_Rect* rect) {
   // convert game coords into screen
   // d_log("");
   // d_logl("converted %s to ", d_name_rect(rect));
   
   if (!rect) return;

/*
   // METHOD 1 - edge rounding
   float left = g_renderer.viewport.x + (rect->x * g_renderer.scale_factor);
   float right = g_renderer.viewport.x + ((rect->x + rect->w) * g_renderer.scale_factor);
   float top = g_renderer.viewport.y + (rect->y * g_renderer.scale_factor);
   float bottom = g_renderer.viewport.y + ((rect->y + rect->h) * g_renderer.scale_factor);
   
   rect->x = (int)roundf(left);
   rect->y = (int)roundf(top);
   rect->w = (int)roundf(right) - rect->x;
   rect->h = (int)roundf(bottom) - rect->y;
   
   // METHOD 2 - floor-based mapping
   float left = g_renderer.viewport.x + (rect->x * g_renderer.scale_factor);
   float right = g_renderer.viewport.x + ((rect->x + rect->w) * g_renderer.scale_factor);
   float top = g_renderer.viewport.y + (rect->y * g_renderer.scale_factor);
   float bottom = g_renderer.viewport.y + ((rect->y + rect->h) * g_renderer.scale_factor);
   
   rect->x = (int)floorf(left);
   rect->y = (int)floorf(top);
   rect->w = (int)floorf(right) - rect->x;
   rect->h = (int)floorf(bottom) - rect->y;
*/

   // METHOD 3 - integer math w/ fixed-point scaling
   // use integer math with fixed-point scaling
   int sf1000 = (int)(g_renderer.scale_factor * 1000.0f); // 3 digits precision
   
   // edge-based conversion with integer math
   int left = g_renderer.viewport.x + (rect->x * sf1000) / 1000;
   int right = g_renderer.viewport.x + ((rect->x + rect->w) * sf1000) / 1000;
   int top = g_renderer.viewport.y + (rect->y * sf1000) / 1000;
   int bottom = g_renderer.viewport.y + ((rect->y + rect->h) * sf1000) / 1000;
   
   rect->x = left;
   rect->y = top;
   rect->w = right - left;
   rect->h = bottom - top;
   
   // d_logl("%s\n", d_name_rect(rect));
}

// TODO: change this as well
void renderer_convert_screen_to_game(SDL_Rect* rect) {
   // convert screen coords into game coords
   // d_log("");
   // d_logl("converted %s to ", d_name_rect(rect));
   if (!rect) return;

   // METHOD 1 - edge rounding
   // float left = (rect->x - g_renderer.viewport.x) / g_renderer.scale_factor;
   // float right = (rect->x + rect->w - g_renderer.viewport.x) / g_renderer.scale_factor;
   // float top = (rect->y - g_renderer.viewport.y) / g_renderer.scale_factor;
   // float bottom = (rect->y + rect->h - g_renderer.viewport.y) / g_renderer.scale_factor;
   // 
   // rect->x = (int)roundf(left);
   // rect->y = (int)roundf(top);
   // rect->w = (int)roundf(right) - rect->x;
   // rect->h = (int)roundf(bottom) - rect->y;

/*
   // METHOD 2 - floor-based mapping
   float left = (rect->x - g_renderer.viewport.x) / g_renderer.scale_factor;
   float right = (rect->x + rect->w - g_renderer.viewport.x) / g_renderer.scale_factor;
   float top = (rect->y - g_renderer.viewport.y) / g_renderer.scale_factor;
   float bottom = (rect->y + rect->h - g_renderer.viewport.y) / g_renderer.scale_factor;
   
   rect->x = (int)floorf(left);
   rect->y = (int)floorf(top);
   rect->w = (int)floorf(right) - rect->x;
   rect->h = (int)floorf(bottom) - rect->y;
*/
   // d_logl("%s\n", d_name_rect(rect));
}

void renderer_get_screen_dimensions(int* width, int* height) {
   if (width) *width = g_renderer.screen.w;
   if (height) *height = g_renderer.screen.h;
}

void renderer_get_viewport_dimensions(int* width, int* height) {
   if (width) *width = g_renderer.viewport.w;
   if (height) *height = g_renderer.viewport.h;
}

void renderer_get_game_dimensions(int* width, int* height) {
   if (width) *width = g_renderer.game_coords.w;
   if (height) *height = g_renderer.game_coords.h;
}

float renderer_get_scale_factor(void) {
   return g_renderer.scale_factor;
}

bool renderer_is_in_viewport(int x, int y){
   return (x >= 0 && x < g_renderer.screen.w && 
            y >= 0 && y < g_renderer.screen.h);
}

// INTERNAL
static void calculate_viewport(void) {
   /* affects viewport x,y,w,h and scale_factor */
   ResizeMode effective_resize_mode = g_renderer.resize_mode;
   if (g_renderer.display_mode == DISPLAY_BORDERLESS || 
       g_renderer.display_mode == DISPLAY_FULLSCREEN) {
      effective_resize_mode = RESIZE_FIT;
   }
   
   switch (effective_resize_mode) {
   case RESIZE_FIT:
      SDL_Rect result = { 0 };
      float fixed_aspect = (float)g_renderer.game_coords.w / g_renderer.game_coords.h;
      float window_aspect = (float)g_renderer.screen.w / g_renderer.screen.h;

      if (fixed_aspect > window_aspect) { // taller, letterboxing on top/bottom
         result.w = g_renderer.screen.w;
         result.h = (int)(g_renderer.screen.w / fixed_aspect);
      } else { // wider, letterboxing on sides
         result.w = (int)(g_renderer.screen.h * fixed_aspect);
         result.h = g_renderer.screen.h;
      }
      g_renderer.scale_factor = (float)result.w / g_renderer.game_coords.w;
      
      g_renderer.viewport.x = (g_renderer.screen.w - result.w) / 2;
      g_renderer.viewport.y = (g_renderer.screen.h - result.h) / 2;
      g_renderer.viewport.w = result.w;
      g_renderer.viewport.h = result.h;

      break;
      
   case RESIZE_FIXED:
      // viewport width and height don't change
      g_renderer.viewport.x = (g_renderer.screen.w - g_renderer.viewport.w) / 2;
      g_renderer.viewport.y = (g_renderer.screen.h - g_renderer.viewport.h) / 2;
      break;
   }
   d_logv(2, "viewport calculated to be %s", d_name_rect(&g_renderer.viewport));
}

static Layer* find_layer(LayerHandle handle) {
   if (handle == INVALID_LAYER) return NULL;
   
   for (int i = 0; i < g_renderer.layer_count; i++) {
      if (g_renderer.layers[i].handle == handle) {
         return &g_renderer.layers[i];
      }
   }
   return NULL;
}

// only used for layers
SDL_Surface* create_surface(void) {
   SDL_Surface* surface = SDL_CreateRGBSurface(0, g_renderer.screen.w, g_renderer.screen.h, 8, 0, 0, 0, 0);
   if (d_dne(surface)) {
      return NULL;
   }   
   SDL_SetPaletteColors(surface->format->palette, get_palette_colors(), 0, PALETTE_SIZE);
   SDL_SetColorKey(surface, SDL_TRUE, g_renderer.transparent_color_index);
   SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
   SDL_SetSurfaceAlphaMod(surface, 255);

   SDL_FillRect(surface, NULL, g_renderer.transparent_color_index);
   return surface;
}

void recreate_surface(SDL_Surface** surface) {
   // called if change in g_renderer.screen or transparent_color_index
   if (surface && *surface) {
      SDL_FreeSurface(*surface);
   }
   *surface = create_surface();
   if (d_dne(*surface)) {
      d_log("new surface doesn't exist");
      return;
   }
   // d_var((*surface)->w);
   // d_var((*surface)->h);
}

void resize_all_surfaces(void) {
   int new_w, new_h;
   SDL_GetWindowSize(g_renderer.window, &new_w, &new_h);
   
   if (g_renderer.composite_surface->w != new_w || g_renderer.composite_surface->h != new_h) {
      d_log("");
      d_logl("recreating composite surface");
      SDL_FreeSurface(g_renderer.composite_surface);
      g_renderer.composite_surface = SDL_CreateRGBSurfaceWithFormat(
                     0, g_renderer.screen.w, g_renderer.screen.h,
                     32, SDL_PIXELFORMAT_RGBA8888);
      if (d_dne(g_renderer.composite_surface)) {
         return;
      }
      for (int i = 0; i < g_renderer.layer_count; i++) {
         if (i != 0) d_logl(", %d", i);
         else d_logl(" and layer %d", i);
         recreate_surface(&g_renderer.layers[i].surface);
      }
      d_logl("\n");
   }
   g_renderer.window_surface = SDL_GetWindowSurface(g_renderer.window);
   if (d_dne(g_renderer.window_surface)) d_err("can't get widnow surfact haha");
   // d_print_renderer_dims();
   return;
}

static void blit_masked_scaled(Layer* layer, ImageData* source, SDL_Rect src_rect,
                              SDL_Rect dest_screen_rect, uint8_t draw_color,
                              uint8_t size, int clip_left, int clip_top) {
   // say("dest_screen_rect = %s\n", d_name_rect(&dest_screen_rect));
   uint8_t* layer_pixels = (uint8_t*)layer->surface->pixels;
   int layer_pitch = layer->surface->pitch;
   
   float x_scale = (float)dest_screen_rect.w / src_rect.w;
   float y_scale = (float)dest_screen_rect.h / src_rect.h;
   
   for (int dest_y = 0; dest_y < dest_screen_rect.h; dest_y++) {
      for (int dest_x = 0; dest_x < dest_screen_rect.w; dest_x++) {
         // map destination pixel back to source
         int src_x = (int)(dest_x / x_scale);
         int src_y = (int)(dest_y / y_scale);
         
         // source pixel check with clipping
         int source_pixel_x = src_rect.x + src_x + clip_left / size;
         int source_pixel_y = src_rect.y + src_y + clip_top / size;
         int src_offset = (source_pixel_y * source->width + source_pixel_x) * 4;
         
         if (source->data[src_offset + 1] > 128) continue; // transparent
         
         // draw the destination pixel
         int screen_x = dest_screen_rect.x + dest_x;
         int screen_y = dest_screen_rect.y + dest_y;
         layer_pixels[screen_y * layer_pitch + screen_x] = draw_color;
      }
   }
}

static SDL_Color* get_palette_colors(void) {
   static SDL_Color colors[PALETTE_SIZE];
   static bool initialized = false;
   
   if (!initialized) {
      for (int i = 0; i < PALETTE_SIZE; i++) {
         colors[i].r = (palette[i] >> 24) & 0xFF;
         colors[i].g = (palette[i] >> 16) & 0xFF;
         colors[i].b = (palette[i] >> 8) & 0xFF;
         colors[i].a = 255;
      }
      initialized = true;
   }
   return colors;
}

const RendererState* renderer_get_debug_state(void) {
   return &g_renderer;
}

void d_print_renderer_dims(void) {
   d_log("-------------");
   d_log("RENDERER DIMS");
   d_var(g_renderer.window_surface->w);
   d_var(g_renderer.window_surface->h);
   d_var(g_renderer.composite_surface->w);
   d_var(g_renderer.composite_surface->h);
   d_log("display_resolution = %s", d_name_display_resolution(g_renderer.display_resolution));
   d_log("viewport =           %s", d_name_rect(&g_renderer.viewport));
   d_log("screen =             %s", d_name_rect(&g_renderer.screen));
   d_log("display_mode =       %s", d_name_display_mode(g_renderer.display_mode));
   d_var(g_renderer.last_windowed_width);
   d_var(g_renderer.last_windowed_height);
   for (int i = 0; i < g_renderer.layer_count; i++) {
      d_log("layer %d:", i);
      d_var(g_renderer.layers[i].surface->w);
      d_var(g_renderer.layers[i].surface->h);
   }
}