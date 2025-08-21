#include "renderer.h"
#include "timing.h"
#include "file.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>

static RendererState g_renderer = { 0 };

static void draw_system_header(int* x, int* y);
static void draw_system_data(SystemData data, int* x, int* y);
static void calculate_mapping(void);
static Layer* find_layer(LayerHandle handle);
static SDL_Surface* create_layer_surface(bool can_draw_outside);
static void resize_all_surfaces(void);
static SDL_Color* get_palette_colors(void);
static void blit_rect(Layer* layer, Rect* rect, uint8_t color_index);

// TODO: mapping method and dirty rect marking

// CORE FUNCTIONS
bool renderer_init(float scale_factor) {
   if (g_renderer.initialized) {
      d_err("the renderer is already initialized...");
      return false;
   }
   
   if (scale_factor < 1.0f || scale_factor > 20.0f) {
      d_err("scale factor must be >= 1.0... and not too big either");
      return false;
   }
   
   g_renderer.window = SDL_CreateWindow(
      WINDOW_TITLE,
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      (int)(GAME_WIDTH_FWVGA * scale_factor),
      (int)(GAME_HEIGHT_FWVGA * scale_factor),
      SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
   );
   if (d_dne(g_renderer.window)) {
      renderer_cleanup();
      return false;
   }
   
   g_renderer.scale_factor = scale_factor;
   g_renderer.display_resolution = RES_VGA;
   SDL_SetWindowMinimumSize(g_renderer.window, GAME_WIDTH_VGA, GAME_HEIGHT_VGA);
   
   g_renderer.window_surface = SDL_GetWindowSurface(g_renderer.window);
   if (d_dne(g_renderer.window_surface)) {
      renderer_cleanup();
      return false;
   }
   
   calculate_mapping();
   
   g_renderer.resize_mode = RESIZE_FIT;
   g_renderer.resize_in_progress = false;
   g_renderer.resize_start_time = timing_get_game_time_ms();
   g_renderer.resize_delay_ms = RESIZE_DELAY;
   
   g_renderer.display_mode = DISPLAY_WINDOWED;
   g_renderer.last_windowed_width = g_renderer.window_surface->w;
   g_renderer.last_windowed_height = g_renderer.window_surface->h;
   
   g_renderer.clear_color_index = 4; // mono-black
   g_renderer.transparent_color_index = PALETTE_TRANSPARENT;

   g_renderer.composite_surface = SDL_CreateRGBSurfaceWithFormat(
                  0,
                  (g_renderer.unit_map.x * 2) + g_renderer.unit_map.w,
                  (g_renderer.unit_map.y * 2) + g_renderer.unit_map.h,
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
   
   LayerHandle system_layer = renderer_create_layer(true);
   if (system_layer == INVALID_LAYER) {
      d_err("could not create system layer");
      renderer_cleanup();
      return false;
   }
   g_renderer.system_layer_handle = system_layer;
   for (int i = 0; i < SYS_MAX; i++) {
      g_renderer.system_layer_data[i] = false;
   }
   renderer_set_layer_size(g_renderer.system_layer_handle, 1);
   
   if (file_load_sheets(&g_renderer.sprite_array, &g_renderer.font_array) == 0) {
      renderer_cleanup();
      return false;
   }
   
   g_renderer.initialized = true;

   // debug display toggles
   renderer_toggle_system_data(SYS_CURRENT_FPS, true);
   renderer_toggle_system_data(SYS_AVG_FPS, true);
   
   return true;
}

void renderer_cleanup(void) {
   if (!g_renderer.initialized) return;

   // unload assets
   file_unload_sheets(&g_renderer.font_array, &g_renderer.sprite_array);

   // destroy layers
   if (g_renderer.layers) {
      while (g_renderer.layer_count > 0) {
         renderer_destroy_layer(g_renderer.layers[0].handle);
      }
      free(g_renderer.layers);
      g_renderer.layers = NULL;
      g_renderer.layer_capacity = 0;
   }

   // free composite surface
   if (g_renderer.composite_surface) {
      SDL_FreeSurface(g_renderer.composite_surface);
      g_renderer.composite_surface = NULL;
   }
   g_renderer.window_surface = NULL; // SDL will handle freeing

   // destory the window
   if (g_renderer.window) {
      SDL_DestroyWindow(g_renderer.window);
      g_renderer.window = NULL;
   }
   
   memset(&g_renderer, 0, sizeof(RendererState));
   g_renderer.initialized = false;
   SDL_Log("I cleaned up the renderer");
}

void renderer_clear(void) {
   if (!g_renderer.initialized) return;
   SDL_FillRect(g_renderer.composite_surface, NULL, palette[g_renderer.clear_color_index]); // clear composite
   for (int i = 1; i < g_renderer.layer_count; i++) {
      Layer* layer = &g_renderer.layers[i];
      if (!layer) continue;
      renderer_draw_fill(i, g_renderer.transparent_color_index);
   }
}

extern void scene_render(void);
void renderer_present(void) {   
   if (!g_renderer.initialized) return;
   // d_print_renderer_dims();
   if (g_renderer.resize_in_progress) {
      uint32_t current_time = timing_get_game_time_ms();
      uint32_t time_since_resize = current_time - g_renderer.resize_start_time;
      
      if (time_since_resize >= g_renderer.resize_delay_ms) {
         d_log("resized, scale factor = %f", g_renderer.scale_factor);
         resize_all_surfaces();
         g_renderer.resize_in_progress = false;
      } else {
         // just stretch it !
         g_renderer.window_surface = SDL_GetWindowSurface(g_renderer.window);
         if (d_dne(g_renderer.window_surface)) d_err("can't get window surface");
         SDL_BlitScaled(g_renderer.composite_surface, NULL,
                       g_renderer.window_surface, NULL);
         SDL_UpdateWindowSurface(g_renderer.window);
         return;
      }
   }
   SDL_FillRect(g_renderer.composite_surface, NULL, g_renderer.clear_color_index);
   
   scene_render(); // get all rendering calls from current scene
   
   // composite all visible layers
   for (int i = 0; i < g_renderer.layer_count; i++) {
      Layer* layer = &g_renderer.layers[i];
      if (!layer->visible || layer->handle == g_renderer.system_layer_handle) continue;
      
      SDL_SetSurfaceAlphaMod(layer->surface, layer->opacity);
      
      if (layer->can_draw_outside_viewport) {
         SDL_BlitSurface(layer->surface, NULL, g_renderer.composite_surface, NULL);
      } else {
         SDL_BlitSurface(layer->surface, NULL, g_renderer.composite_surface, &g_renderer.unit_map);
      }
   }

   // draw system layer
   Layer* system_layer = find_layer(g_renderer.system_layer_handle);
   if (system_layer->visible) {
      /* header */
      int x = -g_renderer.unit_map.x;
      int y = -g_renderer.unit_map.y;
      x += (1 * system_layer->size);
      y += (1 * system_layer->size);
      draw_system_header(&x, &y);
      y += (8 * system_layer->size);
      y += 2; // padding

      /* system data */
      for (int i = 0; i < SYS_MAX; i++) {
         if (g_renderer.system_layer_data[i]) {
            draw_system_data(i, &x, &y);
         }
      }
      SDL_BlitSurface(system_layer->surface, NULL, g_renderer.composite_surface, NULL);
   }
   
   SDL_FillRect(g_renderer.window_surface, NULL, g_renderer.clear_color_index);
   SDL_BlitScaled(g_renderer.composite_surface, NULL,
                 g_renderer.window_surface, NULL);
   SDL_UpdateWindowSurface(g_renderer.window);
}

void renderer_handle_window_event(SDL_Event* event) {
   if (event->type != SDL_WINDOWEVENT) return;
   
   switch (event->window.event) {
   case SDL_WINDOWEVENT_SIZE_CHANGED:
      g_renderer.window_surface = SDL_GetWindowSurface(g_renderer.window); // updates w/h
      if (d_dne(g_renderer.window_surface)) d_err("HELP! can't get the window surface");
      calculate_mapping();
      break;
   case SDL_WINDOWEVENT_RESIZED:
      if (!g_renderer.resize_in_progress) {
         g_renderer.resize_in_progress = true;
         g_renderer.resize_start_time = timing_get_game_time_ms();
      } else {
         // subsequent resize events - just update the start time
         g_renderer.resize_start_time = timing_get_game_time_ms();
      }
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
void renderer_set_display_resolution(DisplayResolution res) {
   if (!g_renderer.initialized) return;
   
   g_renderer.display_resolution = res;
   calculate_mapping();
}

void renderer_set_display_mode(DisplayMode mode) {
   if (!g_renderer.initialized || mode == g_renderer.display_mode) return;

   // going to fullscreen
   if (g_renderer.display_mode == DISPLAY_WINDOWED) {
      g_renderer.last_windowed_width = g_renderer.window_surface->w;
      g_renderer.last_windowed_height = g_renderer.window_surface->h;
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
}

void renderer_set_resize_mode(ResizeMode mode) {
   if (!g_renderer.initialized) return;

   // might actually make this so you have to specify scale_factor
   if (mode == RESIZE_FIXED) {
      switch (g_renderer.display_resolution) {
      case RES_VGA: {
         int w = GAME_WIDTH_VGA * g_renderer.scale_factor;
         int h = GAME_HEIGHT_VGA * g_renderer.scale_factor;
         SDL_SetWindowMinimumSize(g_renderer.window, w, h);
         break;
      }
         
      case RES_FWVGA: {
         int w = GAME_WIDTH_FWVGA * g_renderer.scale_factor;
         int h = GAME_HEIGHT_FWVGA * g_renderer.scale_factor;
         SDL_SetWindowMinimumSize(g_renderer.window, w, h);
         break;
      }
      }
   }
   g_renderer.resize_mode = mode;
}

void renderer_set_clear_color(uint8_t color_index) {
   if (!g_renderer.initialized) return;
   
   g_renderer.clear_color_index = color_index;
}

// LAYER MANAGEMENT
LayerHandle renderer_create_layer(bool can_draw_outside) {
   if (g_renderer.layer_count >= g_renderer.layer_capacity) {
      int new_capacity = g_renderer.layer_capacity * 2;
      Layer* new_layers = realloc(g_renderer.layers, sizeof(Layer) * new_capacity);
      if (d_dne(new_layers)) {
         d_err("couldn't resize layer array");
         return INVALID_LAYER;
      }
      g_renderer.layers = new_layers;
      g_renderer.layer_capacity = new_capacity;
      memset(&g_renderer.layers[g_renderer.layer_count], 0, 
            sizeof(Layer) * (new_capacity - g_renderer.layer_count));
   }
   
   // find next available slot
   Layer* layer = &g_renderer.layers[g_renderer.layer_count];
   
   layer->surface = create_layer_surface(can_draw_outside);
   if (d_dne(layer->surface)) {
      d_err("couldn't create layer surface");
      return INVALID_LAYER;
   }
   
   layer->handle = g_renderer.next_layer_handle++;
   layer->can_draw_outside_viewport = can_draw_outside;
   layer->size = 2;
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

void renderer_set_layer_draw_outside(LayerHandle handle, bool can_draw) {
   Layer* layer = find_layer(handle);
   // TODO: preserve what was already done when scaling
   if (layer && layer->surface && can_draw != layer->can_draw_outside_viewport) {
      SDL_FreeSurface(layer->surface);
      layer->can_draw_outside_viewport = can_draw;
      layer->surface = create_layer_surface(layer->can_draw_outside_viewport);
      if (d_dne(layer->surface)) {
         d_log("new surface don't exist");
         return;
      }
   }
}

void renderer_set_layer_visible(LayerHandle handle, bool visible) {
   Layer* layer = find_layer(handle);
   if (layer && visible != layer->visible) {
      layer->visible = visible;
   }
}

void renderer_set_layer_opacity(LayerHandle handle, uint8_t opacity) {
   Layer* layer = find_layer(handle);
   if (layer && opacity != layer->opacity) {
      layer->opacity = opacity;
   }
}

void renderer_set_layer_size(LayerHandle handle, uint8_t size) {
   Layer* layer = find_layer(handle);
   if (layer && size != layer->size && size != 0) {
      layer->size = size;
   }
}

// DRAWING FUNCTIONS
void renderer_blit_masked(LayerHandle handle, ImageData* source, Rect src_rect,
                          int dest_x, int dest_y, uint8_t color_index) {
   if (g_renderer.resize_in_progress || color_index >= PALETTE_SIZE) return;
   Layer* layer = find_layer(handle);
   if (!layer || !layer->surface || !source) return;

   // TODO: adjust this to align with layer size
   Rect dest_rect = {
      dest_x,
      dest_y,
      src_rect.w * layer->size,
      src_rect.h * layer->size
   };
   
   // bounds check
   int lower_bound_x, lower_bound_y, upper_bound_x, upper_bound_y;
   if (layer->can_draw_outside_viewport) {
      // can draw anywhere on the surface, expressed in window coordinates
      lower_bound_x = -g_renderer.unit_map.x;
      lower_bound_y = -g_renderer.unit_map.y;
      upper_bound_x = layer->surface->w - g_renderer.unit_map.x;
      upper_bound_y = layer->surface->h - g_renderer.unit_map.y;
   } else {
      // can only draw within viewport
      lower_bound_x = 0;
      lower_bound_y = 0;
      upper_bound_x = g_renderer.unit_map.w;
      upper_bound_y = g_renderer.unit_map.h;
   }

   if (dest_rect.x + dest_rect.w <= lower_bound_x ||
       dest_rect.y + dest_rect.h <= lower_bound_y ||
       dest_rect.x >= upper_bound_x ||
       dest_rect.y >= upper_bound_y) {
      return;
   }
   
   // TODO: adjust this to align with layer size
   // clamp to drawing bounds
   int clip_left   = (dest_rect.x < lower_bound_x) ? (lower_bound_x - dest_rect.x) : 0;
   int clip_top    = (dest_rect.y < lower_bound_y) ? (lower_bound_y - dest_rect.y) : 0;
   int clip_right  = (dest_rect.x + dest_rect.w > upper_bound_x) ?
                     (dest_rect.x + dest_rect.w - upper_bound_x) : 0;
   int clip_bottom = (dest_rect.y + dest_rect.h > upper_bound_y) ?
                     (dest_rect.y + dest_rect.h - upper_bound_y) : 0;
   
   int src_clip_left = clip_left / layer->size;
   int src_clip_top = clip_top / layer->size;
   
   // adjust destination rect
   dest_rect.x += clip_left;
   dest_rect.y += clip_top;
   dest_rect.w -= (clip_left + clip_right);
   dest_rect.h -= (clip_bottom + clip_top);
   
   if (dest_rect.w <= 0 || dest_rect.h <= 0) return;
   
   uint8_t* layer_pixels = (uint8_t*)layer->surface->pixels;
   int layer_pitch = layer->surface->pitch;

   if (layer->can_draw_outside_viewport) {
      dest_rect.x += g_renderer.unit_map.x;
      dest_rect.y += g_renderer.unit_map.y;
   }
   
   for (int dest_y = 0; dest_y < dest_rect.h; dest_y++) {
      for (int dest_x = 0; dest_x < dest_rect.w; dest_x++) {
         // map destination pixel back to original unclipped position
         int src_x = src_rect.x + src_clip_left + (dest_x / layer->size);
         int src_y = src_rect.y + src_clip_top + (dest_y / layer->size);
         
         // bounds check for source image
         if (src_x >= source->width || src_y >= source->height ||
             src_x < 0 || src_y < 0) continue;
         
         int src_offset = (src_y * source->width + src_x) * 4;
         
         if (source->data[src_offset + 1] > 128) continue; // transparent
         
         // draw the destination pixel
         int screen_x = dest_rect.x + dest_x;
         int screen_y = dest_rect.y + dest_y;
         
         // bounds check for layer surface
         if (screen_x >= 0 && screen_x < layer->surface->w && 
             screen_y >= 0 && screen_y < layer->surface->h) {
            layer_pixels[screen_y * layer_pitch + screen_x] = color_index;
         }
      }
   }
}

void renderer_draw_pixel(LayerHandle handle, int x, int y, uint8_t color_index) {
   if (g_renderer.resize_in_progress || color_index >= PALETTE_SIZE) return;
   Layer* layer = find_layer(handle);
   if (!layer || !layer->surface) return;
   Rect pixel = {x, y, layer->size, layer->size};
   blit_rect(layer, &pixel, color_index);
}

void renderer_draw_rect(LayerHandle handle, Rect rect, uint8_t color_index) {
   if (g_renderer.resize_in_progress || color_index >= PALETTE_SIZE) return;
   Layer* layer = find_layer(handle);
   if (!layer || !layer->surface) return;

   blit_rect(layer, &rect, color_index);
}

void renderer_draw_rect_raw(Rect rect, uint8_t color_index) {
   if (g_renderer.resize_in_progress || color_index >= PALETTE_SIZE) return;
   
   SDL_FillRect(g_renderer.composite_surface, &rect, color_index);
}

void renderer_draw_fill(LayerHandle handle, uint8_t color_index) {
   if (g_renderer.resize_in_progress || color_index >= PALETTE_SIZE) return;
   Layer* layer = find_layer(handle);
   if (!layer || !layer->surface) return;
   
   blit_rect(layer, NULL, color_index);
}

void renderer_draw_char(LayerHandle handle, FontType font_type, char c, int x, int y, uint8_t color_index) {
   if (g_renderer.resize_in_progress || color_index >= PALETTE_SIZE) return;
   Layer* layer = find_layer(handle);
   Font* font = file_get_font(&g_renderer.font_array, font_type);
   if (!layer || !layer->surface || !font) return;
   if (c == ' ') return;

   int sheet_index = (int)c - font->ascii_start;
   if (sheet_index < 0 || sheet_index >= (font->image_w * font->image_h)) return;
   
   int tile_x = sheet_index % font->image_w;
   int tile_y = sheet_index / font->image_w;
   
   Rect src_rect = {
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
   if (g_renderer.resize_in_progress || !str || color_index >= PALETTE_SIZE) return;
   Layer* layer = find_layer(handle);
   Font* font = file_get_font(&g_renderer.font_array, font_type);
   if (!layer || !layer->surface || !font) return;
   
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

      Rect src_rect = {
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

// SYSTEM LAYER
void renderer_toggle_system_data(SystemData data, bool display) {
   if (data < 0 || data >= SYS_MAX) return;
   display ? (g_renderer.system_layer_data[data] = true) : (g_renderer.system_layer_data[data] = false);
   d_logv(2, "%s display set to %s", d_name_system_data(data), display ? "true" : "false");
}

static void draw_system_header(int* x, int* y) {
   uint8_t color = 0; // mono-white
   renderer_draw_string(g_renderer.system_layer_handle, FONT_SHARP_8_8, "teaf/g v0.01", *x, *y, color);
}

static void draw_system_data(SystemData data, int* x, int* y) {
   LayerHandle handle = g_renderer.system_layer_handle;
   // Layer* layer = find_layer(handle);
   uint8_t size = find_layer(handle)->size;
   int new_line = 8 * size; // TODO: get font tile height
   new_line += 2; // padding
   FontType font = FONT_SHARP_8_8;
   uint8_t color = 0; // mono-white
   char buffer[128];
   
   switch (data) {
   case SYS_CURRENT_FPS:
      float fps = timing_get_current_fps();
      snprintf(buffer, sizeof(buffer), "%.2f fps", fps);
      renderer_draw_string(handle, font, buffer, *x, *y, color);
      *y += new_line;
      break;
      
   case SYS_AVG_FPS:
      uint32_t min_ms = 0, max_ms = 0, avg_ms = 0, frames_over = 0;
      char buffer[128];
      timing_get_performance_info(&min_ms, &max_ms, &avg_ms, &frames_over);
      
      snprintf(buffer, sizeof(buffer), "avg = %u ms", avg_ms);
      renderer_draw_string(handle, font, buffer, *x, *y, color);
      if (frames_over > 0) {
         snprintf(buffer, sizeof(buffer), "(%u over budget!)", frames_over);
         renderer_draw_string(handle, font, buffer, (*x + (12 * 8)), *y, color);
      }
      *y += new_line;
      snprintf(buffer, sizeof(buffer), "(min %u, max %u)", min_ms, max_ms);
      renderer_draw_string(handle, font, buffer, *x, *y, color);
      *y += new_line;
      break;
      
   case SYS_LAYER_COUNT:
      renderer_draw_string(handle, font, "haha weeeeee", *x, *y, color);
      *y += new_line;
      
      break;

   default:
      d_log("unhandled system data draw");
   }
}

void renderer_draw_system_quit(uint8_t duration_held) {
   LayerHandle handle = g_renderer.system_layer_handle;
   Layer* layer = find_layer(handle);
   if (!layer || !layer->surface) return;

   // if (duration_held == 0) { renderer_set_layer_visible(handle, false); return; }
   // else if (!layer->visible) renderer_set_layer_visible(handle, true);

   uint8_t color = 0;
   if (duration_held < 64) color = 3;
   else if (duration_held < 128) color = 2;
   else if (duration_held < 192) color = 1;
   
   renderer_draw_string(handle, FONT_ACER_8_8, "Quitting game...", 1, 1, color);
}

// UTILITY FUNCTIONS
SDL_Surface* renderer_get_layer_surface(LayerHandle handle) {
   Layer* layer = find_layer(handle);
   return layer ? layer->surface : NULL;
}

void renderer_get_window_dims(int* width, int* height) {
   if (width) *width = g_renderer.window_surface->w;
   if (height) *height = g_renderer.window_surface->h;
}

void renderer_get_dims(int* width, int* height) {
   if (width) *width = g_renderer.unit_map.w;
   if (height) *height = g_renderer.unit_map.h;
}

void renderer_get_dims_full(int* width, int* height, int* x_offset, int* y_offset) {
   if (width) *width = g_renderer.unit_map.w;
   if (height) *height = g_renderer.unit_map.h;
   if (x_offset) *x_offset = g_renderer.unit_map.x;
   if (y_offset) *y_offset = g_renderer.unit_map.y;
}

bool renderer_is_in_viewport(int x, int y) {
   return (x >= g_renderer.window_map.x &&
           x < g_renderer.window_map.x + g_renderer.window_map.w &&
           y >= g_renderer.window_map.y &&
           y < g_renderer.window_map.y + g_renderer.window_map.h);
}

// INTERNAL
static void calculate_mapping(void) {
   // call if change in window size or display resolution
   switch (g_renderer.display_resolution) {
   case RES_VGA:
      g_renderer.unit_map.w = GAME_WIDTH_VGA;
      g_renderer.unit_map.h = GAME_HEIGHT_VGA;
      break;
      
   case RES_FWVGA:
      g_renderer.unit_map.w = GAME_WIDTH_FWVGA;
      g_renderer.unit_map.h = GAME_HEIGHT_FWVGA;
      break;
   }
   
   // unit viewport always starts at origin - it's the full game area
   g_renderer.unit_map.x = 0;
   g_renderer.unit_map.y = 0;

   // this never changes
   g_renderer.window_map.x = 0;
   g_renderer.window_map.y = 0;
   
   ResizeMode effective_resize_mode = g_renderer.resize_mode;
   if (g_renderer.display_mode == DISPLAY_BORDERLESS || 
       g_renderer.display_mode == DISPLAY_FULLSCREEN) {
      effective_resize_mode = RESIZE_FIT;
   }
   
   switch (effective_resize_mode) {
   case RESIZE_FIT: {
      float viewport_aspect = (float)g_renderer.unit_map.w / g_renderer.unit_map.h;
      float window_aspect = (float)g_renderer.window_surface->w / g_renderer.window_surface->h;
      
      if (viewport_aspect < window_aspect) {
         // letterbox left/right
         g_renderer.scale_factor = (float)g_renderer.window_surface->h / g_renderer.unit_map.h;
   
         float viewport_width = g_renderer.unit_map.w * g_renderer.scale_factor; // window coords
         float x_offset_window = (g_renderer.window_surface->w - viewport_width) / 2;
         g_renderer.unit_map.x = (int)(x_offset_window / g_renderer.scale_factor + 0.5f);
         g_renderer.unit_map.y = 0;
      } else {
         // letterbox top/bottom
         g_renderer.scale_factor = (float)g_renderer.window_surface->w / g_renderer.unit_map.w;
   
         float viewport_height = g_renderer.unit_map.h * g_renderer.scale_factor; // window coords
         float y_offset_window = (g_renderer.window_surface->h - viewport_height) / 2;
         g_renderer.unit_map.x = 0;
         g_renderer.unit_map.y = (int)(y_offset_window / g_renderer.scale_factor + 0.5f);
      }
      }
      break;
      
   case RESIZE_FIXED:
      int window_x = (g_renderer.window_surface->w - (g_renderer.unit_map.w * g_renderer.scale_factor)) / 2;
      int window_y = (g_renderer.window_surface->h - (g_renderer.unit_map.h * g_renderer.scale_factor)) / 2;
      g_renderer.window_map.x = window_x;
      g_renderer.window_map.y = window_y;
      g_renderer.unit_map.x = window_x / g_renderer.scale_factor;
      g_renderer.unit_map.y = window_y / g_renderer.scale_factor;
      break;
   }
   
   d_logv(2, "unit_map: %s", d_name_rect(&g_renderer.unit_map));
   d_logv(2, "window_map: %s", d_name_rect(&g_renderer.window_map));
   d_logv(2, "scale_factor: %f", g_renderer.scale_factor);
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

static SDL_Surface* create_layer_surface(bool can_draw_outside) {
   SDL_Surface* surface;
   if (can_draw_outside) {
      int full_w = g_renderer.unit_map.w + (g_renderer.unit_map.x * 2);
      int full_h = g_renderer.unit_map.h + (g_renderer.unit_map.y * 2);
      surface = SDL_CreateRGBSurface(0, full_w, full_h, 8, 0, 0, 0, 0);
   } else {
      surface = SDL_CreateRGBSurface(0, g_renderer.unit_map.w, g_renderer.unit_map.h, 8, 0, 0, 0, 0);
   }
   if (d_dne(surface)) return NULL;
   
   SDL_SetPaletteColors(surface->format->palette, get_palette_colors(), 0, PALETTE_SIZE);
   SDL_SetColorKey(surface, SDL_TRUE, g_renderer.transparent_color_index);
   SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
   SDL_SetSurfaceAlphaMod(surface, 255);

   SDL_FillRect(surface, NULL, g_renderer.transparent_color_index);
   
   return surface;
}

// TODO: preserve what was already drawn when scaling
static void resize_all_surfaces(void) {
   d_log("");
   d_logl("recreating composite surface");
   SDL_FreeSurface(g_renderer.composite_surface);
   g_renderer.composite_surface = SDL_CreateRGBSurfaceWithFormat(
                  0,
                  (g_renderer.unit_map.x * 2) + g_renderer.unit_map.w,
                  (g_renderer.unit_map.y * 2) + g_renderer.unit_map.h,
                  32, SDL_PIXELFORMAT_RGBA8888);
   if (d_dne(g_renderer.composite_surface)) {
      d_err("couldn't recreate composite surface");
   }
   for (int i = 0; i < g_renderer.layer_count; i++) {
      if (i != 0) d_logl(", %d", i);
      else d_logl(" and layer %d", i);
      
      Layer* layer = &g_renderer.layers[i];
      if (layer && layer->surface) {
         SDL_FreeSurface(layer->surface);
      }
      
      layer->surface = create_layer_surface(layer->can_draw_outside_viewport);
      
      if (d_dne(layer->surface)) {
         d_log("new surface doesn't exist");
         return;
      }
      d_logl(" (%dx%d)", layer->surface->w, layer->surface->h);
   }
   d_logl("\n");
   
   g_renderer.window_surface = SDL_GetWindowSurface(g_renderer.window);
   if (d_dne(g_renderer.window_surface)) d_err("can't get widnow surfact haha");
   // d_print_renderer_dims();
   return;
}

// TODO: align rects and pixels with layer size as well (double check pixel alignment)
static void blit_rect(Layer* layer, Rect* rect, uint8_t color_index) {
   /* assumes layer exists and color is in bounds */
   if (rect && layer->can_draw_outside_viewport) {
      rect->x += g_renderer.unit_map.x;
      rect->y += g_renderer.unit_map.y;
   }
   
   SDL_FillRect(layer->surface, rect, color_index);
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
