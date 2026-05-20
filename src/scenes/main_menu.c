// main menu scene. owns the top-level menu and the SOLO submenu.
// SETTINGS routes to a separate scene (settings.c), not a submenu.

#include "scenes/main_menu.h"
#include "scenes/settings.h"
#include "scenes/title.h"
#include "scene.h"
#include "menu.h"
#include "game.h"
#include "game_assets.h"
#include "input.h"
#include "debug.h"

#include <liren/liren.h>
#include <stdlib.h>

#define MAIN_MENU_X 80
#define MAIN_MENU_Y 80
#define SUBMENU_GAP 24   // pixels between main menu and submenu

typedef struct {
   GameAssets* assets;
   LR_FontHandle font;

   MenuStyle style;

   Menu* main;   // always rendered
   Menu* solo;   // rendered when in_submenu == true, to the right of main

   bool in_submenu;

   // cached layout for submenu placement
   int main_w;
} MainMenuData;

// --- back callbacks ---------------------------------------------------------

static void on_solo_back(void* user) {
   MainMenuData* d = (MainMenuData*)user;
   d->in_submenu = false;
   d_log("main_menu: <- back to main");
}

// --- action callbacks -------------------------------------------------------

static void on_solo_pressed(void* user) {
   MainMenuData* d = (MainMenuData*)user;
   d->in_submenu = true;
   d_log("main_menu: -> SOLO submenu");
}

static void on_versus_pressed(void* user) {
   (void)user;
   d_log("main_menu: VERSUS (not wired)");
}

static void on_settings_pressed(void* user) {
   (void)user;
   d_log("main_menu: -> settings scene");
   scene_stack_push(game_get_stack(), settings_scene_create());
}

static void on_title_pressed(void* user) {
   (void)user;
   d_log("main_menu: -> title scene");
   scene_stack_replace(game_get_stack(), title_scene_create());
}

static void on_arcade_pressed   (void* user) { (void)user; d_log("ARCADE"); }
static void on_training_pressed (void* user) { (void)user; d_log("TRAINING"); }
static void on_story_pressed    (void* user) { (void)user; d_log("STORY"); }

// --- vtable -----------------------------------------------------------------

static void main_menu_init(Scene* s, LiRen* r) {
   MainMenuData* d = (MainMenuData*)s->data;

   d->assets = game_get_assets();
   d->font   = game_assets_get_font(d->assets, "Master");
   if (d->font == LR_INVALID_FONT) {
      d_err("main_menu: Master font missing; menu will be empty");
   }

   d->style = MENU_DEFAULT_STYLE;
   d->style.default_font  = d->font;
   d->style.default_color = 0;     // mono-white
   d->style.line_height   = 12;
   d->style.title_gap     = 8;

   // main menu
   d->main = menu_create(r, "MAIN MENU");
   menu_add_action(d->main, "SOLO",     on_solo_pressed,     d);
   menu_add_action(d->main, "VERSUS",   on_versus_pressed,   NULL);
   menu_add_action(d->main, "SETTINGS", on_settings_pressed, NULL);
   menu_add_action(d->main, "TITLE",    on_title_pressed,    NULL);

   // solo submenu
   d->solo = menu_create(r, "SOLO");
   menu_add_action(d->solo, "ARCADE",   on_arcade_pressed,   NULL);
   menu_add_action(d->solo, "TRAINING", on_training_pressed, NULL);
   menu_add_action(d->solo, "STORY",    on_story_pressed,    NULL);
   menu_set_wrap_around(d->solo, false);
   menu_set_on_back(d->solo, on_solo_back, d);

   d->in_submenu = false;
   d->main_w = menu_get_width(d->main, &d->style);
}

static void main_menu_update(Scene* s, float dt) {
   MainMenuData* d = (MainMenuData*)s->data;
   // only the active menu polls repeat-nav, so an inactive parent
   // doesn't scroll while the player is using the submenu.
   Menu* active = d->in_submenu ? d->solo : d->main;
   menu_update(active, dt);
}

static void main_menu_render(Scene* s, LiRen* r) {
   MainMenuData* d = (MainMenuData*)s->data;

   // main always renders. submenu renders to its right when active.
   menu_render(d->main, r, MAIN_MENU_X, MAIN_MENU_Y, &d->style);
   if (d->in_submenu) {
      menu_render(d->solo, r,
                  MAIN_MENU_X + d->main_w + SUBMENU_GAP, MAIN_MENU_Y,
                  &d->style);
   }
}

static void main_menu_handle_input(Scene* s, InputEvent e, InputState st,
                                   int device_id) {
   MainMenuData* d = (MainMenuData*)s->data;
   Menu* active = d->in_submenu ? d->solo : d->main;
   menu_handle_input(active, e, st, device_id);
}

static void main_menu_destroy(Scene* s, LiRen* r) {
   MainMenuData* d = (MainMenuData*)s->data;
   menu_destroy(d->solo, r);
   menu_destroy(d->main, r);
   free(d);
}

static const SceneVTable MAIN_MENU_VTABLE = {
   .init         = main_menu_init,
   .update       = main_menu_update,
   .render       = main_menu_render,
   .handle_input = main_menu_handle_input,
   .destroy      = main_menu_destroy,
};

Scene* main_menu_scene_create(void) {
   Scene* s = calloc(1, sizeof(Scene));
   if (!s) { d_err("alloc Scene (main_menu)"); return NULL; }
   MainMenuData* d = calloc(1, sizeof(MainMenuData));
   if (!d) { d_err("alloc MainMenuData"); free(s); return NULL; }
   s->vtable = &MAIN_MENU_VTABLE;
   s->data   = d;
   s->name   = "main_menu";
   return s;
}
