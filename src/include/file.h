#ifndef FILE_H
#define FILE_H

#include <stdint.h>
#include <stdio.h>

#define DIR_SHEETS "assets/sheets/" // includes fonts, sprites, and other tile textures

typedef struct { // TODO: make sure the data is 8-bit indexed colors
   uint32_t size;
   int32_t width;
   int32_t height;
   uint8_t data[];
} ImageData;

// TODO: handle if the sheet doesn't exist
         // also maybe i don't need to specify dims in enum name idk
typedef enum {
   FONT_ACER_8_8,
   FONT_COMPIS_8_16,
   FONT_MASTER_8_8,
   FONT_MASTER_8_8_BOLD,
   FONT_SHARP_8_8,
   FONT_VERITE_8_8,
   FONT_VERITE_9_8,
   FONT_MAX
} FontType;
#define FONT_DEFAULT FONT_ACER_8_8

typedef struct {
   FontType type; // TODO: add to file_load_sheets 
   const char* fname;
   int tile_w;
   int tile_h;
   int image_w;
   int image_h;
   ImageData* data;
   int ascii_start;
} Font;

typedef struct {
   Font* fonts;
   int font_count;
   int font_capacity;
} FontArray;

typedef struct {
   const char* fname;
   int tile_w;
   int tile_h;
   int image_w;
   int image_h;
   ImageData* data;
   char name[128]; // parsed name from filename (e.g., "guy-run")
} Sprite;

typedef struct {
   Sprite* sprites;
   int sprite_count;
   int sprite_capacity;
} SpriteArray;

int file_load_sheets(FontArray* fonts, SpriteArray* sprites); // called once in renderer_init(), returns 0 on failure
void file_unload_sheets(FontArray* fonts, SpriteArray* sprites);
Font* file_get_font(FontArray* font_array, FontType type);
Sprite* file_get_sprite(SpriteArray* sprites, const char* sprite_name);

#endif