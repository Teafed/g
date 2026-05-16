/* liren_core.c
 * defers platform details via BackendOps,
 * config interpretation, resolution/mapping, palette, layer initialization
 */

#include "liren_internal.h"
#include "liren_backend_sdl.h"

// palette
// TODO: add support for other palettes
static const ui32 PALETTE_V5[PALETTE_SIZE] = {
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
void liren_init_palette(LiRen* r) {
   if (!r) return;
   for (int i = 0; i < PALETTE_SIZE; ++i) {
      r->palette[i] = PALETTE_V5[i];
   }
}

Layer* liren_find_layer(LiRen* r, LR_LayerHandle handle) {
   if (!r || handle == 0) return NULL;
   for (ui32 i = 0; i < r->layer_count; ++i) {
      if (r->layers[i].handle == handle) {
         return &r->layers[i];
      }
   }
   return NULL;
}

bool liren_ensure_layer_capacity(LiRen* r, ui32 needed) {
   if (!r) return false;
   if (r->layer_capacity >= needed) return true;

   // need more layers
   ui32 new_capacity = (r->layer_capacity == 0) ? 16 : r->layer_capacity;
   while (new_capacity < needed) {
      new_capacity *= 2;
   }

   Layer* new_layers = realloc(r->layers, new_capacity * sizeof(Layer));
   if (!new_layers) {
      liren_log(r, "[liren] failed to grow layer array");
      return false;
   }

   memset(new_layers + r->layer_capacity, 0,
          (new_capacity - r->layer_capacity) * sizeof(Layer));

   r->layers = new_layers;
   r->layer_capacity = new_capacity;
   return true;
}

// helpers
static void set_logical_size(LiRen* r) {
   if (!r) return;
   switch (r->display_res) {
   case LR_RES_VGA:
      r->logical_width = GAME_WIDTH_VGA;
      r->logical_height = GAME_HEIGHT_VGA;
      break;
   case LR_RES_FWVGA:
      r->logical_width = GAME_WIDTH_FWVGA;
      r->logical_height = GAME_HEIGHT_FWVGA;
      break;
   default:
      r->logical_width = GAME_WIDTH_VGA;
      r->logical_height = GAME_HEIGHT_VGA;
      break;
   }
}

static void liren_apply_config_defaults(LR_Config* out_cfg, const LR_Config* in_cfg) {
   LR_Config cfg = {
      .scale_factor = 2.0f,
      .resolution = LR_RES_VGA,
      .resize_mode = LR_RESIZE_FIT,
      .window_mode = LR_WINDOW_WINDOWED,
      .log_fn = NULL,
      .log_user_data = NULL,
   };

   if (in_cfg) cfg = *in_cfg;

   if (cfg.scale_factor < 1.0f) cfg.scale_factor = 1.0f;
   if (cfg.scale_factor > 20.0f) cfg.scale_factor = 20.0f;

   *out_cfg = cfg;
}

// PUBLIC API
LiRen* lr_create(const LR_Config* user_cfg) {
   // config / logical viewport
   LR_Config cfg;
   liren_apply_config_defaults(&cfg, user_cfg);

   LiRen* r = (LiRen*)calloc(1, sizeof(LiRen));
   if (!r) {
      return NULL;
   }

   r->initialized = false;
   r->scale_factor = cfg.scale_factor;
   r->display_res = cfg.resolution;
   r->resize_mode = cfg.resize_mode;
   r->window_mode = cfg.window_mode;

   r->log_fn = cfg.log_fn;
   r->log_user_data = cfg.log_user_data;

   set_logical_size(r);

   // palette
   liren_init_palette(r);
   r->clear_color_index = 4; // mono-black
   r->transparent_color_index = PALETTE_TRANSPARENT;

   // layers
   r->layer_capacity = 16; // start with space for 16 layers
   r->layers = (Layer*)calloc(r->layer_capacity, sizeof(Layer));
   if (!r->layers) {
      free(r);
      return NULL;
   }
   r->layer_count = 0;
   r->next_layer_handle = 1;

   // fonts / sprites (allocated lazily; capacity starts at 0)
   r->fonts = NULL;
   r->font_count = 0;
   r->font_capacity = 0;
   r->next_font_handle = 1;

   r->sprites = NULL;
   r->sprite_count = 0;
   r->sprite_capacity = 0;
   r->next_sprite_handle = 1;

   // system overlay layer
   r->system_layer_handle = 0;
   for (int i = 0; i < SYS_MAX; ++i) {
      r->system_layer_flags[i] = false;
   }
   
    // backend
    // temp: for now always SDL
    r->backend = &LIREN_BACKEND_SDL;
    r->backend_data = NULL;

    if (!r->backend || !r->backend->init) {
        liren_log(r, "liren: backend missing or invalid");
        lr_destroy(r);
        return NULL;
    }

    if (!r->backend->init(r, &cfg)) {
        // backend_init should log details
        lr_destroy(r);
        return NULL;
    }

   r->initialized = true;
   return r;
}

void lr_destroy(LiRen* r) {
   if (!r) return;

   // free assets (font/sprite pixel buffers)
   liren_destroy_all_assets(r);

   // backend cleans up platform resources
   if (r->backend && r->backend->shutdown) {
      r->backend->shutdown(r);
   }

   if (r->layers) {
      free(r->layers);
      r->layers = NULL;
   }
   free(r);
}

void lr_begin_frame(LiRen* r) {
   if (!r || !r->initialized) return;

   if (r->backend && r->backend->begin_frame) {
      r->backend->begin_frame(r);
   }
}

void lr_end_frame(LiRen* r) {
  if (!r || !r->initialized) return;

   if (r->backend && r->backend->present) {
      r->backend->present(r);
   }

   // other logic here? like drawing layers in a certain order
}

void lr_set_clear_color(LiRen* r, uint8_t color_index) {
   if (!r || !r->initialized) return;
   if (color_index >= PALETTE_SIZE) return;
   r->clear_color_index = color_index;
}

void lr_clear(LiRen* r) {
   if (!r || !r->initialized) return;
   if (!r->backend || !r->backend->clear_layer) return;

   for (ui32 i = 0; i < r->layer_count; ++i) {
      Layer* layer = &r->layers[i];
      r->backend->clear_layer(r, layer, r->clear_color_index);
   }
}

const LR_Framebuffer* lr_get_framebuffer(LiRen* r) {
   if (!r || !r->initialized) return NULL;

   if (r->backend && r->backend->update_framebuffer_view) {
      r->backend->update_framebuffer_view(r, &r->framebuffer);
   }

   return &r->framebuffer;
}

LR_LayerHandle lr_layer_create(LiRen* r, bool can_draw_outside) {
   if (!r || !r->initialized) return 0;

   ui32 needed = r->layer_count + 1;
   if (!liren_ensure_layer_capacity(r, needed)) {
      return 0;
   }

   Layer* layer = &r->layers[r->layer_count];
   memset(layer, 0, sizeof(*layer));

   layer->handle                     = r->next_layer_handle++;
   layer->can_draw_outside_viewport  = can_draw_outside;
   layer->visible                    = true;
   layer->opacity                    = 255;
   layer->size                       = 2;   // default pixel size

   // let backend create its surface / texture
   if (r->backend && r->backend->create_layer) {
      if (!r->backend->create_layer(r, layer)) {
         // backend failed; don't bump layer_count, reset struct
         memset(layer, 0, sizeof(*layer));
         return 0;
      }
   }

   r->layer_count++;
   return layer->handle;
}

void lr_layer_destroy(LiRen* r, LR_LayerHandle handle) {
   if (!r || !r->initialized || handle == 0) return;

   // find index
   int index = -1;
   for (ui32 i = 0; i < r->layer_count; ++i) {
      if (r->layers[i].handle == handle) {
         index = (int)i;
         break;
      }
   }
   if (index < 0) {
      return;  // invalid handle
   }

   Layer* layer = &r->layers[index];

   // let backend free its resources
   if (r->backend && r->backend->destroy_layer) {
      r->backend->destroy_layer(r, layer);
   }

   // shift remaining layers down
   for (ui32 i = (ui32)index; i + 1 < r->layer_count; ++i) {
      r->layers[i] = r->layers[i + 1];
   }

   r->layer_count--;

   // clear the last slot (now unused)
   if (r->layer_count > 0) {
      memset(&r->layers[r->layer_count], 0, sizeof(Layer));
   }
}

void lr_layer_set_visible(LiRen* r, LR_LayerHandle handle, bool visible) {
   if (!r || !r->initialized) return;
   Layer* layer = liren_find_layer(r, handle);
   if (!layer) return;
   layer->visible = visible;
}

void lr_layer_set_opacity(LiRen* r, LR_LayerHandle handle, uint8_t opacity) {
   if (!r || !r->initialized) return;
   Layer* layer = liren_find_layer(r, handle);
   if (!layer) return;
   layer->opacity = opacity;
}

void lr_layer_set_size(LiRen* r, LR_LayerHandle handle, uint8_t size) {
   if (!r || !r->initialized) return;
   if (size == 0) return;
   Layer* layer = liren_find_layer(r, handle);
   if (!layer) return;
   layer->size = size;
}

void lr_draw_rect(LiRen* r, LR_LayerHandle handle,
                  LR_Rect rect, uint8_t color_index) {
   if (!r || !r->initialized) return;
   if (color_index >= PALETTE_SIZE) return;
   if (!r->backend || !r->backend->fill_rect) return;

   Layer* layer = liren_find_layer(r, handle);
   if (!layer || !layer->backend_layer) return;

   // TODO: apply layer->size, clipping, can_draw_outside_viewport, etc.
   r->backend->fill_rect(r, layer, &rect, color_index);
}

void lr_draw_fill(LiRen* r, LR_LayerHandle handle,
                  uint8_t color_index) {
   if (!r || !r->initialized) return;
   if (color_index >= PALETTE_SIZE) return;
   if (!r->backend || !r->backend->fill_rect) return;

   Layer* layer = liren_find_layer(r, handle);
   if (!layer || !layer->backend_layer) return;

   // NULL rect means "fill the whole layer" per our backend contract
   r->backend->fill_rect(r, layer, NULL, color_index);
}

void lr_draw_pixel(LiRen* r, LR_LayerHandle handle,
                   int x, int y, uint8_t color_index) {
   if (!r || !r->initialized) return;
   if (color_index >= PALETTE_SIZE) return;
   if (!r->backend || !r->backend->fill_rect) return;

   Layer* layer = liren_find_layer(r, handle);
   if (!layer || !layer->backend_layer) return;

   // respect layer->size as a pixel “block” size
   int size = (layer->size > 0) ? layer->size : 1;

   LR_Rect rect;
   rect.x = x;
   rect.y = y;
   rect.w = size;
   rect.h = size;

   r->backend->fill_rect(r, layer, &rect, color_index);
}

void lr_set_display_resolution(LiRen* r, LR_DisplayResolution res) {
   if (!r) return;
   r->display_res = res;
}

LR_DisplayResolution lr_get_display_resolution(LiRen* r) {
   if (!r) return LR_RES_VGA;
   return r->display_res;
}

void lr_set_resize_mode(LiRen* r, LR_ResizeMode resize_mode) {
   if (!r) return;
   r->resize_mode = resize_mode;
}

LR_ResizeMode lr_get_resize_mode(LiRen* r) {
   if (!r) return LR_RESIZE_FIT;
   return r->resize_mode;
}

void lr_set_window_mode(LiRen* r, LR_WindowMode window_mode) {
   if (!r) return;
   r->window_mode = window_mode;
}

LR_WindowMode lr_get_window_mode(LiRen* r) {
   if (!r) return LR_WINDOW_WINDOWED;
   return r->window_mode;
}

void lr_get_viewport_size(LiRen* r, int* width, int* height) {
   if (!r || !r->initialized) {
      if (width)  *width  = 0;
      if (height) *height = 0;
      return;
   }
   if (width)  *width  = r->logical_width;
   if (height) *height = r->logical_height;
}

bool lr_point_in_viewport(LiRen* r, int x, int y) {
   if (!r || !r->initialized) return false;
   return x >= 0 && y >= 0
       && x < r->logical_width
       && y < r->logical_height;
}

void lr_enable_system_overlay(LiRen* r) {
   // TODO: implement system overlay (fps, layer count, etc).
   // stubbed so the public API symbol exists.
   (void)r;
}

void lr_on_window_resized(LiRen* r, int window_w, int window_h) {
   if (!r || !r->initialized) return;
   if (!r->backend || !r->backend->resize) return;

   r->backend->resize(r, window_w, window_h);
}

const uint32_t* lr_get_palette(LiRen* r, int* out_size) {
   if (!r) {
      if (out_size) *out_size = 0;
      return NULL;
   }
   if (out_size) *out_size = PALETTE_SIZE;
   return r->palette;
}

uint8_t lr_get_transparent_index(LiRen* r) {
   if (!r) return PALETTE_TRANSPARENT;
   return r->transparent_color_index;
}
