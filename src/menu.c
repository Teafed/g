#include "menu.h"
#include "renderer.h"
#include "file.h"
#include "scene.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static Menu* g_active_menu = NULL;
static Menu* last_active_menu = NULL; // to track when active menu changes
static void format_option_text(MenuOption* opt, char* buffer, int buffer_size);
static void process_action(MenuAction action, int data, int player);
static void hide_all_layers(Menu* menu);
static void menu_draw_main(Menu* menu, int chain_position);
static void menu_draw_settings(Menu* menu);
static void menu_draw_charsel(Menu* menu);

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
   if (!state.pressed && (state.held && state.duration < 0.3f)) return;
   
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
      if (menu->options[*selected].type == OPTION_TYPE_CHOICE) {
         if (menu->options[*selected].unconfirmed_choice != -1) break;
      }
      // go to prev option
      do {
         // TODO: assuming charsel has row size 3 rn, round it up
         int amt = menu->options[*selected].type == OPTION_TYPE_CHARSEL ? 3 : 1;
         (*selected) -= amt;
         if (*selected < 0) *selected = menu->option_count + (*selected); // TODO: i don't think this accounts for disableds
      } while (!menu->options[*selected].enabled && menu->option_count > 1);
      break;
      
   case INPUT_DOWN:
      if (!any_enabled) break;
      if (menu->options[*selected].type == OPTION_TYPE_CHOICE) {
         if (menu->options[*selected].unconfirmed_choice != -1) break;
      }
      // go to next option
      do {
         int amt = menu->options[*selected].type == OPTION_TYPE_CHARSEL ? 3 : 1;
         (*selected) += amt;
         if (*selected >= menu->option_count) *selected = (*selected) - menu->option_count;
      } while (!menu->options[*selected].enabled && menu->option_count > 1);
      break;
      
   case INPUT_LEFT:
      if (!any_enabled) break;
      if (menu->options[*selected].type == OPTION_TYPE_CHARSEL) {
         do {
            int amt = *selected % 3 == 0 ? (-3 + 1) : 1;
            *selected -= amt;
            if (*selected < 0) *selected += amt;
         } while (!menu->options[*selected].enabled && menu->option_count > 1);
      }
      else if (menu->options[*selected].type == OPTION_TYPE_CHOICE) {
         MenuOption* opt = &menu->options[*selected];
         if (opt->current_choice) {
            if (*opt->current_choice > 0 && opt->unconfirmed_choice == -1) {
               opt->unconfirmed_choice = *opt->current_choice - 1;
            } else if (opt->unconfirmed_choice > 0) {
               opt->unconfirmed_choice--;
            }
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
      if (menu->options[*selected].type == OPTION_TYPE_CHARSEL) {
         do {
            int amt = (*selected + 1) % 3 == 0 ? (-3 + 1) : 1;
            *selected += amt;
            if (*selected >= menu->option_count) *selected -= amt;
         } while (!menu->options[*selected].enabled && menu->option_count > 1);
      }
      else if (menu->options[*selected].type == OPTION_TYPE_CHOICE) {
         MenuOption* opt = &menu->options[*selected];
         if (opt->current_choice) {
            if (*opt->current_choice < opt->choice_count - 1 && opt->unconfirmed_choice == -1) {
               opt->unconfirmed_choice = *opt->current_choice + 1;
            } else if (opt->unconfirmed_choice < opt->choice_count - 1) {
               if (opt->unconfirmed_choice != -1) opt->unconfirmed_choice++;
            }
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
            
         case OPTION_TYPE_TOGGLE:
            if (opt->toggle_value) {
               *opt->toggle_value = !(*opt->toggle_value);
               d_logv(2, "option %s changed to %s", opt->text, (*opt->toggle_value) ? "TRUE" : "FALSE");
            }
            break;
            
         case OPTION_TYPE_CHOICE:
            if (opt->current_choice && opt->unconfirmed_choice != -1) {
               // *opt->current_choice = opt->unconfirmed_choice;
               int new_value = opt->unconfirmed_choice;
               opt->unconfirmed_choice = -1;
               d_log("option %s changed to %s", opt->text, opt->choices[new_value]);
               if (opt->on_change) {
                  opt->on_change(new_value);
               } else {
                  *opt->current_choice = new_value; // fallback for non-callback options
               }
            }
            break;
            
         case OPTION_TYPE_SLIDER:
            // maybe open a detailed slider interface?
            // or just do nothing for now
            break;
            
         case OPTION_TYPE_SUBMENU:
            menu_navigate_to_child(menu, *selected, player);
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
      } else if (menu->options[*selected].enabled) {
         if (menu->options[*selected].unconfirmed_choice != -1) {
            menu->options[*selected].unconfirmed_choice = -1;
         }
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
         menu_draw_main(menu, chain_length - 1 - i); // pass position in chain
         break;
      case MENU_TYPE_SETTINGS:
         menu_draw_settings(menu);
         break;
      case MENU_TYPE_CHARSEL:
         menu_draw_charsel(menu);
         break;
      case MENU_TYPE_PAUSE:
         // menu_draw_pause(menu);
         break;
      default:
         return;
      }
   }
}

void menu_destroy(Menu* menu) {
   if (d_dne(menu)) return;
   
   // destroy layer
   renderer_destroy_layer(menu->layer_handle);
   
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

void menu_add_choice_option(Menu* menu, const char* text, char* choices[], int count, int* current_ptr, SettingChange on_change) {
   if (!menu || menu->option_count >= MAX_MENU_OPTIONS || !choices || !current_ptr) return;
   if (count > MAX_CHOICES) count = MAX_CHOICES;
   
   MenuOption* opt = &menu->options[menu->option_count];
   strncpy(opt->text, text, MAX_OPTION_TEXT_LEN - 1);
   opt->text[MAX_OPTION_TEXT_LEN - 1] = '\0';
   opt->type = OPTION_TYPE_CHOICE;
   opt->enabled = true;
   
   for (int i = 0; i < count; i++) { opt->choices[i] = choices[i]; }
   opt->choice_count = count;
   opt->unconfirmed_choice = -1;
   opt->current_choice = current_ptr;
   opt->on_change = on_change;
   
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
         if (opt->unconfirmed_choice != -1) {
            snprintf(buffer, buffer_size, "%s: %s *", opt->text, 
                     opt->choices[opt->unconfirmed_choice]);
         }
         else {
            snprintf(buffer, buffer_size, "%s: %s", opt->text, 
                     opt->choices[*opt->current_choice]);
         }
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
      d_logv(3, "SceneType = %s", d_name_scene_type(data));
      if (data < 0 || data >= SCENE_MAX) {
         d_log("invalid scene");
         return;
      }
      scene_change_to(data);
      break;
      
   case MENU_ACTION_GAME_SETUP:
      // setup game with mode specified in data
      // TODO: this!!!!!!
      d_logv(3, "GameModeType = %s", d_name_game_mode_type(data));
      scene_start_game_session(data);
      scene_change_to(SCENE_CHARACTER_SELECT);
      break;
      
   case MENU_ACTION_BACK:
      // handle back action
      if (g_active_menu && g_active_menu->parent) {
         menu_navigate_to_parent(g_active_menu, player);
      }
      // TODO: if it's a pause menu or other floating type menu, close if !g_active_menu->parent
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

static void menu_draw_main(Menu* menu, int chain_position) {
   if (!menu) return;
   
   // clear the layer
   renderer_draw_fill(menu->layer_handle, PALETTE_TRANSPARENT);
   
   // calculate position based on chain position
   int base_x = 50 + (chain_position * 200); // 200 pixels between columns
   int base_y = 50;
   
   // draw menu title
   renderer_draw_string(menu->layer_handle, FONT_ACER_8_8, menu->title, base_x, base_y, 0);
   
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

static void menu_draw_settings(Menu* menu) {
   if (!menu) return;
   
   // clear the menu's layer
   renderer_draw_fill(menu->layer_handle, PALETTE_TRANSPARENT);
   
   int base_x = 50;
   int base_y = 50;
   
   // draw menu title
   renderer_draw_string(menu->layer_handle, FONT_ACER_8_8, menu->title, base_x, base_y, 0);
   
   // draw options
   for (int i = 0; i < menu->option_count; i++) {
      uint8_t color = (i == menu->selected_option[0]) ? 7 : 1; // highlight selected
      if (!menu->options[i].enabled) {
         color = 3; // disabled color
      }

      int current_y = base_y + 30 + (i * 20);

      // draw option text
      char display_text[128];
      format_option_text(&menu->options[i], display_text, sizeof(display_text));
      renderer_draw_string(menu->layer_handle, FONT_ACER_8_8, display_text, base_x, current_y, color);
   }
}

static void menu_draw_charsel(Menu* menu) {
   if (!menu) return;
   
   // clear the layer
   renderer_draw_fill(menu->layer_handle, PALETTE_TRANSPARENT);
   
   int base_x = 50;
   int base_y = 50;
   
   // grid configuration - adjust these as needed
   int cols = 3; // characters per row
   int cell_width = 120;
   int cell_height = 60;
   int grid_start_x = base_x;
   int grid_start_y = base_y + 40; // space below title
   
   // draw character grid
   for (int i = 0; i < menu->option_count; i++) {
      int row = i / cols;
      int col = i % cols;
      
      int cell_x = grid_start_x + (col * cell_width);
      int cell_y = grid_start_y + (row * cell_height);
      
      uint8_t color = 1; // default color
      bool is_selected = false;
      
      // check if any player has this option selected
      for (int player = 1; player <= MAX_PLAYERS; player++) {
         // if (input_get_player_device(player) == -1) continue;
         if (i == menu->selected_option[player - 1]) {
            is_selected = true;
            color = 19 + (player - 1); // start at blue-sky
            break;
         }
      }
      
      if (!menu->options[i].enabled) color = 3; // disabled color
      
      // draw character name centered in cell
      int text_x = cell_x + (cell_width / 2) - ((strlen(menu->options[i].text) * 8) / 2);  // assuming 8px wide font
      int text_y = cell_y + (cell_height / 2) - 4; // assuming 8px tall font
      
      renderer_draw_string(menu->layer_handle, FONT_ACER_8_8, menu->options[i].text,
                          text_x, text_y, color);
      // TODO
      if (is_selected && menu->options[i].enabled) {
         // renderer_draw_rect_outline(menu->layer_handle, cell_x - 2, cell_y - 2, 
         //                           cell_width + 4, cell_height + 4, color);
      }
   }
}