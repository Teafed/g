#ifndef FILE_H
#define FILE_H

#include <stdint.h>

// raw image data loaded from disk. RGBA, row-major, top-left origin
// each pixel is 4 bytes: R, G, B, A
typedef struct {
   int32_t width;
   int32_t height;
   uint8_t* pixels;   // owned; freed by image_free
} ImageData;

// loads a 24-bit BMP from disk into an RGBA buffer
// returns NULL on failure (logs the reason)
// caller owns the returned ImageData and must call image_free
ImageData* image_load_bmp(const char* path);

void image_free(ImageData* image);

#endif
