/* liren_assets.c
 * font and sprite management; both are palette-indexed pixel buffers
 * stored in liren-owned memory. drawing routes through backend->blit_indexed.
 */

#include "liren_internal.h"

// LOOKUP HELPERS
Font* liren_find_font(LiRen* r, LR_FontHandle handle) {
   if (!r || handle == 0) return NULL;
   for (ui32 i = 0; i < r->font_count; ++i) {
      if (r->fonts[i].handle == handle) return &r->fonts[i];
   }
   return NULL;
}

Sprite* liren_find_sprite(LiRen* r, LR_SpriteHandle handle) {
   if (!r || handle == 0) return NULL;
   for (ui32 i = 0; i < r->sprite_count; ++i) {
      if (r->sprites[i].handle == handle) return &r->sprites[i];
   }
   return NULL;
}

// CAPACITY HELPERS
static bool ensure_font_capacity(LiRen* r, ui32 needed) {
   if (r->font_capacity >= needed) return true;
   ui32 new_cap = (r->font_capacity == 0) ? 8 : r->font_capacity;
   while (new_cap < needed) new_cap *= 2;
   Font* nf = realloc(r->fonts, new_cap * sizeof(Font));
   if (!nf) {
      liren_log(r, "[liren] failed to grow font array");
      return false;
   }
   memset(nf + r->font_capacity, 0, (new_cap - r->font_capacity) * sizeof(Font));
   r->fonts = nf;
   r->font_capacity = new_cap;
   return true;
}

static bool ensure_sprite_capacity(LiRen* r, ui32 needed) {
   if (r->sprite_capacity >= needed) return true;
   ui32 new_cap = (r->sprite_capacity == 0) ? 8 : r->sprite_capacity;
   while (new_cap < needed) new_cap *= 2;
   Sprite* ns = realloc(r->sprites, new_cap * sizeof(Sprite));
   if (!ns) {
      liren_log(r, "[liren] failed to grow sprite array");
      return false;
   }
   memset(ns + r->sprite_capacity, 0,
          (new_cap - r->sprite_capacity) * sizeof(Sprite));
   r->sprites = ns;
   r->sprite_capacity = new_cap;
   return true;
}

// FONTS
LR_FontHandle lr_font_create_from_pixels(LiRen* r, const LR_BitmapFontDesc* desc) {
   if (!r || !r->initialized || !desc || !desc->pixels) return LR_INVALID_FONT;
   if (desc->width <= 0 || desc->height <= 0)            return LR_INVALID_FONT;
   if (desc->glyph_w <= 0 || desc->glyph_h <= 0)         return LR_INVALID_FONT;
   if (desc->glyph_count <= 0)                            return LR_INVALID_FONT;

   if (!ensure_font_capacity(r, r->font_count + 1)) return LR_INVALID_FONT;

   // copy pixel buffer so caller can free theirs
   size_t bytes = (size_t)desc->width * (size_t)desc->height;
   ui8* pixels = (ui8*)malloc(bytes);
   if (!pixels) {
      liren_log(r, "[liren] out of memory copying font pixels");
      return LR_INVALID_FONT;
   }
   memcpy(pixels, desc->pixels, bytes);

   Font* f = &r->fonts[r->font_count];
   f->handle      = r->next_font_handle++;
   f->width       = desc->width;
   f->height      = desc->height;
   f->glyph_w     = desc->glyph_w;
   f->glyph_h     = desc->glyph_h;
   f->ascii_start = desc->ascii_start;
   f->glyph_count = desc->glyph_count;
   f->pixels      = pixels;

   r->font_count++;
   return f->handle;
}

void lr_font_destroy(LiRen* r, LR_FontHandle handle) {
   if (!r || !r->initialized || handle == 0) return;

   int index = -1;
   for (ui32 i = 0; i < r->font_count; ++i) {
      if (r->fonts[i].handle == handle) { index = (int)i; break; }
   }
   if (index < 0) return;

   free(r->fonts[index].pixels);
   for (ui32 i = (ui32)index; i + 1 < r->font_count; ++i) {
      r->fonts[i] = r->fonts[i + 1];
   }
   r->font_count--;
   if (r->font_count < r->font_capacity) {
      memset(&r->fonts[r->font_count], 0, sizeof(Font));
   }
}

void lr_draw_text(LiRen* r, LR_LayerHandle layer_handle, LR_FontHandle font_handle,
                  int x, int y, uint8_t color_index, const char* text) {
   if (!r || !r->initialized || !text) return;
   if (color_index >= r->palette_size) return;
   if (!r->backend || !r->backend->blit_indexed) return;

   Layer* layer = liren_find_layer(r, layer_handle);
   if (!layer || !layer->backend_layer) return;

   Font* f = liren_find_font(r, font_handle);
   if (!f) return;

   // glyphs are laid out left-to-right, top-to-bottom in the sheet.
   int cols = f->width / f->glyph_w;
   if (cols <= 0) return;

   int pen_x = x;
   for (const char* p = text; *p; ++p) {
      char c = *p;

      // simple newline support: drop a line, reset to start x.
      if (c == '\n') {
         pen_x = x;
         y += f->glyph_h;
         continue;
      }

      int idx = (int)(unsigned char)c - f->ascii_start;
      if (idx < 0 || idx >= f->glyph_count) {
         // unknown glyph; skip a cell width
         pen_x += f->glyph_w;
         continue;
      }

      int src_x = (idx % cols) * f->glyph_w;
      int src_y = (idx / cols) * f->glyph_h;

      r->backend->blit_indexed(
         r, layer,
         f->pixels, f->width, f->height,
         src_x, src_y,
         f->glyph_w, f->glyph_h,
         pen_x, y,
         color_index,                  // tint: all non-transparent => color_index
         r->transparent_color_index
      );

      pen_x += f->glyph_w;
   }
}

// SPRITES
LR_SpriteHandle lr_sprite_create_from_pixels(LiRen* r, const LR_SpriteDesc* desc) {
   if (!r || !r->initialized || !desc || !desc->pixels) return LR_INVALID_SPRITE;
   if (desc->width <= 0 || desc->height <= 0)            return LR_INVALID_SPRITE;
   if (desc->frame_w <= 0 || desc->frame_h <= 0)         return LR_INVALID_SPRITE;
   if (desc->frame_count <= 0)                            return LR_INVALID_SPRITE;

   if (!ensure_sprite_capacity(r, r->sprite_count + 1)) return LR_INVALID_SPRITE;

   size_t bytes = (size_t)desc->width * (size_t)desc->height;
   ui8* pixels = (ui8*)malloc(bytes);
   if (!pixels) {
      liren_log(r, "[liren] out of memory copying sprite pixels");
      return LR_INVALID_SPRITE;
   }
   memcpy(pixels, desc->pixels, bytes);

   Sprite* s = &r->sprites[r->sprite_count];
   s->handle      = r->next_sprite_handle++;
   s->width       = desc->width;
   s->height      = desc->height;
   s->frame_w     = desc->frame_w;
   s->frame_h     = desc->frame_h;
   s->frame_count = desc->frame_count;
   s->pixels      = pixels;

   r->sprite_count++;
   return s->handle;
}

void lr_sprite_destroy(LiRen* r, LR_SpriteHandle handle) {
   if (!r || !r->initialized || handle == 0) return;

   int index = -1;
   for (ui32 i = 0; i < r->sprite_count; ++i) {
      if (r->sprites[i].handle == handle) { index = (int)i; break; }
   }
   if (index < 0) return;

   free(r->sprites[index].pixels);
   for (ui32 i = (ui32)index; i + 1 < r->sprite_count; ++i) {
      r->sprites[i] = r->sprites[i + 1];
   }
   r->sprite_count--;
   if (r->sprite_count < r->sprite_capacity) {
      memset(&r->sprites[r->sprite_count], 0, sizeof(Sprite));
   }
}

void lr_draw_sprite(LiRen* r, LR_LayerHandle layer_handle, LR_SpriteHandle sprite_handle,
                    int x, int y, int frame_index) {
   if (!r || !r->initialized) return;
   if (!r->backend || !r->backend->blit_indexed) return;

   Layer* layer = liren_find_layer(r, layer_handle);
   if (!layer || !layer->backend_layer) return;

   Sprite* s = liren_find_sprite(r, sprite_handle);
   if (!s) return;

   // clamp frame index
   if (frame_index < 0) frame_index = 0;
   if (frame_index >= s->frame_count) frame_index = s->frame_count - 1;

   int cols = s->width / s->frame_w;
   if (cols <= 0) return;

   int src_x = (frame_index % cols) * s->frame_w;
   int src_y = (frame_index / cols) * s->frame_h;

   r->backend->blit_indexed(
      r, layer,
      s->pixels, s->width, s->height,
      src_x, src_y,
      s->frame_w, s->frame_h,
      x, y,
      LIREN_TINT_NONE,                 // no tint, draw as-is
      r->transparent_color_index
   );
}

// BULK CLEANUP
void liren_destroy_all_assets(LiRen* r) {
   if (!r) return;

   for (ui32 i = 0; i < r->font_count; ++i) {
      free(r->fonts[i].pixels);
   }
   free(r->fonts);
   r->fonts = NULL;
   r->font_count = 0;
   r->font_capacity = 0;

   for (ui32 i = 0; i < r->sprite_count; ++i) {
      free(r->sprites[i].pixels);
   }
   free(r->sprites);
   r->sprites = NULL;
   r->sprite_count = 0;
   r->sprite_capacity = 0;
}
