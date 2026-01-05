// liren_text.h
#ifndef LIREN_TEXT_H
#define LIREN_TEXT_H
typedef struct LiRenFont LiRenFont;

LiRenFont* liren_font_create_bitmap(LiRen* r, const LR_BitmapFontDesc* desc);
void liren_font_destroy(LiRenFont* font);

void liren_draw_text(LiRen* r,
                     LiRenFont* font,
                     LR_LayerHandle layer,
                     const char* text,
                     int x, int y,
                     ui8 color_index);
#endif