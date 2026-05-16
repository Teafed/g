/* game_assets.c
 * scans a sheets directory, loads BMPs, converts to palette-indexed pixels,
 * and registers fonts/sprites with liren.
 *
 * filename convention: <type>_<name>_<W>x<H>.bmp
 *   type  = "font" or "sprite"
 *   name  = arbitrary identifier (no underscores)
 *   W x H = tile / frame dimensions in pixels
 *
 * for fonts, the per-glyph size is (W, H), and the sheet is assumed to be a
 * grid of glyphs starting at ascii 33 ('!'). for sprites, (W, H) is the size
 * of a single frame, and frame_count is derived from the sheet's tile grid.
 */

#include "game_assets.h"
#include "file.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
   #include <windows.h>
#else
   #include <dirent.h>
   #include <strings.h>   // strcasecmp
#endif

// for the default font glyph range. ascii 33 is '!'; printable ascii goes
// up through 126 ('~'), so 94 glyphs in a sheet covers most practical fonts.
#define DEFAULT_FONT_ASCII_START 33

// ASSET RECORDS
typedef struct {
   char            name[64];
   LR_FontHandle   handle;
} FontEntry;

typedef struct {
   char            name[64];
   LR_SpriteHandle handle;
} SpriteEntry;

struct GameAssets {
   LiRen* renderer;

   FontEntry*   fonts;
   int          font_count;
   int          font_capacity;

   SpriteEntry* sprites;
   int          sprite_count;
   int          sprite_capacity;
};

// PALETTE CONVERSION
// "log once per unique color" tracker for unmatched pixels during one
// conversion pass. small fixed-size dedup; if we overflow we just stop logging.
#define MAX_LOGGED_BAD_COLORS 16

typedef struct {
   uint32_t colors[MAX_LOGGED_BAD_COLORS];
   int      count;
} BadColorLog;

static bool bad_color_seen(BadColorLog* log, uint32_t rgba) {
   for (int i = 0; i < log->count; ++i) {
      if (log->colors[i] == rgba) return true;
   }
   if (log->count < MAX_LOGGED_BAD_COLORS) {
      log->colors[log->count++] = rgba;
   }
   return false;
}

// converts an RGBA pixel buffer to a palette-indexed buffer using strict matching.
// any pixel not present in the palette is logged once and emitted as the
// transparent index. returns a newly-allocated buffer the caller must free,
// or NULL on allocation failure.
static uint8_t* rgba_to_indexed(LiRen* r,
                                const uint8_t* rgba, int width, int height,
                                const char* source_name) {
   int palette_size = 0;
   const uint32_t* palette = lr_get_palette(r, &palette_size);
   uint8_t transparent = lr_get_transparent_index(r);
   if (!palette || palette_size <= 0) return NULL;

   size_t count = (size_t)width * (size_t)height;
   uint8_t* out = malloc(count);
   if (!out) {
      d_err("out of memory converting %s to indexed", source_name);
      return NULL;
   }

   BadColorLog bad = { .count = 0 };
   int bad_pixel_count = 0;

   for (size_t i = 0; i < count; ++i) {
      // pack source as 0xRRGGBBAA to match palette format
      uint32_t r8 = rgba[i * 4 + 0];
      uint32_t g8 = rgba[i * 4 + 1];
      uint32_t b8 = rgba[i * 4 + 2];
      uint32_t a8 = rgba[i * 4 + 3];
      uint32_t key = (r8 << 24) | (g8 << 16) | (b8 << 8) | a8;

      int found = -1;
      for (int p = 0; p < palette_size; ++p) {
         if (palette[p] == key) { found = p; break; }
      }

      if (found >= 0) {
         out[i] = (uint8_t)found;
      } else {
         out[i] = transparent;
         bad_pixel_count++;
         if (!bad_color_seen(&bad, key)) {
            d_err("%s: color 0x%08X not in palette (becoming transparent)",
                  source_name, key);
         }
      }
   }

   if (bad_pixel_count > 0) {
      d_logv(1, "%s: %d pixel(s) used %d unmatched color(s)",
             source_name, bad_pixel_count, bad.count);
   }
   return out;
}

// CAPACITY HELPERS
static bool ensure_font_capacity(GameAssets* a, int needed) {
   if (a->font_capacity >= needed) return true;
   int cap = a->font_capacity ? a->font_capacity : 8;
   while (cap < needed) cap *= 2;
   FontEntry* nf = realloc(a->fonts, sizeof(FontEntry) * cap);
   if (!nf) return false;
   a->fonts = nf;
   a->font_capacity = cap;
   return true;
}

static bool ensure_sprite_capacity(GameAssets* a, int needed) {
   if (a->sprite_capacity >= needed) return true;
   int cap = a->sprite_capacity ? a->sprite_capacity : 16;
   while (cap < needed) cap *= 2;
   SpriteEntry* ns = realloc(a->sprites, sizeof(SpriteEntry) * cap);
   if (!ns) return false;
   a->sprites = ns;
   a->sprite_capacity = cap;
   return true;
}

// FILENAME PARSING
// expects "<type>_<name>_<W>x<H>.bmp". writes into the caller's buffers.
// returns true on success.
static bool parse_sheet_filename(const char* fname,
                                 char* out_type, size_t type_cap,
                                 char* out_name, size_t name_cap,
                                 int* out_w, int* out_h) {
   // work on a mutable copy
   char buf[256];
   size_t flen = strlen(fname);
   if (flen >= sizeof(buf)) return false;
   memcpy(buf, fname, flen + 1);

   char* type = strtok(buf, "_");
   if (!type) return false;
   char* name = strtok(NULL, "_");
   if (!name) return false;
   char* dims = strtok(NULL, ".");
   if (!dims) return false;

   if (sscanf(dims, "%dx%d", out_w, out_h) != 2) return false;
   if (*out_w <= 0 || *out_h <= 0) return false;

   if (strlen(type) >= type_cap || strlen(name) >= name_cap) return false;
   strcpy(out_type, type);
   strcpy(out_name, name);
   return true;
}

static bool has_bmp_extension(const char* fname) {
   size_t n = strlen(fname);
   if (n < 4) return false;
#ifdef _WIN32
   return _stricmp(fname + n - 4, ".bmp") == 0;
#else
   return strcasecmp(fname + n - 4, ".bmp") == 0;
#endif
}

// SHEET PROCESSING
static void process_sheet(GameAssets* a, const char* dir, const char* fname) {
   char type[32], name[64];
   int tile_w = 0, tile_h = 0;
   if (!parse_sheet_filename(fname, type, sizeof type, name, sizeof name,
                             &tile_w, &tile_h)) {
      d_log("skipping: couldn't parse filename: %s", fname);
      return;
   }

   char full_path[512];
   snprintf(full_path, sizeof full_path, "%s%s", dir, fname);

   ImageData* img = image_load_bmp(full_path);
   if (!img) {
      d_log("skipping: failed to load bitmap: %s", fname);
      return;
   }

   if (img->width % tile_w != 0 || img->height % tile_h != 0) {
      d_log("WARNING: %s has partial tiles; truncating",
            fname);
   }
   int cols = img->width  / tile_w;
   int rows = img->height / tile_h;
   int tile_count = cols * rows;

   uint8_t* indexed = rgba_to_indexed(a->renderer, img->pixels,
                                      img->width, img->height, fname);
   image_free(img);
   if (!indexed) return;

   if (strcmp(type, "font") == 0) {
      LR_BitmapFontDesc desc = {
         .width       = cols * tile_w,
         .height      = rows * tile_h,
         .glyph_w     = tile_w,
         .glyph_h     = tile_h,
         .ascii_start = DEFAULT_FONT_ASCII_START,
         .glyph_count = tile_count,
         .pixels      = indexed,
      };
      LR_FontHandle h = lr_font_create_from_pixels(a->renderer, &desc);
      free(indexed);   // liren copied the buffer; release ours

      if (h == LR_INVALID_FONT) {
         d_err("liren rejected font: %s", fname);
         return;
      }
      if (!ensure_font_capacity(a, a->font_count + 1)) {
         d_err("out of memory adding font: %s", fname);
         lr_font_destroy(a->renderer, h);
         return;
      }
      FontEntry* e = &a->fonts[a->font_count++];
      strncpy(e->name, name, sizeof e->name - 1);
      e->name[sizeof e->name - 1] = '\0';
      e->handle = h;
      d_logv(2, "loaded font \"%s\" (%dx%d, %d glyphs)",
             name, tile_w, tile_h, tile_count);

   } else if (strcmp(type, "sprite") == 0) {
      LR_SpriteDesc desc = {
         .width       = cols * tile_w,
         .height      = rows * tile_h,
         .frame_w     = tile_w,
         .frame_h     = tile_h,
         .frame_count = tile_count,
         .pixels      = indexed,
      };
      LR_SpriteHandle h = lr_sprite_create_from_pixels(a->renderer, &desc);
      free(indexed);

      if (h == LR_INVALID_SPRITE) {
         d_err("liren rejected sprite: %s", fname);
         return;
      }
      if (!ensure_sprite_capacity(a, a->sprite_count + 1)) {
         d_err("out of memory adding sprite: %s", fname);
         lr_sprite_destroy(a->renderer, h);
         return;
      }
      SpriteEntry* e = &a->sprites[a->sprite_count++];
      strncpy(e->name, name, sizeof e->name - 1);
      e->name[sizeof e->name - 1] = '\0';
      e->handle = h;
      d_logv(2, "loaded sprite \"%s\" (%dx%d, %d frame(s))",
             name, tile_w, tile_h, tile_count);

   } else {
      d_log("unknown sheet type \"%s\" in %s", type, fname);
      free(indexed);
   }
}

// PUBLIC API
GameAssets* game_assets_load(LiRen* r, const char* sheets_dir) {
   if (!r || !sheets_dir) return NULL;

   GameAssets* a = calloc(1, sizeof(*a));
   if (!a) return NULL;
   a->renderer = r;

#ifdef _WIN32
   WIN32_FIND_DATA find;
   char pattern[512];
   snprintf(pattern, sizeof pattern, "%s*.bmp", sheets_dir);
   HANDLE h = FindFirstFile(pattern, &find);
   if (h == INVALID_HANDLE_VALUE) {
      d_err("no bitmap files found in %s", sheets_dir);
      game_assets_destroy(a);
      return NULL;
   }
   do {
      if (has_bmp_extension(find.cFileName)) {
         process_sheet(a, sheets_dir, find.cFileName);
      }
   } while (FindNextFile(h, &find));
   FindClose(h);
#else
   DIR* dir = opendir(sheets_dir);
   if (!dir) {
      d_err("could not open directory: %s", sheets_dir);
      game_assets_destroy(a);
      return NULL;
   }
   struct dirent* ent;
   while ((ent = readdir(dir)) != NULL) {
      if (has_bmp_extension(ent->d_name)) {
         process_sheet(a, sheets_dir, ent->d_name);
      }
   }
   closedir(dir);
#endif

   d_logv(2, "loaded %d font(s), %d sprite(s) from %s",
          a->font_count, a->sprite_count, sheets_dir);
   return a;
}

void game_assets_destroy(GameAssets* a) {
   if (!a) return;
   // liren owns the pixel buffers and will free them on lr_destroy, but
   // explicitly destroying the handles keeps liren's tables clean even if
   // the caller destroys assets before the renderer.
   for (int i = 0; i < a->font_count; ++i) {
      lr_font_destroy(a->renderer, a->fonts[i].handle);
   }
   for (int i = 0; i < a->sprite_count; ++i) {
      lr_sprite_destroy(a->renderer, a->sprites[i].handle);
   }
   free(a->fonts);
   free(a->sprites);
   free(a);
}

LR_FontHandle game_assets_get_font(GameAssets* a, const char* name) {
   if (!a || !name) return LR_INVALID_FONT;
   for (int i = 0; i < a->font_count; ++i) {
      if (strcmp(a->fonts[i].name, name) == 0) return a->fonts[i].handle;
   }
   return LR_INVALID_FONT;
}

LR_SpriteHandle game_assets_get_sprite(GameAssets* a, const char* name) {
   if (!a || !name) return LR_INVALID_SPRITE;
   for (int i = 0; i < a->sprite_count; ++i) {
      if (strcmp(a->sprites[i].name, name) == 0) return a->sprites[i].handle;
   }
   return LR_INVALID_SPRITE;
}
