#include "menu.h"
#include "renderer.h"
#include "input.h"
#include "scene.h"
#include "debug.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static Menu* active_menu = NULL;
static Menu* menus[MAX_MENUS];
static int menu_count = 0;

void menu_system_init(void) {
   // TODO: maybe change away from using active_menu?
   //       also make sure parent/children are NULL
   active_menu = NULL;
   
   menu_count = 0;
   for (int i = 0; i < MAX_MENUS; i++) {
      menus[i] = NULL;
   }
}

void menu_handle_input(InputEvent event) {
   if (d_dne(active_menu)) return;
   
   switch (event) {
   case INPUT_UP:
      menu_move_up(active_menu);
      break;
   case INPUT_DOWN:
      menu_move_down(active_menu);
      break;
   case INPUT_LEFT:
      menu_move_left(active_menu);
      break;
   case INPUT_RIGHT:
      menu_move_right(active_menu);
      break;
   case INPUT_A:
      menu_select(active_menu);
      break;
   case INPUT_B:
      menu_back(active_menu);
      break;
   case INPUT_START:
      menu_close(active_menu);
      break;
   default:
      d_logv(2, "unhandled menu input");
   }
}

// TODO: be able to determine dimensions and placement on screen
void menu_update_display(void) {
   if (d_dne(active_menu)) return;
   
   Menu* menu = active_menu;
   
   // clear menu area
   // !!! renderer_fill_rect(5, 2, 40, 18, ' ', 0, 13);
   
   // draw title
   // int title_x = 20 - strlen(menu->title) / 2;
   // !!! renderer_draw_string(title_x, 3, menu->title, 6, 13);
   
   if (menu->type == MENU_TYPE_GRID) {
      // draw grid layout
      for (int i = 0; i < menu->option_count; i++) {
         // int row = i / menu->grid_cols;
         // int col = i % menu->grid_cols;
         // int x = 10 + col * 10;
         // int y = 6 + row * 3;
         
         // int color = (i == menu->selected_option) ? 11 : 7;
         // !!! renderer_draw_string(x, y, menu->options[i].text, color, 13);
         
         if (i == menu->selected_option) {
            // !!! renderer_set_char(x - 2, y, '>', 11, 13);
         }
      }
   } else {
      // TODO: be able to show list and sublist at the same time when sublist is active
      // draw list layout
      for (int i = 0; i < menu->option_count; i++) {
         // int y = 6 + i * 2;
         // int color = (i == menu->selected_option) ? 11 : 7;
         
         char display_text[64];
         if (menu->type == MENU_TYPE_SETTINGS && menu->options[i].action == MENU_ACTION_SETTING_CHANGE) {
            snprintf(display_text, sizeof(display_text), "%s: %s", 
               menu->options[i].text, 
               menu->options[i].setting_values[menu->options[i].current_setting]);
         } else {
            strcpy(display_text, menu->options[i].text);
         }
       
         if (i == menu->selected_option) {
            // !!! renderer_set_char(8, y, '>', color, 13);
         }
         // !!! renderer_draw_string(10, y, display_text, color, 13);
      }
   }
}

void menu_destroy(void) {
   for (int i = 0; i < menu_count; i++) {
      if (menus[i]) {
         free(menus[i]);
         menus[i] = NULL;
      }
   }
   menu_count = 0;
   active_menu = NULL;
}

// always create top level menu first when making a new scene, then child menus after
Menu* menu_create(const char* title, MenuType type) {
   if (menu_count >= MAX_MENUS) {
      d_err("you're making too many menus!");
      return NULL;
   }
   
   Menu* menu = malloc(sizeof(Menu));
   if (d_dne(menu)) return NULL;
   
   strncpy(menu->title, title, MAX_MENU_TITLE_LEN - 1);
   menu->title[MAX_MENU_TITLE_LEN - 1] = '\0';
   menu->type = type;
   memset(menu->options, 0, sizeof(menu->options));
   menu->option_count = 0;
   menu->selected_option = 0;
   
   menu->grid_cols = 3; // default grid menu is 3x3
   menu->grid_rows = 3;
   menu->selected_col = 0;
   menu->selected_row = 0;
   
   menu->parent = NULL;
   memset(menu->children, 0, sizeof(menu->children));
   menu->is_active = false;
   
   menus[menu_count] = menu;
   menu_count++;
   
   return menu;
}

void menu_make_child(Menu* child, Menu* parent, int index) {
   if (!child || !parent) return;
   if (!parent->children[index]) {
      parent->children[index] = child;
      child->parent = parent;
   }
   else d_log("infertile");
}

void menu_add_option(Menu* menu, const char* text, MenuAction action, int action_data) {
   if (!menu || menu->option_count >= MAX_MENU_OPTIONS) return;
   
   MenuOption* option = &menu->options[menu->option_count];
   strncpy(option->text, text, MAX_OPTION_TEXT_LEN - 1);
   option->text[MAX_OPTION_TEXT_LEN - 1] = '\0';
   
   option->action = action;
   option->action_data = action_data;
   option->setting_count = 0;
   option->current_setting = 0;
   
   menu->option_count++;
}

void menu_add_setting_option(Menu* menu, const char* text, char* values[], int value_count, int default_value) {
   if (!menu || menu->option_count >= MAX_MENU_OPTIONS || default_value < value_count) return;
   
   MenuOption* option = &menu->options[menu->option_count]; // this line is so confusing
   strncpy(option->text, text, MAX_OPTION_TEXT_LEN - 1);
   option->text[MAX_OPTION_TEXT_LEN - 1] = '\0';
   
   option->action = MENU_ACTION_SETTING_CHANGE;
   option->action_data = 0;
   option->setting_count = value_count;
   option->current_setting = default_value;
   
   for (int i = 0; i < value_count; i++) {
      option->setting_values[i] = values[i];
   }
   
   menu->option_count++;
}

Menu* menu_get_active(void) {
   return active_menu;
}

// maybe menu_open()? to mirror menu_close()
void menu_set_active(Menu* menu) {
   if (active_menu) {
      active_menu->is_active = false;
   }
   active_menu = menu;
   if (menu) {
      menu->is_active = true;
      menu_update_display();
   }
}

// utility for cursor position
int menu_traverse(Menu* menu, int* counter, int target, bool set_cursor) {
   for (int i = 0; i < menu->option_count; i++) {
      // get cursor index
      if (!set_cursor && menu->is_active && menu->selected_option == i) {
         return *counter;
      }
      // set cursor index
      if (set_cursor && *counter == target) {
         active_menu = menu;
         menu->is_active = true;
         menu->selected_option = i;
         return 1; // found
      }

      (*counter)++;

      // recurse into child if exists
      Menu* child = menu->children[i];
      if (child) {
         int res = menu_traverse(child, counter, target, set_cursor);
         if (res) return res;
      }
   }
   return 0; // not found
}

int menu_get_cursor_position(void) {
   if (!menus[0]) return 0;
   int counter = 0;
   return menu_traverse(menus[0], &counter, -1, false);
}

void menu_set_cursor_position(int menu_position) {
   if (!menus[0]) return;

   for (int i = 0; i < menu_count; i++) {
      if (menus[i]) {
         menus[i]->is_active = false;
      }
   }

   int counter = 0;
   menu_traverse(menus[0], &counter, menu_position, true);
}

void menu_move_up(Menu* menu) {
   if (d_dne(menu)) return;
   
   if (menu->type == MENU_TYPE_GRID) {
      menu->selected_row--;
      if (menu->selected_row < 0) {
         menu->selected_row = menu->grid_rows - 1;
      }
      menu->selected_option = menu->selected_row * menu->grid_cols + menu->selected_col;
   } else {
      menu->selected_option--;
      if (menu->selected_option < 0) {
         menu->selected_option = menu->option_count - 1;
      }
   }
   menu_update_display();
}

void menu_move_down(Menu* menu) {
   if (!menu) return;
   
   if (menu->type == MENU_TYPE_GRID) {
      menu->selected_row++;
      if (menu->selected_row >= menu->grid_rows) {
         menu->selected_row = 0;
      }
      menu->selected_option = menu->selected_row * menu->grid_cols + menu->selected_col;
   } else {
      menu->selected_option++;
      if (menu->selected_option >= menu->option_count) {
         menu->selected_option = 0;
      }
   }
   menu_update_display();
}

void menu_move_left(Menu* menu) {
   if (!menu) return;
   
   if (menu->type == MENU_TYPE_GRID) {
      menu->selected_col--;
      if (menu->selected_col < 0) {
         menu->selected_col = menu->grid_cols - 1;
      }
      menu->selected_option = menu->selected_row * menu->grid_cols + menu->selected_col;
      menu_update_display();
   } else if (menu->type == MENU_TYPE_SETTINGS) {
      // change setting value
      MenuOption* option = &menu->options[menu->selected_option];
      if (option->action == MENU_ACTION_SETTING_CHANGE && option->setting_count > 0) {
         option->current_setting--;
         if (option->current_setting < 0) {
            option->current_setting = option->setting_count - 1;
         }
         menu_update_display();
         d_logv(2, "setting changed: %s = %s", option->text, option->setting_values[option->current_setting]);
      }
   } else if (menu->type == MENU_TYPE_LIST) {
      if (menu->parent) {
         menu_back(menu);
      }
   }
}

void menu_move_right(Menu* menu) {
   if (!menu) return;

   switch (menu->type) {
   case MENU_TYPE_GRID:
      menu->selected_col++;
      if (menu->selected_col >= menu->grid_cols) {
         menu->selected_col = 0;
      }
      menu->selected_option = menu->selected_row * menu->grid_cols + menu->selected_col;
      menu_update_display();
      break;
      
   case MENU_TYPE_SETTINGS:
      // Change setting value
      MenuOption* option = &menu->options[menu->selected_option];
      if (option->action == MENU_ACTION_SETTING_CHANGE && option->setting_count > 0) {
         option->current_setting++;
         if (option->current_setting >= option->setting_count) {
            option->current_setting = 0;
         }
         menu_update_display();
         d_logv(2, "setting changed: %s = %s", option->text, option->setting_values[option->current_setting]);
      }
      break;
   case MENU_TYPE_LIST:
      if (menu->options[menu->selected_option].action == MENU_ACTION_SUBMENU) {
         menu_select(menu);
      }
   }
}

extern SceneType scene_get_current_scene(void);
void menu_select(Menu* menu) {
   if (!menu || menu->selected_option >= menu->option_count || menu->selected_option < 0) return;
   
   MenuOption* option = &menu->options[menu->selected_option];
   SceneType current_scene = SCENE_MAX;
   current_scene = scene_get_current_scene();
   
   d_logv(2, "menu select: %s (action: %d)", option->text, option->action);
   
   switch (option->action) {
   case MENU_ACTION_NONE: break;
   case MENU_ACTION_SELECT:
      // maybe for selecting a character?
      break;
        
   case MENU_ACTION_CLOSE:
      // bye bye menu!
      menu_close(menu);
      break;
        
   case MENU_ACTION_BACK:
      // go to parent menu or scene pop
      menu_back(menu);
      break;
      
   case MENU_ACTION_SUBMENU:
      // check all menus to see which has parent that is the current active menu
      for (int i = 0; i < MAX_MENUS; i++) {
         if (menus[i] && menus[i]->parent && menus[i]->parent->is_active) {
            menu_set_active(menus[i]);
         }
      }
      break;
        
   case MENU_ACTION_SCENE_CHANGE:
      if (current_scene == SCENE_MAIN_MENU) {
         main_menu_handle_scene_change(option->action_data);
      } else if (current_scene == SCENE_CHARACTER_SELECT) {
         character_select_handle_scene_change(option->action_data);
      } else if (current_scene == SCENE_SETTINGS) {
         settings_handle_scene_change(option->action_data);
      } else d_log("didn't get all cases");
      break;
   
   case MENU_ACTION_GAME_SETUP:
      // handle different kinds of changes to device select
      if (current_scene == SCENE_MAIN_MENU) {
         main_menu_handle_game_setup(option->action_data);
      } else d_log("didn't get all cases. basterd.\n");
      break;
        
   case MENU_ACTION_MAX: 
   default:
      d_log("unhandled menu action: %d", option->action);
      break;
   }
}

void menu_back(Menu* menu) {
   if (!menu) return;
   
   if (menu->parent) {
      menu_set_active(menu->parent);
      return;
   }
   
   switch (scene_get_current_scene()) {
   case SCENE_MAIN_MENU:
      main_menu_handle_scene_change(SCENE_TITLE);
      break;
   case SCENE_SETTINGS:
      settings_handle_scene_change(SCENE_MAIN_MENU);
      break;
   case SCENE_CHARACTER_SELECT:
      character_select_handle_scene_change(SCENE_MAIN_MENU);
      break;
   case SCENE_GAMEPLAY:
      // if (pause_menu && !has_parent) gameplay_unpause(); // TODO
      break;
   default:
      d_log("hmm. curious");
      break;
   }
}

// active_menu = NULL; nothing displayed
// TODO: a way to determine how menus are displayed while inactive
void menu_close(Menu* menu) {
   if (!menu || menu->selected_option >= menu->option_count || menu->selected_option < 0) {
      d_log("huh???");
      return;
   }
   
   // go back to previous scene
   switch (scene_get_current_scene()) {
   case SCENE_GAMEPLAY:
      // gameplay_unpause(); // TODO
      active_menu = NULL;
      break;
   default:
      d_log("hmmm weird!!!");
      break;
   }
}

void menu_character_set(Menu* menu) {
   if (!menu || scene_get_current_scene() != SCENE_CHARACTER_SELECT) return;

   d_logv(2, "menu character set pressed");
}
