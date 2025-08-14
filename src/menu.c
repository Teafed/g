#include "menu.h"
#include "renderer.h"
#include "file.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static Menu* g_active_menu = NULL;
static Menu* last_active_menu = NULL; // to track when active menu changes
static void format_option_text(MenuOption* opt, char* buffer, int buffer_size);
static void process_action(MenuAction action, int data, int player);
static void hide_all_layers(Menu* menu);

// CORE FUNCTIONS
void menu_system_init(void) {
   // memset(g_menu_memory, 0, sizeof(g_menu_memory));
   g_active_menu = NULL;
}

Menu* menu_create(MenuType type, const char* title) {
   Menu* menu = malloc(sizeof(Menu));
   if (d_dne(menu)) return NULL;
   
   strncpy(menu->title, title, MAX_MENU_TITLE_LEN - 1);
   menu->title[MAX_MENU_TITLE_LEN - 1] = '\0';
   
   menu->type = type;
   menu->option_count = 0;
   
   for (int i = 0; i < MAX_PLAYERS; i++) {
      menu->selected_option[i] = 0;
   }

   menu->layer_handle = renderer_create_layer(false);
   menu->parent = NULL;

   return menu;
}

void menu_handle_input(InputEvent event, InputState state, int device_id) {
   (void) state;
   int player = input_get_player(device_id);
   if (!g_active_menu || player <= 0 || player > MAX_PLAYERS) return;

   player--; // use as index
   Menu* menu = g_active_menu;
   int* selected = &menu->selected_option[player];

   // make sure at least one option is enabled
   bool any_enabled = false;
   for (int i = 0; i < menu->option_count; i++) {
      if (menu->options[i].enabled) {
         any_enabled = true;
         break;
      }
   }
   
   switch (event) {
   case INPUT_UP:
      if (!any_enabled) break;
      do {
         (*selected)--;
         if (*selected < 0) *selected = menu->option_count - 1;
      } while (!menu->options[*selected].enabled && menu->option_count > 1);
      break;
      
   case INPUT_DOWN:
      if (!any_enabled) break;
      do {
         (*selected)++;
         if (*selected >= menu->option_count) *selected = 0;
      } while (!menu->options[*selected].enabled && menu->option_count > 1);
      break;
      
   case INPUT_LEFT:
      if (!any_enabled) break;
      // handle settings navigation
      if (menu->options[*selected].type == OPTION_TYPE_CHOICE) {
         MenuOption* opt = &menu->options[*selected];
         if (opt->current_choice && *opt->current_choice > 0) {
            (*opt->current_choice)--;
         }
      } else if (menu->options[*selected].type == OPTION_TYPE_SLIDER) {
         MenuOption* opt = &menu->options[*selected];
         if (opt->slider_value && *opt->slider_value > opt->min_value) {
            *opt->slider_value -= opt->step;
            if (*opt->slider_value < opt->min_value) {
               *opt->slider_value = opt->min_value;
            }
         }
      }
      break;
      
   case INPUT_RIGHT:
      if (!any_enabled) break;
      // handle settings navigation
      if (menu->options[*selected].type == OPTION_TYPE_CHOICE) {
         MenuOption* opt = &menu->options[*selected];
         if (opt->current_choice && *opt->current_choice < opt->choice_count - 1) {
            (*opt->current_choice)++;
         }
      } else if (menu->options[*selected].type == OPTION_TYPE_SLIDER) {
         MenuOption* opt = &menu->options[*selected];
         if (opt->slider_value && *opt->slider_value < opt->max_value) {
            *opt->slider_value += opt->step;
            if (*opt->slider_value > opt->max_value) {
               *opt->slider_value = opt->max_value;
            }
         }
      }
      break;
      
   case INPUT_A:
      if (menu->options[*selected].enabled) {
         MenuOption* opt = &menu->options[*selected];
         
         switch (opt->type) {
         case OPTION_TYPE_ACTION:
            process_action(opt->action, opt->action_data, player);
            break;
            
         case OPTION_TYPE_SUBMENU:
            menu_navigate_to_child(menu, *selected, player);
            break;
            
         case OPTION_TYPE_TOGGLE:
            if (opt->toggle_value) {
               *opt->toggle_value = !(*opt->toggle_value);
            }
            break;
            
         case OPTION_TYPE_CHOICE:
            if (opt->current_choice) {
               *opt->current_choice = (*opt->current_choice + 1) % opt->choice_count;
            }
            break;
            
         case OPTION_TYPE_SLIDER:
            // maybe open a detailed slider interface?
            // or just do nothing for now
            break;
            
         case OPTION_TYPE_CHARSEL:
            // TODO: this
            break;
            
         default:
            d_log("unhandled option type: %s", d_name_option_type(opt->type));
         }
      }
      break;
      
   case INPUT_B:
      if (menu->parent) {
         menu_navigate_to_parent(menu, player);
      }
      break;
      
   default:
      break;
   }
}

void menu_render(Menu* root) {
   if (!root) return;
   
   int max_depth = 32; // max menus to render in a chain
   Menu* chain[max_depth];
   int chain_length = 0;

   Menu* current = g_active_menu;
   while (current && chain_length < max_depth) {
      chain[chain_length++] = current;
      current = current->parent;
   }
   // prolly need to include an additional check on chain rendering, not all menu types are gonna show parent menus
   // actually i should probably do this in a different way because what about rendering charsel menus?
   // probably figure out the design of that screen first, but maybe would be good to support players being able to control individual submenu
   
   if (g_active_menu != last_active_menu) {
      hide_all_layers(root);
      for (int i = chain_length - 1; i >= 0; i--) {
         Menu* menu = chain[i];
         renderer_set_layer_visible(menu->layer_handle, true);
      }
      last_active_menu = g_active_menu;
   }
   
   // render chain from root to active (reverse order)
   for (int i = chain_length - 1; i >= 0; i--) {
      Menu* menu = chain[i];
      
      // render based on menu type
      switch (menu->type) {
      case MENU_TYPE_MAIN:
         menu_render_main_type(menu, chain_length - 1 - i); // pass position in chain
         break;
      case MENU_TYPE_CHARSEL:
         // menu_render_charsel_type(menu);
         break;
      case MENU_TYPE_SETTINGS:
         // menu_render_settings_type(menu);
         break;
      case MENU_TYPE_PAUSE:
         // menu_render_pause_type(menu);
         break;
      default:
         return;
      }
   }
}

void menu_draw_menu(Menu* menu) { // TODO: customize
   if (!menu) return;
   
   // clear the menu's layer
   renderer_draw_fill(menu->layer_handle, PALETTE_TRANSPARENT);

   // case MENU_TYPE_MAIN
   int title_x = 50;
   int title_y = 30;
   uint8_t title_color = 6;

   int start_y = 80;
   int line_width = 180;
   int line_height = 25;
   
   // draw title
   renderer_draw_string(menu->layer_handle, FONT_ACER_8_8, menu->title, title_x, title_y, title_color); // orange-teaf
   
   for (int i = 0; i < menu->option_count; i++) {
      MenuOption* opt = &menu->options[i];
      int y = start_y + (i * line_height);
      uint8_t color = opt->enabled ? 11 : 7;
      
      // highlight selected option for player 1
      bool is_selected = (menu->selected_option[0] == i); // TODO: highlighting for both players
      if (is_selected) {
         // selection box
         SDL_Rect select_rect = {title_x - 5, y - 3, line_width, line_height};
         renderer_draw_rect(menu->layer_handle, select_rect, 13); // red-dark
         // selected text color
         color = 8; // brown-light
      }
      
      // draw option text
      char display_text[128];
      format_option_text(opt, display_text, sizeof(display_text));
      renderer_draw_string(menu->layer_handle, FONT_ACER_8_8, display_text, title_x, y, color);
   }
}

void menu_destroy(Menu* menu) {
   if (d_dne(menu)) return;
   
   // destroy layer
   renderer_destroy_layer(menu->layer_handle);
   
   // TODO: make this destroy any child menus as well
   
   free(menu);
   menu = NULL;
}

void menu_system_cleanup(void) {
   g_active_menu = NULL;
   // note: menus are destroyed individually in x_scene_destroy()
}

// OPTION MANAGEMENT
void menu_add_action_option(Menu* menu, const char* text, MenuAction action, int data) {
   if (!menu || menu->option_count >= MAX_MENU_OPTIONS) return;
   
   MenuOption* opt = &menu->options[menu->option_count];
   strncpy(opt->text, text, MAX_OPTION_TEXT_LEN - 1);
   opt->text[MAX_OPTION_TEXT_LEN - 1] = '\0';
   opt->type = OPTION_TYPE_ACTION;
   opt->enabled = true;
   
   opt->action = action;
   opt->action_data = data;
   
   menu->option_count++;
}

void menu_add_submenu_option(Menu* menu, const char* text, Menu* submenu) {
   if (!menu || menu->option_count >= MAX_MENU_OPTIONS) return;
   
   MenuOption* opt = &menu->options[menu->option_count];
   strncpy(opt->text, text, MAX_OPTION_TEXT_LEN - 1);
   opt->text[MAX_OPTION_TEXT_LEN - 1] = '\0';
   opt->type = OPTION_TYPE_SUBMENU;
   opt->enabled = true;
   
   if (submenu) {
      opt->child = submenu;
      submenu->parent = menu;
   }  else {
      d_err("submenu doesn't exist");
   }
   
   menu->option_count++;
}

void menu_add_toggle_option(Menu* menu, const char* text, bool* toggle_ptr) {
   if (!menu || menu->option_count >= MAX_MENU_OPTIONS || !toggle_ptr) return;
   
   MenuOption* opt = &menu->options[menu->option_count];
   strncpy(opt->text, text, MAX_OPTION_TEXT_LEN - 1);
   opt->text[MAX_OPTION_TEXT_LEN - 1] = '\0';
   opt->type = OPTION_TYPE_TOGGLE;
   opt->enabled = true;
   
   opt->toggle_value = toggle_ptr;
   
   menu->option_count++;
}

void menu_add_choice_option(Menu* menu, const char* text, char* choices[], int count, int* current_ptr) {
   if (!menu || menu->option_count >= MAX_MENU_OPTIONS || !choices || !current_ptr) return;
   if (count > MAX_CHOICES) count = MAX_CHOICES;
   
   MenuOption* opt = &menu->options[menu->option_count];
   strncpy(opt->text, text, MAX_OPTION_TEXT_LEN - 1);
   opt->text[MAX_OPTION_TEXT_LEN - 1] = '\0';
   opt->type = OPTION_TYPE_CHOICE;
   opt->enabled = true;
   
   for (int i = 0; i < count; i++) { opt->choices[i] = choices[i]; }
   opt->choice_count = count;
   opt->current_choice = current_ptr;
   
   menu->option_count++;
}

void menu_add_slider_option(Menu* menu, const char* text, int* value_ptr, int min, int max, int step) {
   if (!menu || menu->option_count >= MAX_MENU_OPTIONS || !value_ptr) return;
   
   MenuOption* opt = &menu->options[menu->option_count];
   strncpy(opt->text, text, MAX_OPTION_TEXT_LEN - 1);
   opt->text[MAX_OPTION_TEXT_LEN - 1] = '\0';
   opt->type = OPTION_TYPE_SLIDER;
   opt->enabled = true;
   
   opt->slider_value = value_ptr;
   opt->min_value = min;
   opt->max_value = max;
   opt->step = step;
   
   menu->option_count++;
}

void menu_add_charsel_option(Menu* menu, const char* text, int character_index) {
   if (!menu || menu->option_count >= MAX_MENU_OPTIONS) return;
   
   MenuOption* opt = &menu->options[menu->option_count];
   strncpy(opt->text, text, MAX_OPTION_TEXT_LEN - 1);
   opt->text[MAX_OPTION_TEXT_LEN - 1] = '\0';
   opt->type = OPTION_TYPE_CHARSEL;
   opt->enabled = true;

   opt->character_index = character_index;
   
   menu->option_count++;
}

void menu_set_option_enabled(Menu* menu, int option_index, bool enabled) {
   if (!menu || option_index < 0 || option_index >= menu->option_count) return;
   menu->options[option_index].enabled = enabled;
}

void menu_remove_option(Menu* menu, int option_index) {
   if (!menu || option_index < 0 || option_index >= menu->option_count) return;
   
   for (int i = option_index; i < menu->option_count - 1; i++) {
      menu->options[i] = menu->options[i + 1];
   }
   
   menu->option_count--;
   
   // adjust selected options if they're beyond the removed option
   for (int p = 0; p < MAX_PLAYERS; p++) {
      if (menu->selected_option[p] > option_index) {
         menu->selected_option[p]--;
      } else if (menu->selected_option[p] >= menu->option_count && menu->option_count > 0) {
         // if removed selected option was last one, select new last option
         menu->selected_option[p] = menu->option_count - 1;
      }
   }
}

// NAVIGATION AND STATE
void menu_make_child(Menu* child, Menu* parent, int parent_option_index) {
   if (!child || !parent || parent_option_index < 0 || parent_option_index >= parent->option_count) return;
   
   child->parent = parent;
}

void menu_navigate_to_child(Menu* menu, int option_index, int player) {
   if (!menu || option_index < 0 || option_index >= menu->option_count) return;
   if (player < 0 || player >= MAX_PLAYERS) return;
   
   MenuOption* opt = &menu->options[option_index];
   if (opt->type == OPTION_TYPE_SUBMENU && opt->child) {
      menu_set_active(opt->child);
   }
}

void menu_navigate_to_parent(Menu* menu, int player) {
   if (!menu || !menu->parent) return;
   if (player < 0 || player >= MAX_PLAYERS) return;
   
   menu_set_active(menu->parent);
}

void menu_set_active(Menu* menu) {
   if (d_dne(menu)) return;

   g_active_menu = menu;
   d_logv(2, "g_active_menu = %s", menu->title);
}


void menu_save_current_state(void) { // TODO
   if (!g_active_menu) return;
   
   for (int p = 0; p < MAX_PLAYERS; p++) {
//      if (/* player p made a selection */) {
//         menu_save_selection(g_active_menu->title, p, 
//                           g_active_menu->selected_option[p], 
//                           /* device_id for player p */);
//      }
   }
}

void menu_restore_state(Menu* menu) { // TODO
   if (!menu) return;
   
   for (int p = 0; p < MAX_PLAYERS; p++) {
//      int device_id;
//      int saved_option = menu_restore_selection(menu->title, p, &device_id);
//      if (saved_option >= 0 && saved_option < menu->option_count) {
//         menu->selected_option[p] = saved_option;
//      }
   }
}

// INTERNAL
static void format_option_text(MenuOption* opt, char* buffer, int buffer_size) {
   if (!buffer) return;
   switch (opt->type) {
   case OPTION_TYPE_ACTION:
   case OPTION_TYPE_SUBMENU:
      snprintf(buffer, buffer_size, "%s", opt->text);
      break;
      
   case OPTION_TYPE_TOGGLE:
      snprintf(buffer, buffer_size, "%s: %s", opt->text, 
              (opt->toggle_value && *opt->toggle_value) ? "ON" : "OFF");
      break;
      
   case OPTION_TYPE_CHOICE:
      if (opt->current_choice && *opt->current_choice < opt->choice_count) {
         snprintf(buffer, buffer_size, "%s: %s", opt->text, 
                 opt->choices[*opt->current_choice]);
      } else {
         snprintf(buffer, buffer_size, "%s: ???", opt->text);
      }
      break;
      
   case OPTION_TYPE_SLIDER:
      snprintf(buffer, buffer_size, "%s: %d", opt->text, 
              opt->slider_value ? *opt->slider_value : 0);
      break;
      
   default:
      snprintf(buffer, buffer_size, "%s", opt->text);
      break;
   }
}

// placeholder for action processing - implement this
static void process_action(MenuAction action, int data, int player) {
   switch (action) {
   case MENU_ACTION_SCENE_CHANGE:
      // change to scene specified in data
      // TODO: this!!!!!!
      d_logv(2, "SceneType = %s", d_name_scene_type(data));
      break;
      
   case MENU_ACTION_GAME_SETUP:
      // setup game with mode specified in data
      // TODO: this!!!!!!
      d_logv(2, "GameModeType = %s", d_name_game_mode_type(data));
      break;
      
   case MENU_ACTION_BACK:
      // handle back action
      if (g_active_menu && g_active_menu->parent) {
         menu_navigate_to_parent(g_active_menu, player);
      }
      break;
      
   case MENU_ACTION_QUIT:
      // handle quit
      d_logv(2, "MenuAction = %s", d_name_menu_action(action));
      break;
      
   default:
      break;
   }
}

static void hide_all_layers(Menu* menu) {
   if (!menu) return;
   
   renderer_set_layer_visible(menu->layer_handle, false);
   
   for (int i = 0; i < menu->option_count; i++) {
      if (menu->options[i].type == OPTION_TYPE_SUBMENU) {
         hide_all_layers(menu->options[i].child);
      }
   }
}

void menu_render_main_type(Menu* menu, int chain_position) {
   if (!menu) return;
   
   // clear the layer
   renderer_draw_fill(menu->layer_handle, PALETTE_TRANSPARENT);
   
   // calculate position based on chain position
   int base_x = 50 + (chain_position * 200); // 200 pixels between columns
   int base_y = 50;
   
   // draw menu title
   renderer_draw_string(menu->layer_handle, FONT_ACER_8_8, menu->title, 
                       base_x, base_y, 0);
   
   // draw options
   for (int i = 0; i < menu->option_count; i++) {
      uint8_t color = (i == menu->selected_option[0]) ? 7 : 1; // highlight selected
      if (!menu->options[i].enabled) {
         color = 3; // disabled color
      }
      
      renderer_draw_string(menu->layer_handle, FONT_ACER_8_8, menu->options[i].text,
                          base_x, base_y + 30 + (i * 20), color);
   }
}