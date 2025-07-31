// menu.h
#ifndef MENU_H
#define MENU_H

#include <stdbool.h>

#define MAX_MENUS 16 // we have room for so many menus
#define MAX_MENU_OPTIONS 8
#define MAX_MENU_TITLE_LEN 32
#define MAX_OPTION_TEXT_LEN 32

typedef enum {
   MENU_TYPE_LIST,      // vertical list
   MENU_TYPE_GRID,      // grid layout
   MENU_TYPE_SETTINGS,  // vertical list with configurable values
} MenuType;

typedef enum {
   MENU_ACTION_NONE,
   MENU_ACTION_SELECT,
   MENU_ACTION_CLOSE,
   MENU_ACTION_BACK,
   MENU_ACTION_SETTING_CHANGE,
   // action_data -> [unused]
   MENU_ACTION_SUBMENU,
   // action_data -> [unused]
   MENU_ACTION_SCENE_CHANGE,
   // action_data -> SceneType
   MENU_ACTION_GAME_SETUP,
   // action_data -> GameModeType
   MENU_ACTION_MAX
} MenuAction;

typedef struct MenuOption {
   char text[MAX_OPTION_TEXT_LEN];
   MenuAction action;
   int action_data;  // scene ID, submenu ID, etc
   
   // for settings menus
   char* setting_values[8];  // array of strings (e.g. low, medium, high)
   int setting_count;        // number of possible values
   int current_setting;      // current selected value index
} MenuOption;

typedef struct Menu {
   char title[MAX_MENU_TITLE_LEN];
   MenuType type;
   MenuOption options[MAX_MENU_OPTIONS];
   int option_count;
   int selected_option;
   
   int grid_cols;
   int grid_rows;
   int selected_col;
   int selected_row;
   
   // parent menu for sub-menus
   struct Menu* parent;
   struct Menu* children[MAX_MENU_OPTIONS];
   bool is_active;
} Menu;

// menu system
void menu_system_init(void);
#include "input.h"
void menu_handle_input(InputEvent event);
void menu_update_display(void);
void menu_destroy(void);

Menu* menu_create(const char* title, MenuType type);
void menu_make_child(Menu* child, Menu* parent, int index);
void menu_add_option(Menu* menu, const char* text, MenuAction action, int action_data);
void menu_add_setting_option(Menu* menu, const char* text, char* values[], int value_count, int default_value);
Menu* menu_get_active(void);
void menu_set_active(Menu* menu);
int menu_get_cursor_position(void);
void menu_set_cursor_position(int menu_position);

// menu navigation
void menu_move_up(Menu* menu);
void menu_move_down(Menu* menu);
void menu_move_left(Menu* menu);
void menu_move_right(Menu* menu);
void menu_select(Menu* menu);
void menu_back(Menu* menu);
void menu_close(Menu* menu);
void menu_character_set(Menu* menu);

// debug
void menu_debug_print_menu_action(MenuAction action);

#endif
