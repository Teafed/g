#include "file.h"
#include "renderer.h" // for palette
#include "debug.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
   #include <windows.h>
#else
   #include <dirent.h>
   #include <sys/stat.h>
#endif

static ImageData* load_bitmap(const char* fname);
static int parse_sheet_filename(const char* fname, char* type, char* name, int* tile_w, int* tile_h);
static void verify_image_colors(ImageData* image_data);

int file_load_sheets(SpriteArray* sprites, FontArray* fonts) {
   if (!sprites || !fonts) {
      d_log("sprites or fonts do not exist");
      return 0;
   }   

   // initialize arrays
   sprites->sprites = malloc(sizeof(Sprite) * 16);
   sprites->sprite_count = 0;
   sprites->sprite_capacity = 16;
   if (d_dne(sprites->sprites)) {
      return 0;
   }
   
   fonts->fonts = malloc(sizeof(Font) * 8);
   fonts->font_count = 0;
   fonts->font_capacity = 8;
   if (d_dne(fonts->fonts)) {
      free(fonts->fonts);
      return 0;
   }
   
#ifdef _WIN32
   WIN32_FIND_DATA find_data;
   HANDLE find_handle;
   char search_path[256];
   snprintf(search_path, sizeof(search_path), "%s*.bmp", DIR_SHEETS);
   
   find_handle = FindFirstFile(search_path, &find_data);
   if (find_handle == INVALID_HANDLE_VALUE) {
      printf("no bitmap files found in %s\n", DIR_SHEETS);
      free(sprites->sprites);
      free(fonts->fonts);
      return 0;
   }
   
   do {
      char full_path[512];
      snprintf(full_path, sizeof(full_path), "%s%s", DIR_SHEETS, find_data.cFileName);
      
      // parse filename
      char type[64], name[128];
      int tile_w, tile_h;
      
      if (parse_sheet_filename(find_data.cFileName, type, name, &tile_w, &tile_h)) {
         ImageData* image_data = load_bitmap(full_path);
         if (!image_data) {
            d_log("failed to load bitmap: %s", find_data.cFileName);
            continue;
         }

         // calculate image dimensions in tiles
         int image_w = image_data->width / tile_w;
         int image_h = image_data->height / tile_h;
         
         // check for partial tiles and warn
         if (image_data->width % tile_w != 0 || image_data->height % tile_h != 0) {
            d_log("WARNING: %s has partial tiles - truncating to %dx%d tiles", 
                   find_data.cFileName, image_w, image_h);
            // TODO: modify ImageData to remove partial tiles
         }

         verify_image_colors(image_data); // TODO: finish this pls
         
         // determine resource type and add to appropriate array
         if (strcmp(type, "font") == 0) {
            // resize font array if needed
            if (fonts->font_count >= fonts->font_capacity) {
               fonts->font_capacity *= 2;
               Font* new_fonts = realloc(fonts->fonts, sizeof(Font) * fonts->font_capacity);
               if (!new_fonts) {
                  free(image_data);
                  continue;
               }
               fonts->fonts = new_fonts;
            }
            
            Font* font = &fonts->fonts[fonts->font_count];
            font->fname = malloc(strlen(find_data.cFileName) + 1);
            if (!font->fname) {
               free(image_data);
               continue;
            }
            strcpy((char*)font->fname, find_data.cFileName);
            font->tile_w = tile_w;
            font->tile_h = tile_h;
            font->image_w = image_w;
            font->image_h = image_h;
            font->data = image_data;
            font->ascii_start = 33; // fonts start at '!'
            
            fonts->font_count++;
            
         } else if (strcmp(type, "sprite") == 0) {
            // resize sprite array if needed
            if (sprites->sprite_count >= sprites->sprite_capacity) {
               sprites->sprite_capacity *= 2;
               Sprite* new_sprites = realloc(sprites->sprites, sizeof(Sprite) * sprites->sprite_capacity);
               if (!new_sprites) {
                  free(image_data);
                  continue;
               }
               sprites->sprites = new_sprites;
            }
            
            Sprite* sprite = &sprites->sprites[sprites->sprite_count];
            sprite->fname = malloc(strlen(find_data.cFileName) + 1);
            if (!sprite->fname) {
               free(image_data);
               continue;
            }
            strcpy((char*)sprite->fname, find_data.cFileName);
            strcpy(sprite->name, name); // store the parsed name for lookups
            sprite->tile_w = tile_w;
            sprite->tile_h = tile_h;
            sprite->image_w = image_w;
            sprite->image_h = image_h;
            sprite->data = image_data;
            
            sprites->sprite_count++;
            
         } else {
            d_log("unknown resource type '%s' in file %s", type, find_data.cFileName);
            free(image_data);
         }
      }
      
   } while (FindNextFile(find_handle, &find_data));
   
   FindClose(find_handle);
   
#else
   DIR* dir = opendir(DIR_SHEETS);
   if (d_dne(dir)) {
      d_log("could not open directory: %s", DIR_SHEETS);
      free(sprites->sprites);
      free(fonts->fonts);
      return 0;
   }
   
   struct dirent* entry;
   while ((entry = readdir(dir)) != NULL) {
      // skip non-bmp files
      if (strstr(entry->d_name, ".bmp") == NULL) continue;
      
      char full_path[512];
      snprintf(full_path, sizeof(full_path), "%s%s", DIR_SHEETS, entry->d_name);
      
      // parse filename
      char type[64], name[128];
      int tile_w, tile_h;
      
      if (parse_sheet_filename(entry->d_name, type, name, &tile_w, &tile_h)) {
         
         ImageData* image_data = load_bitmap(full_path);
         if (d_dne(image_data)) {
            d_log("failed to load bitmap: %s", entry->d_name);
            continue;
         }

         // calculate image dimensions in tiles
         int image_w = image_data->width / tile_w;
         int image_h = image_data->height / tile_h;
         
         // check for partial tiles and warn
         if (image_data->width % tile_w != 0 || image_data->height % tile_h != 0) {
            d_log("WARNING: %s has partial tiles - truncating to %dx%d tiles", 
                   entry->d_name, image_w, image_h);
            // TODO: modify ImageData to remove partial tiles
         }
         
         verify_image_colors(image_data); // TODO: finish this pls
         
         // determine resource type and add to appropriate array
         if (strcmp(type, "font") == 0) {
            // resize font array if needed
            if (fonts->font_count >= fonts->font_capacity) {
               fonts->font_capacity *= 2;
               Font* new_fonts = realloc(fonts->fonts, sizeof(Font) * fonts->font_capacity);
               if (!new_fonts) {
                  free(image_data);
                  continue;
               }
               fonts->fonts = new_fonts;
            }
            
            Font* font = &fonts->fonts[fonts->font_count];
            font->fname = malloc(strlen(entry->d_name) + 1);
            if (!font->fname) {
               free(image_data);
               continue;
            }
            strcpy((char*)font->fname, entry->d_name);
            font->tile_w = tile_w;
            font->tile_h = tile_h;
            font->image_w = image_w;
            font->image_h = image_h;
            font->data = image_data;
            font->ascii_start = 33; // fonts start at '!'
            
            fonts->font_count++;
            
         } else if (strcmp(type, "sprite") == 0) {
            // resize sprite array if needed
            if (sprites->sprite_count >= sprites->sprite_capacity) {
               sprites->sprite_capacity *= 2;
               Sprite* new_sprites = realloc(sprites->sprites, sizeof(Sprite) * sprites->sprite_capacity);
               if (!new_sprites) {
                  free(image_data);
                  continue;
               }
               sprites->sprites = new_sprites;
            }
            
            Sprite* sprite = &sprites->sprites[sprites->sprite_count];
            sprite->fname = malloc(strlen(entry->d_name) + 1);
            if (!sprite->fname) {
               free(image_data);
               continue;
            }
            strcpy((char*)sprite->fname, entry->d_name);
            strcpy(sprite->name, name); // store the parsed name for lookups
            sprite->tile_w = tile_w;
            sprite->tile_h = tile_h;
            sprite->image_w = image_w;
            sprite->image_h = image_h;
            sprite->data = image_data;
            
            sprites->sprite_count++;
            
         } else {
            d_log("unknown resource type '%s' in file %s", type, entry->d_name);
            free(image_data);
         }
      }
   }
   closedir(dir);
#endif
   
   d_logv(2, "loaded %d fonts and %d sprites from %s", 
          fonts->font_count, sprites->sprite_count, DIR_SHEETS);
   return 1;
}

Font* file_get_font(FontArray* font_array, FontType type) { // TODO: redo this
   const char* fname = d_name_font(type);
   if (!fname) return NULL;
   // TODO: finish this pls
   for (int i = 0; i < font_array->font_count; i++) {
      if (strcmp(font_array->fonts[i].fname, fname) == 0) {
         return &font_array->fonts[i];
      }
   }
   return NULL;
}

Sprite* file_get_sprite(SpriteArray* sprite_array, const char* sprite_name) {
   // TODO: finish this pls
   return NULL;
}

// INTERNAL
static ImageData* load_bitmap(const char* fname) {
   /* bitmap is assumed to be in its simplest structure */
   /* e.g. header, DIB header, pixel array. DIB assumed */
   /* to be windows bitmap.                             */
   FILE *image_file = NULL;
   image_file = fopen(fname, "rb");
   if (!image_file) {
      d_dne("image_file");
      fclose(image_file);
      return NULL;
   }

   uint8_t file_header[16];
   fread(&file_header, 1, 14, image_file);
   if(!(((uint16_t*)file_header)[0] == 0x4D42)) {
      d_err("file is not a valid bitmap");
      fclose(image_file);
      return NULL;
   }

   struct {
      uint32_t header_size;
      int32_t width;
      int32_t height;
      uint16_t planes;
      uint16_t bitdepth;
      uint32_t compression;
      uint32_t byte_length;
      int32_t length_per_meter;
      int32_t height_per_meter;
      uint32_t color_count;
      uint32_t important_colors;
   } dib_header; // dib = device-independent format
   fread(&dib_header, 1, sizeof(dib_header), image_file);
   
   ImageData *image_data;
   uint32_t data_size = dib_header.width * dib_header.height * 4;
   
   image_data = malloc(sizeof(ImageData) + data_size);
   if (!image_data) { printf("couldn't malloc ImageData\n"); return NULL; }
   {
      image_data->size = data_size;
      image_data->width = dib_header.width;
      image_data->height = dib_header.height;
      // image_data->data = image_data + 1;
   }
   
   // uint32_t bmp_pitch = dib_header.width * 3;
   uint32_t bmp_pitch = ((dib_header.width * 3 + 3) / 4) * 4; // for some reason this fixes 9x8 tiles
   uint32_t img_data_pitch = dib_header.width * 4;
   uint8_t pixel_buffer[bmp_pitch];
   // uint8_t *pixel_data = image_data->data + image_data->size;
   uint8_t *pixel_data = image_data->data;
   
   for (uint32_t i = dib_header.height; i > 0; i--) {
      fread(&pixel_buffer, 1, bmp_pitch, image_file);
      uint8_t* row_start = pixel_data + (i - 1) * img_data_pitch;
      for (uint32_t j = 0; j < dib_header.width; j++) {
         uint32_t buffer_offset = j * 3;
         uint32_t data_offset = j * 4;
         
         row_start[data_offset + 0] = pixel_buffer[buffer_offset + 2]; // R
         row_start[data_offset + 1] = pixel_buffer[buffer_offset + 1]; // G
         row_start[data_offset + 2] = pixel_buffer[buffer_offset + 0]; // B
         row_start[data_offset + 3] = 255;
      }
   }
   
   fclose(image_file);
   return image_data;
}

static int parse_sheet_filename(const char* fname, char* type, char* name, int* tile_w, int* tile_h) {
   // format: [type]_[name]_[w]x[h].bmp
   char* fname_copy = malloc(strlen(fname) + 1);
   strcpy(fname_copy, fname);
   
   char* token = strtok(fname_copy, "_");
   if (!token) { free(fname_copy); return 0; }
   strcpy(type, token);
   
   token = strtok(NULL, "_");
   if (!token) { free(fname_copy); return 0; }
   strcpy(name, token);
   
   token = strtok(NULL, ".");
   if (!token) { free(fname_copy); return 0; }
   
   if (sscanf(token, "%dx%d", tile_w, tile_h) != 2) {
      free(fname_copy);
      return 0;
   }
   
   free(fname_copy);
   return 1;
}

static void verify_image_colors(ImageData* image_data) {
   // load ImageData, make sure all colors match palette
   // if not, warn and change colors to closest matching in image_data
   // later we can call modify_image_colors() to change the actual file
}