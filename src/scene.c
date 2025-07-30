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

static Menu* main_menu = NULL;
static Menu* character_menu = NULL;
static Menu* settings_menu = NULL;

void scene_init(void) {
   scene_reset_session();
   menu_system_init();

   // function pointers
   // setup title scene
   scene_manager.scenes[SCENE_TITLE].init = title_scene_init;
   scene_manager.scenes[SCENE_TITLE].update = title_scene_update;
   scene_manager.scenes[SCENE_TITLE].render = title_scene_render;
   scene_manager.scenes[SCENE_TITLE].handle_input = title_scene_handle_input;
   scene_manager.scenes[SCENE_TITLE].destroy = NULL;
   
   // setup main menu scene
   scene_manager.scenes[SCENE_MAIN_MENU].init = main_menu_scene_init;
   scene_manager.scenes[SCENE_MAIN_MENU].update = main_menu_scene_update;
   scene_manager.scenes[SCENE_MAIN_MENU].render = main_menu_scene_render;
   scene_manager.scenes[SCENE_MAIN_MENU].handle_input = main_menu_scene_handle_input;
   scene_manager.scenes[SCENE_MAIN_MENU].destroy = NULL;
   
   // setup character select scene
   scene_manager.scenes[SCENE_CHARACTER_SELECT].init = character_select_scene_init;
   scene_manager.scenes[SCENE_CHARACTER_SELECT].update = character_select_scene_update;
   scene_manager.scenes[SCENE_CHARACTER_SELECT].render = character_select_scene_render;
   scene_manager.scenes[SCENE_CHARACTER_SELECT].handle_input = character_select_scene_handle_input;
   scene_manager.scenes[SCENE_CHARACTER_SELECT].destroy = NULL;

   // setup gameplay scene
   scene_manager.scenes[SCENE_GAMEPLAY].init = settings_scene_init;
   scene_manager.scenes[SCENE_GAMEPLAY].update = settings_scene_update;
   scene_manager.scenes[SCENE_GAMEPLAY].render = settings_scene_render;
   scene_manager.scenes[SCENE_GAMEPLAY].handle_input = settings_scene_handle_input;
   scene_manager.scenes[SCENE_GAMEPLAY].destroy = NULL;
   
   // setup settings scene
   scene_manager.scenes[SCENE_SETTINGS].init = settings_scene_init;
   scene_manager.scenes[SCENE_SETTINGS].update = settings_scene_update;
   scene_manager.scenes[SCENE_SETTINGS].render = settings_scene_render;
   scene_manager.scenes[SCENE_SETTINGS].handle_input = settings_scene_handle_input;
   scene_manager.scenes[SCENE_SETTINGS].destroy = NULL;
   
   // initialize first scene - start with title screen
   scene_manager.current_scene = SCENE_TITLE;
   scene_manager.stack_depth = 0;
   if (scene_manager.scenes[SCENE_TITLE].init) {
      scene_manager.scenes[SCENE_TITLE].init();
   }
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

// scenes are only added to the stack once the scene ends
// call this BEFORE reassigning p1 & p2 devices, if that happens on a scene change
void scene_push(SceneType new_scene) {
   // save current scene state to stack
   if (scene_manager.stack_depth < MAX_SCENE_STACK_DEPTH) {
      SceneStackEntry* entry = &scene_manager.stack[scene_manager.stack_depth];
      entry->type = scene_manager.current_scene;
      entry->player_devices[0] = input_get_player_device(1);
      entry->player_devices[1] = input_get_player_device(2);
      
      // save current scene's UI state & player assignment
      switch (scene_manager.current_scene) {
         case SCENE_MAIN_MENU:
            entry->menu_position = menu_get_cursor_position();
            break;
         case SCENE_CHARACTER_SELECT:
            entry->menu_position = menu_get_cursor_position();
            break;
         case SCENE_SETTINGS:
            entry->menu_position = menu_get_cursor_position();
            break;
         default:
            entry->menu_position = 0;
            break;
      }
      scene_manager.stack_depth++;
   }
   else
      printf("tried to push a scene but it was too much\n");
   
   // transition to new scene
   scene_change_to(new_scene);
   
   printf("scene_push() called:\n");
   d_print_scene_stack();
}

void scene_pop(void) {
   if (scene_manager.stack_depth > 0) {
      scene_manager.stack_depth--;
      SceneStackEntry* entry = &scene_manager.stack[scene_manager.stack_depth];
      
      // previous device assignment and scene
      input_set_player_device(1, scene_manager.stack[scene_manager.stack_depth].player_devices[0]);
      input_set_player_device(2, scene_manager.stack[scene_manager.stack_depth].player_devices[1]);
      
      // go back to previous scene
      scene_change_to(entry->type);

      // TODO: i think the problem with going back a scene n remembering cursor pos is here
      switch (entry->type) {
         case SCENE_TITLE:
            // nothing i guess
            break;
         case SCENE_MAIN_MENU:
            // this might be broken
            menu_set_cursor_position(main_menu, entry->menu_position);
            break;
         case SCENE_CHARACTER_SELECT:
            menu_set_cursor_position(character_menu, entry->menu_position);
            break;
         case SCENE_SETTINGS:
            menu_set_cursor_position(settings_menu, entry->menu_position);
            break;
         default:
            break;
      }

      printf("scene_pop():\n");
      d_print_scene_stack();
   }
}

// only use when you don't need to return to the previous scene
// forward and back scene navigation uses scene_push() and scene_pop()
void scene_change_to(SceneType type) {
   d_logv(2, "changing scene from %s to %s", d_name_scene_type(scene_manager.current_scene), d_name_scene_type(type));
   if (type >= SCENE_MAX) {
      d_err("couldn't change scenes");
      return;
   }
   // destroy current scene
   if (scene_manager.scenes[scene_manager.current_scene].destroy) {
      scene_manager.scenes[scene_manager.current_scene].destroy();
   }

   scene_manager.current_scene = type;
   
   // initialize new scene
   if (scene_manager.scenes[scene_manager.current_scene].init) {
      scene_manager.scenes[scene_manager.current_scene].init();
   }
}

void scene_start_game_session(GameModeType mode) {
   scene_reset_session();
   scene_manager.session.mode = mode;
   scene_manager.session.valid = true;
}

void scene_update(float delta_time) {
   // make sure init, update, render, handle_input, destroy are never left uninitialized in scene_init

   scene_manager.scenes[scene_manager.current_scene].update(delta_time);
}

void scene_render(void) {
   if (scene_manager.scenes[scene_manager.current_scene].render) {
      scene_manager.scenes[scene_manager.current_scene].render();
   }
}

void scene_handle_input(InputEvent event, InputState state, int device_id) {
   if (scene_manager.scenes[scene_manager.current_scene].handle_input) {
      scene_manager.scenes[scene_manager.current_scene].handle_input(event, state, device_id);
   }
}

void scene_destroy(void) {
   menu_destroy();
}

// UTILITY
SceneType scene_get_current_scene(void) {
   return scene_manager.current_scene;
}

SceneStackEntry* scene_get_scene_entry_at_index(int index) {
   SceneStackEntry* scene = &scene_manager.stack[index];
   return scene;
}

GameSession* scene_get_session(void) {
    return &scene_manager.session;
}

int scene_get_stack_depth(void) {
   return scene_manager.stack_depth;
}


// ============================================================================
// TITLE SCENE
// ============================================================================
LayerHandle layer_bg, layer_test, layer_sized;
void title_scene_init(void) {
   layer_bg = renderer_create_layer(true);
   layer_test = renderer_create_layer(false);
   layer_sized = renderer_create_layer(false);
   renderer_set_layer_opacity(layer_sized, 64);
   renderer_set_size(layer_sized, 2);
   renderer_set_size(layer_test, 2);
}

void title_scene_update(float delta_time) {
   // no need for continuous updates rn
}

void title_scene_render(void) {
   renderer_clear();
   int dimx = 0;
   int dimy = 0;
   renderer_get_game_dimensions(&dimx, &dimy);
   SDL_Rect bg = {0, 0, dimx, dimy};
   renderer_draw_rect(layer_bg, &bg, 1);
   
   SDL_Rect border1 = {0, 0, 640, 1};
   SDL_Rect border2 = {0, 0, 1, 480};
   SDL_Rect border3 = {0, 479, 640, 1};
   SDL_Rect border4 = {639, 0, 1, 480};
      
   renderer_draw_rect(layer_test, &border1, 9);
   renderer_draw_rect(layer_test, &border2, 9);
   renderer_draw_rect(layer_test, &border3, 9);
   renderer_draw_rect(layer_test, &border4, 9);
   
   {
   renderer_draw_pixel(layer_test, 1, 1, 17);
   renderer_draw_pixel(layer_test, 1, 3, 17);
   renderer_draw_pixel(layer_test, 1, 5, 17);
   renderer_draw_pixel(layer_test, 1, 7, 17);
   renderer_draw_pixel(layer_test, 1, 9, 17);
   renderer_draw_pixel(layer_test, 2, 2, 16);
   renderer_draw_pixel(layer_test, 2, 4, 16);
   renderer_draw_pixel(layer_test, 2, 6, 16);
   renderer_draw_pixel(layer_test, 2, 8, 16);
   renderer_draw_pixel(layer_test, 3, 1, 15);
   renderer_draw_pixel(layer_test, 3, 3, 15);
   renderer_draw_pixel(layer_test, 3, 5, 15);
   renderer_draw_pixel(layer_test, 3, 7, 15);
   renderer_draw_pixel(layer_test, 4, 2, 15);
   renderer_draw_pixel(layer_test, 4, 4, 15);
   renderer_draw_pixel(layer_test, 4, 6, 15);
   renderer_draw_pixel(layer_test, 5, 1, 14);
   renderer_draw_pixel(layer_test, 5, 3, 14);
   renderer_draw_pixel(layer_test, 5, 5, 14);
   renderer_draw_pixel(layer_test, 6, 2, 14);
   renderer_draw_pixel(layer_test, 6, 4, 14);
   renderer_draw_pixel(layer_test, 7, 3, 0);
   renderer_draw_pixel(layer_test, 7, 1, 0);
   renderer_draw_pixel(layer_test, 8, 2, 0);
   renderer_draw_pixel(layer_test, 9, 1, 0);
   }
   FontType font = FONT_ACER_8_8;
   FontType font2 = FONT_COMPIS_8_16;
   
   const char* line1 = "! HEY EVERY    IT";
   const char* line2 = "u hh is me :) hehe";
   const char* line3 = "* H-hello...?";
   const char* line4 = "* ...";
   const char* line5 = "* Where am I...? I'm scared... Please...";
   const char* line6 = "  I don't want to be a test string...";
   SDL_Rect line1rect = {20, 207, 136, 9};
   SDL_Rect line2rect = {20, 217, 144, 9};
   renderer_draw_rect(layer_test, &line1rect, 14);
   renderer_draw_rect(layer_test, &line2rect, 17);
   renderer_draw_string(layer_test, font, line1, 20, 208, 4);
   renderer_draw_string(layer_test, font, line2, 20, 218, 23);
   
   renderer_draw_string(layer_test, font2, line3, 190, 50, 13);
   renderer_draw_string(layer_test, font2, line4, 190, 80, 13);
   renderer_draw_string(layer_test, font2, line5, 190, 110, 13);
   renderer_draw_string(layer_test, font2, line6, 190, 123, 13);
   
   renderer_draw_string(layer_test, font, "This is the title screen.", 10, 10, 4);
   renderer_draw_string(layer_test, font, "   Press [j] to start.", 10, 20, 4);

   renderer_draw_string(layer_sized, font, "This is the title screen.", 10, 30, 4);
   renderer_draw_string(layer_sized, font, "   Press [j] to start.", 10, 50, 4);
}

extern void game_shutdown(void);
void title_scene_handle_input(InputEvent event, InputState state, int device_id) {
   if (event == INPUT_START || event == INPUT_A) {
      // first input detected - assign as player 1
      if (input_get_player_device(1) == -1) {
         scene_push(SCENE_MAIN_MENU);
         input_set_player_device(1, device_id);
      }
      else {
         printf("for some reason player device 1 is already assigned on the main menu... that sohuldn't be\n");
         // d_print_scene_stack();
      }
   }
   else if (event == INPUT_B || event == INPUT_SELECT) {
      game_shutdown();
   }
}

// ============================================================================
// MAIN MENU SCENE
// ============================================================================

void main_menu_handle_scene_change(SceneType new_scene);
void main_menu_handle_game_setup(GameModeType mode);
void main_menu_update_input_display(void);

void main_menu_scene_init(void) {
   renderer_clear();
   
   main_menu = menu_create("MAIN MENU", MENU_TYPE_LIST);
   menu_add_option(main_menu, "SOLO", MENU_ACTION_SUBMENU, 1);
   menu_add_option(main_menu, "VERSUS", MENU_ACTION_GAME_SETUP, GAME_MODE_VERSUS);
   menu_add_option(main_menu, "SETTINGS", MENU_ACTION_SCENE_CHANGE, SCENE_SETTINGS);
   menu_add_option(main_menu, "TITLE", MENU_ACTION_BACK, 0);
   
   menu_set_active(main_menu);

   Menu* solo_menu = menu_create("SOLO MODE", MENU_TYPE_LIST);
   menu_make_child(solo_menu, main_menu, 0);
   menu_add_option(solo_menu, "ARCADE", MENU_ACTION_GAME_SETUP, GAME_MODE_ARCADE);
   menu_add_option(solo_menu, "TRAINING", MENU_ACTION_GAME_SETUP, GAME_MODE_TRAINING);
   menu_add_option(solo_menu, "STORY", MENU_ACTION_GAME_SETUP, GAME_MODE_STORY);

   solo_menu->parent = main_menu;
}

void main_menu_scene_update(float delta_time) {
   // update input display
   main_menu_update_input_display();
}

void main_menu_scene_render(void) {
   // menu rendering is handled by menu system
   // input display is handled separately as well
   // so idk what i have this function for lol
}

void main_menu_scene_handle_input(InputEvent event, InputState state, int device_id) {
   // only respond to player 1's device in menus
   if (device_id != input_get_player_device(1)) return;
   
   menu_handle_input(event, device_id);
}

void main_menu_handle_scene_change(SceneType new_scene) {
   switch (new_scene) {
   case SCENE_SETTINGS:
      scene_push(SCENE_SETTINGS);
      break;
   case SCENE_TITLE:
      scene_pop();
      break;
   default:
      printf("unhandled scene change\n");
   }
}

void main_menu_handle_game_setup(GameModeType mode) {
   scene_push(SCENE_CHARACTER_SELECT);
   scene_start_game_session(mode);
}

typedef struct {
   InputEvent event;
   int col, row;
   char label;
   uint8_t fg_color;
} InputDisplayConfig;

InputDisplayConfig dev_0[] = {
   {INPUT_UP, 55, 7, 'U', 11},
   {INPUT_DOWN, 55, 9, 'D', 11},
   {INPUT_LEFT, 54, 8, 'L', 11},
   {INPUT_RIGHT, 56, 8, 'R', 11},
   {INPUT_A, 58, 7, 'A', 12},
   {INPUT_B, 59, 7, 'B', 12},
   {INPUT_C, 60, 7, 'C', 12},
   {INPUT_D, 61, 7, 'D', 12}
};
InputDisplayConfig dev_1[] = {
   {INPUT_UP, 55, 13, 'U', 15},
   {INPUT_DOWN, 55, 15, 'D', 15},
   {INPUT_LEFT, 54, 14, 'L', 15},
   {INPUT_RIGHT, 56, 14, 'R', 15},
   {INPUT_A, 58, 13, 'A', 16},
   {INPUT_B, 59, 13, 'B', 16},
   {INPUT_C, 60, 13, 'C', 16},
   {INPUT_D, 61, 13, 'D', 16}
};
InputDisplayConfig dev_2[] = {
   {INPUT_UP, 67, 7, 'U', 22},
   {INPUT_DOWN, 67, 9, 'D', 22},
   {INPUT_LEFT, 66, 8, 'L', 22},
   {INPUT_RIGHT, 68, 8, 'R', 22},
   {INPUT_A, 70, 7, 'A', 23},
   {INPUT_B, 71, 7, 'B', 23},
   {INPUT_C, 72, 7, 'C', 23},
   {INPUT_D, 73, 7, 'D', 23}
};
InputDisplayConfig dev_3[] = {
   {INPUT_UP, 67, 13, 'U', 29},
   {INPUT_DOWN, 67, 15, 'D', 29},
   {INPUT_LEFT, 66, 14, 'L', 29},
   {INPUT_RIGHT, 68, 14, 'R', 29},
   {INPUT_A, 70, 13, 'A', 30},
   {INPUT_B, 71, 13, 'B', 30},
   {INPUT_C, 72, 13, 'C', 30},
   {INPUT_D, 73, 13, 'D', 30}
};

void main_menu_update_input_display(void) {
   // runs every frame to show current input states
   // !!! renderer_fill_rect(50, 5, 15, 10, ' ', 0, 4);  // device 0
   // !!! renderer_draw_string(50, 5, "DEVICE 0:", 15, 4);
   // !!! renderer_fill_rect(50, 11, 15, 10, ' ', 0, 4); // device 1
   // !!! renderer_draw_string(50, 11, "DEVICE 1:", 15, 4);
   // !!! renderer_fill_rect(62, 5, 15, 10, ' ', 0, 4);  // device 2
   // !!! renderer_draw_string(62, 5, "DEVICE 2:", 15, 4);
   // !!! renderer_fill_rect(62, 11, 15, 10, ' ', 0, 4); // device 3
   // !!! renderer_draw_string(62, 11, "DEVICE 3:", 15, 4);

   InputDisplayConfig* devs[] = { dev_0, dev_1, dev_2, dev_3 };
   
   for (int dev_i = 0; dev_i < MAX_INPUT_DEVICES; dev_i++) {
      if (g_input.devices[dev_i].connected) {
         for (int btn_i = 0; btn_i < 8; btn_i++) { // 8 buttons to display here
            if (input_held(devs[dev_i][btn_i].event, g_input.devices[dev_i].device_id)) {
               // !!! renderer_set_char(devs[dev_i][btn_i].col, devs[dev_i][btn_i].row, 
               //   devs[dev_i][btn_i].label, devs[dev_i][btn_i].fg_color, 4);
            }
         }
      }
   }
}

// ============================================================================
// CHARACTER SELECT SCENE
// ============================================================================

void device_select_init(void);
void device_select_update(float delta_time);
void device_select_render(void);
void device_select_handle_input(InputEvent event, int device_id);

void character_select_init(void);
void character_select_update(float delta_time);
void character_select_render(void);
void character_select_handle_input(InputEvent event, int device_id);

void character_select_handle_scene_change(SceneType new_scene);

void character_select_scene_init(void) {
   renderer_clear();

   // setup for device select
   input_set_player_device(1, -1);
   input_set_player_device(2, -1);
   scene_manager.session.confirmed_devices = false;

   // setup for character menu
   character_menu = menu_create("SELECT CHARACTER", MENU_TYPE_GRID);
   character_menu->grid_cols = 3;
   character_menu->grid_rows = 2;
   menu_add_option(character_menu, "IVWY", MENU_ACTION_SCENE_CHANGE, SCENE_GAMEPLAY);
   menu_add_option(character_menu, "TEAFED", MENU_ACTION_SCENE_CHANGE, SCENE_GAMEPLAY);
   menu_add_option(character_menu, "KATJA", MENU_ACTION_SCENE_CHANGE, SCENE_GAMEPLAY);
   menu_add_option(character_menu, "CRAIGE2", MENU_ACTION_SCENE_CHANGE, SCENE_GAMEPLAY);
   menu_add_option(character_menu, "FRANKIE", MENU_ACTION_SCENE_CHANGE, SCENE_GAMEPLAY);
   menu_add_option(character_menu, "HAELUN", MENU_ACTION_SCENE_CHANGE, SCENE_GAMEPLAY);

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

void character_select_scene_handle_input(InputEvent event, InputState state, int device_id) {
   if (!scene_manager.session.confirmed_devices) {
      device_select_handle_input(event, device_id);
   }
   else {
      if (device_id != input_get_player_device(1) || device_id != input_get_player_device(2)) return;

      // if want to return to device select, press B while character is unselected
      // set scene_manager.session.confirmed_devices to false
      // but don't reset p1 & p2 back to -1

      // handle character select menu input
      character_select_handle_input(event, device_id);
   }
   
}

void device_select_init(void) {
   // draw initial device select. the background is a pinwheel (layer 0)
   // layer 1, draw [1] [2] [3] & [4] where they should be according to g_input.devices[i].device_id & input_get_player_device()
}

void device_select_update(float delta_time) {
   // bg_pinwheel.rotation++ (changes every ~1 second)
   // figure out how to detect change in assigned devices best
   // on change, update the position coordinates for drawing [n]
}

void device_select_render(void) {
   // draw all the parts that have to be updated
}

void device_select_handle_input(InputEvent event, int device_id) {
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
         renderer_clear();
      }
      break;
   default:
      break;
   }
}

void character_select_init(void) {
   renderer_clear();   
   menu_set_active(character_menu);
}

void character_select_update(float delta_time) {
   // detect update somehow, change what character is shown on p1 and p2 sides
}

void character_select_render(void) {
   menu_update_display(); // i'll make this better
}

void character_select_handle_input(InputEvent event, int device_id) {
   menu_handle_input(event, device_id); // i prommy
}

void character_select_handle_scene_change(SceneType new_scene) {
   if (!scene_manager.session.confirmed_devices) {
      scene_pop();
      return;
   }
   
   switch (new_scene) {
   case SCENE_MAIN_MENU:
      scene_pop();
      break;
   case SCENE_GAMEPLAY:
      scene_push(SCENE_GAMEPLAY);
      break;
   default:
      printf("unhandled scene change\n");
   }
}

// ============================================================================
// SETTINGS SCENE
// ============================================================================

// TODO: what are the settings? fullscreen vs windowed vs window borderless...
// am i committed to only 640x480? remapping inputs? which of these are only on the settings scene, and which can be adjusted in the gameplay pause menu?
// GAME SETTINGS
// BGM VOLUME [0 - 100]
// SFX VOLUME [0 - 100]
// WINDOW [window, borderless windowed, fullscreen]
// * RESIZE MODE [fit, fixed, locked]
// RESOLUTION [VGA, FWVGA]
// * = only if set to window mode
void settings_scene_init(void) {
   renderer_clear();
   
   settings_menu = menu_create("SETTINGS", MENU_TYPE_SETTINGS);
   
   char* difficulty_values[] = {"easy", "normal :)", " hard"};
   menu_add_setting_option(settings_menu, "DIFFICULTY", difficulty_values, 3, 1);
   
   char* sound_values[] = {"OFF", "quiet", "normal :)", "loud!!!"};
   menu_add_setting_option(settings_menu, "SOUND", sound_values, 4, 2);
   
   char* music_values[] = {"off", "on"};
   menu_add_setting_option(settings_menu, "MUSIC", music_values, 2, 1);
   
   menu_add_option(settings_menu, "BACK", MENU_ACTION_SCENE_CHANGE, SCENE_MAIN_MENU);
   
   menu_set_active(settings_menu);
}

void settings_scene_update(float delta_time) {
   // yeah yeah
}

void settings_scene_render(void) {
   // as usual
}

void settings_scene_handle_input(InputEvent event, InputState state, int device_id) {
   // only accepts p1 input as this is accessed from the main menu
   if (device_id != input_get_player_device(1)) return;
   
   menu_handle_input(event, device_id);
}

void settings_handle_scene_change(SceneType new_scene) {
   if (new_scene == SCENE_MAIN_MENU) {
      scene_pop();
   }
}

// ============================================================================
// GAMEPLAY SCENE
// ============================================================================

// TODO: make another file to deal with anything that's not init, update, render, or handle input
void gameplay_scene_init(void) {
   renderer_clear();
}

void gameplay_scene_update(float delta_time) {
   // soon (tm)
}

void gameplay_scene_render(void) {
   // !!! renderer_draw_string(8, 15, "this is where the game will go..............................", 22, 4);
}

void gameplay_scene_handle_input(InputEvent event, InputState state, int device_id) {
   switch (event) {
   case INPUT_A:
      // !!! renderer_draw_string(8, 15, "...this is where the game will go..............................", 22, 4);
      break;
   case INPUT_B:
      // !!! renderer_draw_string(8, 15, "this is w...here the game will go..............................", 23, 4);
      break;
   case INPUT_START:
      scene_pop(); // back to char select menu
      break;
   default:
      break;
   }
}


