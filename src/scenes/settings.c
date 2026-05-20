// settings scene. owns its own menu. B (or BACK) pops the scene.

#include "scenes/settings.h"
#include "scene.h"
#include "menu.h"
#include "game.h"
#include "game_assets.h"
#include "input.h"
#include "debug.h"

#include <liren/liren.h>
#include <stdlib.h>

#define SETTINGS_X 80
#define SETTINGS_Y 80

// resolution choice strings (must outlive the menu).
static const char* RESOLUTION_CHOICES[] = { "VGA", "FWVGA" };
#define RESOLUTION_COUNT 2

typedef struct {
   GameAssets* assets;
   LR_FontHandle font;

   MenuStyle style;
   Menu* menu;

   // backing values for the option targets. scene-local; not persisted.
   bool fullscreen;
   int  volume;
   int  resolution_idx;
} SettingsData;

// --- choice getter/setter ---------------------------------------------------

static int  res_get(void* user) { return ((SettingsData*)user)->resolution_idx; }
static void res_set(void* user, int v) { ((SettingsData*)user)->resolution_idx = v; }

// --- back handler -----------------------------------------------------------

static void on_settings_back(void* user) {
   (void)user;
   d_log("settings: <- pop");
   scene_stack_pop(game_get_stack());
}

static void on_back_pressed(void* user) { on_settings_back(user); }

// --- vtable -----------------------------------------------------------------

static void settings_init(Scene* s, LiRen* r) {
   SettingsData* d = (SettingsData*)s->data;

   d->assets = game_get_assets();
   d->font   = game_assets_get_font(d->assets, "Master");

   d->style = MENU_DEFAULT_STYLE;
   d->style.default_font  = d->font;
   d->style.default_color = 0;
   d->style.line_height   = 12;
   d->style.title_gap     = 8;

   // defaults
   d->fullscreen     = false;
   d->volume         = 80;
   d->resolution_idx = 0;

   d->menu = menu_create(r, "SETTINGS");
   menu_add_toggle(d->menu, "FULLSCREEN", &d->fullscreen);
   menu_add_slider(d->menu, "VOLUME", &d->volume, 0, 100, 5);
   menu_add_choice(d->menu, "RESOLUTION",
                   RESOLUTION_CHOICES, RESOLUTION_COUNT,
                   res_get, res_set, d);
   menu_add_action(d->menu, "BACK", on_back_pressed, d);
   menu_set_on_back(d->menu, on_settings_back, d);
}

static void settings_update(Scene* s, float dt) {
   SettingsData* d = (SettingsData*)s->data;
   menu_update(d->menu, dt);
}

static void settings_render(Scene* s, LiRen* r) {
   SettingsData* d = (SettingsData*)s->data;
   menu_render(d->menu, r, SETTINGS_X, SETTINGS_Y, &d->style);
}

static void settings_handle_input(Scene* s, InputEvent e, InputState st,
                                  int device_id) {
   SettingsData* d = (SettingsData*)s->data;
   menu_handle_input(d->menu, e, st, device_id);
}

static void settings_destroy(Scene* s, LiRen* r) {
   SettingsData* d = (SettingsData*)s->data;
   menu_destroy(d->menu, r);
   free(d);
}

static const SceneVTable SETTINGS_VTABLE = {
   .init         = settings_init,
   .update       = settings_update,
   .render       = settings_render,
   .handle_input = settings_handle_input,
   .destroy      = settings_destroy,
};

Scene* settings_scene_create(void) {
   Scene* s = calloc(1, sizeof(Scene));
   if (!s) { d_err("alloc Scene (settings)"); return NULL; }
   SettingsData* d = calloc(1, sizeof(SettingsData));
   if (!d) { d_err("alloc SettingsData"); free(s); return NULL; }
   s->vtable = &SETTINGS_VTABLE;
   s->data   = d;
   s->name   = "settings";
   return s;
}
