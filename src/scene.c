//dsjafsafh
///jsdfjsdakf;ksdkafjlsa
//jfkdsajfd;sajfkldasjf
//jfdsafjk;sagj;gsad
//fdhsuahgjksdahlgjk

#include "scene.h"
#include "renderer.h"
#include "file.h"
#include "timing.h"
#include "input.h"
#include "menu.h"
#include "debug.h"
#include <stdio.h>

static SceneManager scene_manager = { 0 };

void scene_init(void) {
   scene_reset_session();
   menu_system_init();

   // function pointers
   // make sure init, update, render, destroy are never left uninitialized
   // setup title scene
   scene_manager.scenes[SCENE_TITLE].init = title_scene_init;
   scene_manager.scenes[SCENE_TITLE].update = title_scene_update;
   scene_manager.scenes[SCENE_TITLE].render = title_scene_render;
   scene_manager.scenes[SCENE_TITLE].destroy = title_scene_destroy;
   
   // setup main menu scene
   scene_manager.scenes[SCENE_MAIN_MENU].init = main_menu_scene_init;
   scene_manager.scenes[SCENE_MAIN_MENU].update = main_menu_scene_update;
   scene_manager.scenes[SCENE_MAIN_MENU].render = main_menu_scene_render;
   scene_manager.scenes[SCENE_MAIN_MENU].destroy = main_menu_scene_destroy;
   
   // setup settings scene
   scene_manager.scenes[SCENE_SETTINGS].init = settings_scene_init;
   scene_manager.scenes[SCENE_SETTINGS].update = settings_scene_update;
   scene_manager.scenes[SCENE_SETTINGS].render = settings_scene_render;
   scene_manager.scenes[SCENE_SETTINGS].destroy = settings_scene_destroy;
   
   // setup character select scene
   scene_manager.scenes[SCENE_CHARACTER_SELECT].init = character_select_scene_init;
   scene_manager.scenes[SCENE_CHARACTER_SELECT].update = character_select_scene_update;
   scene_manager.scenes[SCENE_CHARACTER_SELECT].render = character_select_scene_render;
   scene_manager.scenes[SCENE_CHARACTER_SELECT].destroy = character_select_scene_destroy;

   // setup gameplay scene
   scene_manager.scenes[SCENE_GAMEPLAY].init = gameplay_scene_init;
   scene_manager.scenes[SCENE_GAMEPLAY].update = gameplay_scene_update;
   scene_manager.scenes[SCENE_GAMEPLAY].render = gameplay_scene_render;
   scene_manager.scenes[SCENE_GAMEPLAY].destroy = gameplay_scene_destroy;
   
   // initialize first scene
   scene_manager.current_scene = SCENE_TITLE;
   if (scene_manager.scenes[SCENE_TITLE].init) {
      scene_manager.scenes[SCENE_TITLE].init();
   }
}

void scene_update(float delta_time) {
   if (scene_manager.scenes[scene_manager.current_scene].update) {
      scene_manager.scenes[scene_manager.current_scene].update(delta_time);
   }
}

void scene_render(void) {
   if (scene_manager.scenes[scene_manager.current_scene].render) {
      scene_manager.scenes[scene_manager.current_scene].render();
   }
}

void scene_destroy(void) {
   if (scene_manager.scenes[scene_manager.current_scene].destroy) {
      scene_manager.scenes[scene_manager.current_scene].destroy();
   }
   menu_system_cleanup();
}

void scene_change_to(SceneType type) {
   d_logv(2, "changing scene from %s to %s", d_name_scene_type(scene_manager.current_scene), d_name_scene_type(type));
   if (type >= SCENE_MAX) {
      d_err("invalid scene");
      return;
   }
   
   if (scene_manager.scenes[scene_manager.current_scene].destroy) {
      scene_manager.scenes[scene_manager.current_scene].destroy();
   }

   scene_manager.current_scene = type;
   
   if (scene_manager.scenes[scene_manager.current_scene].init) {
      scene_manager.scenes[scene_manager.current_scene].init();
   }
}

void scene_start_game_session(GameModeType mode) {
   scene_reset_session();
   scene_manager.session.mode = mode;
   scene_manager.session.valid = true;
}

void scene_reset_session(void) {
   scene_manager.session.mode = GAME_MODE_MAX; // invalid
   scene_manager.session.selected_characters[0] = -1;
   scene_manager.session.selected_characters[1] = -1;
   scene_manager.session.cpu_players[0] = false;
   scene_manager.session.cpu_players[1] = false;
   scene_manager.session.selected_stage = -1;
   scene_manager.session.valid = false;
}

// ============================================================================
// TITLE SCENE
// ============================================================================

LayerHandle layer_bg, layer_test, layer_sized;
int dimx = 0, dimy = 0;
Rect moving_box = {0, 0, 100, 100};
uint8_t box_color = 5;
bool x_forward = true, y_forward = true;
void update_dvd(Rect* rect, int amt);
void draw_title(void);
void draw_dvd(void);

void title_scene_init(void) {
   layer_bg = renderer_create_layer(false);
   layer_test = renderer_create_layer(false);
   layer_sized = renderer_create_layer(false);
   renderer_set_layer_size(layer_test, 1);
   // renderer_set_layer_size(layer_bg, 1);

   input_reset_player_devices();
   input_set_context(CONTEXT_TITLE);
}

extern void game_shutdown(void);
void title_scene_handle_input(InputEvent event, InputState state, int device_id) {
   if (!state.pressed) return;
   if (event == INPUT_START || event == INPUT_A) {
      // first input detected - assign as player 1
      if (input_get_player_device(1) == -1) {
         scene_change_to(SCENE_MAIN_MENU);
         input_set_player_device(1, device_id);
      }
      else {
         d_log("for some reason player device 1 is already assigned on the main menu... that sohuldn't be");
      }
   }
   else if (event == INPUT_B || event == INPUT_SELECT) {
      game_shutdown();
   }
}

void title_scene_update(float delta_time) {
   (void)delta_time;

   update_dvd(&moving_box, 4);
}

void title_scene_render(void) {
   draw_title();
   draw_dvd();
}

void title_scene_destroy(void) {
   renderer_destroy_layer(layer_sized);
   renderer_destroy_layer(layer_test);
   renderer_destroy_layer(layer_bg);
}

void update_dvd(Rect* rect, int amt) {
   void cycle_color() {
      box_color = (box_color + 1) % (PALETTE_SIZE - 1);
   }
   
   renderer_get_dims(&dimx, &dimy);
   if (x_forward) {
      if (rect->x + rect->w < dimx) rect->x += amt;
      else { rect->x -= amt; x_forward = false; cycle_color(); }
   } else {
      if (rect->x >= 0) rect->x -= amt;
      else { rect->x += amt; x_forward = true; cycle_color(); }
   }
   if (y_forward) {
      if (rect->y + rect->h < dimy) rect->y += amt;
      else { rect->y -= amt; y_forward = false; cycle_color(); }
   } else {
      if (rect->y >= 0) rect->y -= amt;
      else { rect->y += amt; y_forward = true; cycle_color(); }
   }
}

void draw_dvd(void) {
   renderer_draw_rect(layer_sized, moving_box, box_color);
   renderer_draw_string(layer_sized, FONT_ACER_8_8, "DVD", moving_box.x + 28, moving_box.y + 42, 4);
}

void draw_title(void) {
   renderer_draw_fill(layer_bg, 0);
   Rect border1 = {0, 0, 640, 4};
   Rect border2 = {0, 0, 4, 480};
   Rect border3 = {0, 476, 640, 4};
   Rect border4 = {636, 0, 4, 480};
   Rect title_rect = {90, 20, 420, 60};
   
   renderer_draw_rect(layer_sized, border1, 10);
   renderer_draw_rect(layer_sized, border2, 10);
   renderer_draw_rect(layer_sized, border3, 10);
   renderer_draw_rect(layer_sized, border4, 10);

   renderer_draw_rect(layer_sized, title_rect, 7);
   
   renderer_draw_string(layer_sized, FONT_ACER_8_8, "This is the title screen.", 100, 30, 4);
   renderer_draw_string(layer_sized, FONT_ACER_8_8, "   Press [j] to start.", 100, 55, 4);
}

// ============================================================================
// MAIN MENU SCENE
// ============================================================================

static Menu* main_menu = NULL;
static Menu* solo_menu = NULL;

void main_menu_scene_init(void) {
   main_menu = menu_create(MENU_TYPE_MAIN, "MAIN MENU");
   solo_menu = menu_create(MENU_TYPE_MAIN, "SOLO MODE");
   
   menu_add_submenu_option(main_menu, "SOLO", solo_menu);
   menu_add_action_option(main_menu, "VERSUS", MENU_ACTION_GAME_SETUP, GAME_MODE_VERSUS);
   menu_add_action_option(main_menu, "SETTINGS", MENU_ACTION_SCENE_CHANGE, SCENE_SETTINGS);
   menu_add_action_option(main_menu, "TITLE", MENU_ACTION_SCENE_CHANGE, SCENE_TITLE);
   
   menu_make_child(solo_menu, main_menu, 0);
   menu_add_action_option(solo_menu, "ARCADE", MENU_ACTION_GAME_SETUP, GAME_MODE_ARCADE);
   menu_add_action_option(solo_menu, "TRAINING", MENU_ACTION_GAME_SETUP, GAME_MODE_TRAINING);
   menu_add_action_option(solo_menu, "STORY", MENU_ACTION_GAME_SETUP, GAME_MODE_STORY);

   menu_set_active(main_menu);
   input_set_context(CONTEXT_MENU);
}

void main_menu_scene_update(float delta_time) {
   (void)delta_time;
}

void main_menu_scene_render(void) {
   menu_render(main_menu);
}

void main_menu_scene_destroy(void) {
   menu_destroy(solo_menu);
   menu_destroy(main_menu);
}

// ============================================================================
// SETTINGS SCENE
// ============================================================================

static Menu* settings_menu = NULL;
LayerHandle layer_input;

bool g_sound_enabled = true;
int g_master_volume = 90;
void draw_input_display(void);

void on_resolution_change(int value) { renderer_set_display_resolution((DisplayResolution)value); }
void on_window_mode_change(int value) { renderer_set_window_mode((WindowMode)value); }
void on_resize_mode_change(int value) { renderer_set_resize_mode((ResizeMode)value); }

void settings_scene_init(void) {
   layer_input = renderer_create_layer(false);
   settings_menu = menu_create(MENU_TYPE_SETTINGS, "SYSTEM SETTINGS");
   
   menu_add_toggle_option(settings_menu, "SOUND ENABLED", &g_sound_enabled);
   menu_add_slider_option(settings_menu, "MASTER VOLUME", &g_master_volume, 0, 100, 1);
   char* resolution_choices[] = {"VGA", "FWVGA"};
   menu_add_choice_option(settings_menu, "RESOLUTION", resolution_choices, 2, renderer_get_display_resolution(), on_resolution_change);
   char* window_choices[] = {"WINDOWED", "BORDERLESS", "FULLSCREEN"};
   menu_add_choice_option(settings_menu, "WINDOW MODE", window_choices, 3, renderer_get_window_mode(), on_window_mode_change);
   char* resize_choices[] = {"FIT", "FIXED"};
   menu_add_choice_option(settings_menu, "RESIZE MODE", resize_choices, 2, renderer_get_resize_mode(), on_resize_mode_change);
   menu_add_action_option(settings_menu, "BACK", MENU_ACTION_SCENE_CHANGE, SCENE_MAIN_MENU);
   
   menu_set_active(settings_menu);
   input_set_context(CONTEXT_MENU);
}

void settings_scene_update(float delta_time) {
   (void)delta_time;
}

void settings_scene_render(void) {
   menu_render(settings_menu);
   draw_input_display();
}

void settings_scene_destroy(void) {
   menu_destroy(settings_menu);
}

typedef struct {
   InputEvent event;
   int col, row;
   char label;
   uint8_t color;
} InputDisplayConfig;

InputDisplayConfig dev_0[] = {
   {INPUT_UP, 436, 66, 'U', 11},
   {INPUT_DOWN, 436, 98, 'D', 11},
   {INPUT_LEFT, 420, 82, 'L', 11},
   {INPUT_RIGHT, 452, 82, 'R', 11},
   {INPUT_A, 484, 82, 'A', 12},
   {INPUT_B, 500, 82, 'B', 12},
   {INPUT_C, 516, 82, 'C', 12},
   {INPUT_D, 532, 82, 'D', 12}
};
InputDisplayConfig dev_1[] = {
   {INPUT_UP, 436, 166, 'U', 15},
   {INPUT_DOWN, 436, 198, 'D', 15},
   {INPUT_LEFT, 420, 182, 'L', 15},
   {INPUT_RIGHT, 452, 182, 'R', 15},
   {INPUT_A, 484, 182, 'A', 16},
   {INPUT_B, 500, 182, 'B', 16},
   {INPUT_C, 516, 182, 'C', 16},
   {INPUT_D, 532, 182, 'D', 16}
};
InputDisplayConfig dev_2[] = {
   {INPUT_UP, 436, 266, 'U', 22},
   {INPUT_DOWN, 436, 298, 'D', 22},
   {INPUT_LEFT, 420, 282, 'L', 22},
   {INPUT_RIGHT, 452, 282, 'R', 22},
   {INPUT_A, 484, 282, 'A', 23},
   {INPUT_B, 500, 282, 'B', 23},
   {INPUT_C, 516, 282, 'C', 23},
   {INPUT_D, 532, 282, 'D', 23}
};
InputDisplayConfig dev_3[] = {
   {INPUT_UP, 436, 366, 'U', 29},
   {INPUT_DOWN, 436, 398, 'D', 29},
   {INPUT_LEFT, 420, 382, 'L', 29},
   {INPUT_RIGHT, 452, 382, 'R', 29},
   {INPUT_A, 484, 382, 'A', 30},
   {INPUT_B, 500, 382, 'B', 30},
   {INPUT_C, 516, 382, 'C', 30},
   {INPUT_D, 532, 382, 'D', 30}
};

void draw_input_display(void) {
   // runs every frame to show current input states
   Rect dev_rect0 = {420, 50, 144, 62};
   renderer_draw_rect(layer_input, dev_rect0, 3);  // device 0
   renderer_draw_string(layer_input, FONT_ACER_8_8, "DEVICE 0:", dev_rect0.x, dev_rect0.y, 24);
   Rect dev_rect1 = {420, 150, 144, 62};
   renderer_draw_rect(layer_input, dev_rect1, 3);  // device 1
   renderer_draw_string(layer_input, FONT_ACER_8_8, "DEVICE 1:", dev_rect1.x, dev_rect1.y, 25);
   Rect dev_rect2 = {420, 250, 144, 62};
   renderer_draw_rect(layer_input, dev_rect2, 3);  // device 2
   renderer_draw_string(layer_input, FONT_ACER_8_8, "DEVICE 2:", dev_rect2.x, dev_rect2.y, 26);
   Rect dev_rect3 = {420, 350, 144, 62};
   renderer_draw_rect(layer_input, dev_rect3, 3);  // device 3
   renderer_draw_string(layer_input, FONT_ACER_8_8, "DEVICE 3:", dev_rect3.x, dev_rect3.y, 27);

   InputDisplayConfig* devs[] = { dev_0, dev_1, dev_2, dev_3 };
   
   const InputSystem* g_input = input_get_debug_state(); // DOTO: MAKE ACCESSOR
   for (int dev_i = 0; dev_i < MAX_INPUT_DEVICES; dev_i++) {
      if (g_input->devices[dev_i].connected) {
         for (int btn_i = 0; btn_i < 8; btn_i++) { // 8 buttons to display here
            if (input_held(devs[dev_i][btn_i].event, g_input->devices[dev_i].device_id)) {
               renderer_draw_char(layer_input, FONT_ACER_8_8, devs[dev_i][btn_i].label,
                                  devs[dev_i][btn_i].col, devs[dev_i][btn_i].row, devs[dev_i][btn_i].color);
            }
         }
      }
   }
}

// ============================================================================
// CHARACTER SELECT SCENE
// ============================================================================

static Menu* character_menu = NULL;

void device_select_init(void);
void device_select_handle_input(InputEvent event, InputState state, int device_id);
void device_select_update(float delta_time);
void device_select_render(void);

void character_select_init(void);
void character_select_update(float delta_time);
void character_select_render(void);

void character_select_handle_scene_change(SceneType new_scene);

void character_select_scene_init(void) {
   // TODO: change setup based on GameModeType (already set menu's process_action()
   
   // setup for device select
   input_reset_player_devices();
   scene_manager.session.confirmed_devices = false;

   // setup for character menu
   character_menu = menu_create(MENU_TYPE_CHARSEL, "SELECT CHARACTER");
   menu_add_charsel_option(character_menu, "IVWY", 0);
   menu_add_charsel_option(character_menu, "TEAFED", 1);
   menu_add_charsel_option(character_menu, "KATJA", 2);
   menu_add_charsel_option(character_menu, "CRAIGE2", 3);
   menu_add_charsel_option(character_menu, "FRANKIE", 4);
   menu_add_charsel_option(character_menu, "HAELUN", 5);

   device_select_init();
}

void character_select_scene_update(float delta_time) {
   if (!scene_manager.session.confirmed_devices) {
      device_select_update(delta_time);
   }
}

void character_select_scene_render(void) {
   if (!scene_manager.session.confirmed_devices) {
      device_select_render();
      return;
   }
   character_select_render();
}

void character_select_scene_destroy(void) {
   menu_destroy(character_menu);
}

void device_select_init(void) {
   input_set_context(CONTEXT_DEVICE_SELECT);
   // draw initial device select. the background is a pinwheel (layer 0)
   // layer 1, draw [1] [2] [3] & [4] where they should be according to g_input.devices[i].device_id & input_get_player_device()
}

void device_select_update(float delta_time) {
   (void)delta_time;
   // bg_pinwheel.rotation++ (changes every ~1 second)
   // figure out how to detect change in assigned devices best
   // on change, update the position coordinates for drawing [n]
}

void device_select_render(void) {
   // draw all the parts that have to be updated
}

void device_select_handle_input(InputEvent event, InputState state, int device_id) {
   (void) state;
   bool solo_mode = true;
   if (scene_manager.session.mode == GAME_MODE_VERSUS)
      solo_mode = false;

   if (solo_mode && ((input_get_player_device(1) != -1) || (input_get_player_device(2) != -1)))
      return;

   int p1 = input_get_player_device(1);
   int p2 = input_get_player_device(2);

   switch (event) {
   case INPUT_A:
      if (p1 == device_id || p2 == device_id) {
         scene_manager.session.confirmed_devices = true; // confirm devices
         character_select_init();
      }
      else if (p1 == -1)
         input_set_player_device(1, device_id);
      else if (p2 == -1)
         input_set_player_device(2, device_id);
      break;
   case INPUT_LEFT:
      if (p2 == device_id)
         input_set_player_device(2, -1);
      else if (p1 == -1)
         input_set_player_device(1, device_id);
      break;
   case INPUT_RIGHT:
      if (p1 == device_id)
         input_set_player_device(1, -1);
      else if (p2 == -1)
         input_set_player_device(2, device_id);
      break;
   case INPUT_B:
      if (p1 != device_id && p2 != device_id)
         character_select_handle_scene_change(SCENE_MAIN_MENU); // return to main menu
      else if (p1 == device_id)
         input_set_player_device(1, -1);
      else if (p2 == device_id)
         input_set_player_device(2, -1);
      break;
   case INPUT_START:
      if (p1 == device_id || p2 == device_id) {
         scene_manager.session.confirmed_devices = true; // confirm devices
         character_select_init();
      }
      break;
   default:
      break;
   }
}

void character_select_init(void) {
   input_set_context(CONTEXT_MENU);
   menu_set_active(character_menu);
}

void character_select_update(float delta_time) {
   (void)delta_time;
   // detect update somehow, change what character is shown on p1 and p2 sides
}

void character_select_render(void) {
   menu_render(character_menu);
}

void character_select_handle_scene_change(SceneType new_scene) {
   if (!scene_manager.session.confirmed_devices) {
      scene_change_to(SCENE_MAIN_MENU);
      return;
   }
   
   switch (new_scene) {
   case SCENE_MAIN_MENU:
      scene_change_to(SCENE_MAIN_MENU);
      break;
   case SCENE_GAMEPLAY:
      scene_change_to(SCENE_GAMEPLAY);
      break;
   default:
      d_log("unhandled scene change");
   }
}

// ============================================================================
// GAMEPLAY SCENE
// ============================================================================

void gameplay_scene_init(void) {
   //
}

void gameplay_scene_update(float delta_time) {
   (void)delta_time;
   // soon (tm)
}

void gameplay_scene_render(void) {
   // !!! renderer_draw_string(8, 15, "this is where the game will go..............................", 22, 4);
}

void gameplay_scene_destroy(void) {
   // 
}

