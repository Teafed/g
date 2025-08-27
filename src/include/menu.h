#ifndef MENU_H
#define MENU_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_MENU_OPTIONS 16
#define MAX_CHOICES 8
#define MAX_MENU_TITLE_LEN 64
#define MAX_OPTION_TEXT_LEN 32
#define MAX_SCENE_TYPES 16

typedef enum {
   MENU_TYPE_MAIN,
   MENU_TYPE_CHARSEL,
   MENU_TYPE_SETTINGS,
   MENU_TYPE_PAUSE,
   MENU_TYPE_MAX
} MenuType;

typedef enum {
   OPTION_TYPE_ACTION,
   OPTION_TYPE_TOGGLE,
   OPTION_TYPE_CHOICE,
   OPTION_TYPE_SLIDER,
   OPTION_TYPE_SUBMENU,
   OPTION_TYPE_CHARSEL,
   OPTION_TYPE_MAX
} OptionType;

typedef enum {
   MENU_ACTION_NONE,
   MENU_ACTION_GAME_SETUP,    // action_data -> GameModeType
   MENU_ACTION_SCENE_CHANGE,  // action_data -> SceneType
   MENU_ACTION_BACK,
   MENU_ACTION_QUIT,
   MENU_ACTION_MAX
} MenuAction;

typedef struct MenuOption {
   char text[MAX_OPTION_TEXT_LEN];
   OptionType type;
   bool enabled;
   
   union {
      struct {  // ACTION
         MenuAction action;
         int action_data;
      };
      struct {  // TOGGLE
         bool* toggle_value;  // points to actual game setting
      };
      struct {  // CHOICE
         char* choices[MAX_CHOICES];
         int choice_count;
         int* current_choice;  // points to actual setting
      };
      struct {  // SLIDER
         int* slider_value;
         int min_value;
         int max_value;
         int step;
      };
      struct { // SUBMENU
         struct Menu* child;
      };
      struct {  // CHARSEL
         // this will change once i get to it
         int character_index;
      };
   };
} MenuOption;

#include "input.h"
#include "renderer.h"
typedef struct Menu {
   char title[MAX_MENU_TITLE_LEN];
   MenuType type;
   MenuOption options[MAX_MENU_OPTIONS];
   int option_count;
   int selected_option[MAX_PLAYERS];  // per-player selection
   
   LayerHandle layer_handle;
   struct Menu* parent;
} Menu;

// core functions
void menu_system_init(void); // called once in scene_init()
Menu* menu_create(MenuType type, const char* title);
void menu_handle_input(InputEvent event, InputState state, int device_id);
void menu_render(Menu* root); // menu points to root
void menu_destroy(Menu* menu); // called in x_scene_destroy()
void menu_system_cleanup(void); // just set g_active_menu to NULL

// option management
void menu_add_action_option(Menu* menu, const char* text, MenuAction action, int data);
void menu_add_submenu_option(Menu* menu, const char* text, Menu* submenu);
void menu_add_toggle_option(Menu* menu, const char* text, bool* toggle_ptr);
void menu_add_choice_option(Menu* menu, const char* text, char* choices[], int count, int* current_ptr);
void menu_add_slider_option(Menu* menu, const char* text, int* value_ptr, int min, int max, int step);
void menu_add_charsel_option(Menu* menu, const char* text, int character_index);

void menu_set_option_enabled(Menu* menu, int option_index, bool enabled);
void menu_remove_option(Menu* menu, int option_index);

// navigation and state
void menu_make_child(Menu* child, Menu* parent, int parent_option_index);
void menu_navigate_to_child(Menu* menu, int option_index, int player);
void menu_navigate_to_parent(Menu* menu, int player);
void menu_set_active(Menu* menu);

void menu_save_current_state(void); // when leaving a menu (scene change or going deeper)
void menu_restore_state(Menu* menu); // when entering a menu

#endif