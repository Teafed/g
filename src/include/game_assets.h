#ifndef GAME_ASSETS_H
#define GAME_ASSETS_H

#include <liren/liren.h>

// opaque asset manager: loads bitmaps from a sheets directory, converts them
// to palette indices, and registers them with liren as fonts or sprites.
//
// files are expected to follow the naming convention:
//   font_<name>_<W>x<H>.bmp    -> registered as font   under <name>
//   sprite_<name>_<W>x<H>.bmp  -> registered as sprite under <name>
//
// after a successful load, lookups return liren handles ready for drawing.
typedef struct GameAssets GameAssets;

GameAssets* game_assets_load(LiRen* r, const char* sheets_dir);
void        game_assets_destroy(GameAssets* assets);

LR_FontHandle   game_assets_get_font(GameAssets* assets, const char* name);
LR_SpriteHandle game_assets_get_sprite(GameAssets* assets, const char* name);

#endif
