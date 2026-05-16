/* file.c
 * minimal bitmap loader. produces RGBA pixel buffers from 24-bit BMPs.
 */

#include "file.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ImageData* image_load_bmp(const char* path) {
   FILE* f = fopen(path, "rb");
   if (!f) {
      d_err("couldn't open image file: %s", path);
      return NULL;
   }

   // bmp file header is 14 bytes; we only need to verify the "BM" signature.
   uint8_t file_header[14];
   if (fread(file_header, 1, 14, f) != 14) {
      d_err("couldn't read file header: %s", path);
      fclose(f);
      return NULL;
   }
   if (((uint16_t*)file_header)[0] != 0x4D42) {
      d_err("file is not a valid bitmap: %s", path);
      fclose(f);
      return NULL;
   }

   // DIB header (40 bytes for BITMAPINFOHEADER; the format we expect).
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
   } dib;

   if (fread(&dib, 1, sizeof(dib), f) != sizeof(dib)) {
      d_err("couldn't read dib header: %s", path);
      fclose(f);
      return NULL;
   }

   if (dib.width <= 0 || dib.height <= 0) {
      // TODO: negative height means top-down rows; not handled here yet.
      d_err("unsupported bmp dimensions (%d x %d): %s",
            dib.width, dib.height, path);
      fclose(f);
      return NULL;
   }
   if (dib.bitdepth != 24) {
      d_err("only 24-bit BMPs are supported (got %u): %s", dib.bitdepth, path);
      fclose(f);
      return NULL;
   }

   ImageData* img = malloc(sizeof(*img));
   if (!img) {
      d_err("out of memory allocating ImageData: %s", path);
      fclose(f);
      return NULL;
   }
   img->width  = dib.width;
   img->height = dib.height;
   img->pixels = malloc((size_t)dib.width * (size_t)dib.height * 4);
   if (!img->pixels) {
      d_err("out of memory allocating image pixels: %s", path);
      free(img);
      fclose(f);
      return NULL;
   }

   // each BMP row is padded to a 4-byte boundary.
   uint32_t bmp_pitch  = (((uint32_t)dib.width * 3 + 3) / 4) * 4;
   uint32_t dst_pitch  = (uint32_t)dib.width * 4;
   uint8_t* row_buffer = malloc(bmp_pitch);
   if (!row_buffer) {
      d_err("out of memory allocating row buffer: %s", path);
      free(img->pixels);
      free(img);
      fclose(f);
      return NULL;
   }

   // BMP stores rows bottom-up; we want top-down. read bottom-to-top
   // and write to the corresponding row in the output.
   for (int32_t i = dib.height; i > 0; --i) {
      if (fread(row_buffer, 1, bmp_pitch, f) != bmp_pitch) {
         d_err("couldn't read pixel data at row %d: %s", i, path);
         free(row_buffer);
         free(img->pixels);
         free(img);
         fclose(f);
         return NULL;
      }

      uint8_t* dst = img->pixels + (i - 1) * dst_pitch;
      // bmp pixel order is BGR; convert to RGBA.
      for (int32_t x = 0; x < dib.width; ++x) {
         uint8_t* src = row_buffer + x * 3;
         dst[x * 4 + 0] = src[2];   // R
         dst[x * 4 + 1] = src[1];   // G
         dst[x * 4 + 2] = src[0];   // B
         dst[x * 4 + 3] = 255;      // A
      }
   }

   free(row_buffer);
   fclose(f);
   return img;
}

void image_free(ImageData* image) {
   if (!image) return;
   free(image->pixels);
   free(image);
}
