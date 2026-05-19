#ifndef MENU_H
#define MENU_H

#include "input.h"
#include "text.h"
#include <liren/liren.h>
#include <stdbool.h>
#include <stdint.h>

#define MENU_MAX_OPTIONS 16
#define MENU_MAX_CHOICES 8

// option type tag. each tag selects one branch of MenuOption's union
typedef enum {
   MENU_OPT_ACTION, // press A -> callback
   MENU_OPT_TOGGLE, // press A -> flip a bool
   MENU_OPT_SLIDER, // L/R moves an int between min/max
   MENU_OPT_CHOICE, // L/R stages, A commits via getter/setter
} MenuOptionType;

// action option's on-select callback
typedef void (*MenuActionFn)(void* user);

// menu calls get() to render the current value and to seed the staged value
// menu calls set() once on commit (press A while a staged value is pending)
typedef int  (*MenuChoiceGet)(void* user);
typedef void (*MenuChoiceSet)(void* user, int value);

// single option in a menu.
// label is a TextBox so it can carry per-option font/color overrides.
// when label.font == LR_INVALID_FONT or label.color == TEXTBOX_COLOR_UNSET,
// menu_render falls back to the active MenuStyle's defaults.
typedef struct {
   TextBox        label;
   MenuOptionType type;
   bool           enabled;

   union {
      struct {  // MENU_OPT_ACTION
         MenuActionFn on_select;
         void*        user;
      } action;

      struct {  // MENU_OPT_TOGGLE
         bool* target;
      } toggle;

      struct {  // MENU_OPT_SLIDER
         int* target;
         int  min;
         int  max;
         int  step;
      } slider;

      struct {  // MENU_OPT_CHOICE
         const char* const* choices;
         int                count;
         MenuChoiceGet      get;
         MenuChoiceSet      set;
         void*              user;
      } choice;
   };
} MenuOption;

// per-player runtime state. one entry per slot in [0, MAX_PLAYERS)
typedef struct {
   int cursor;             // index into options[]; clamped to [0, option_count)
   int unconfirmed_choice; // staged value for the option under the cursor

   // direction-held tracking for menu_update's polled repeat path.
   // -1 = no direction held by this player.
   InputEvent held_dir;
} MenuPlayerState;

// menu-wide style. passed to menu_render. NULL means MENU_DEFAULT_STYLE
typedef struct {
   // defaults used when an option's TextBox has font/color unset
   LR_FontHandle default_font;
   uint8_t default_color;

   // layout in pixels
   int line_height;         // vertical gap between options
   int title_gap;           // extra gap between title and first option

   // colors (palette indices)
   uint8_t cursor_color_p1; // player 1 cursor glyph color
   uint8_t cursor_color_p2; // player 2 cursor glyph color
   uint8_t selected_color;  // option text color when any cursor is on it
   uint8_t disabled_color;  // option text color when option is disabled
   uint8_t staged_color;    // suffix color when value is staged-not-committed
} MenuStyle;

// global default style. defined in menu.c. callers can copy + modify it
extern const MenuStyle MENU_DEFAULT_STYLE;

// scenes own Menu* in their per-scene data struct.
// any scenes that want submenus have to track the active Menu* and swap it themselves.
typedef struct {
   TextBox title;
   MenuOption options[MENU_MAX_OPTIONS];
   int option_count;

   MenuPlayerState players[MAX_PLAYERS];

   LR_LayerHandle layer;
   bool wrap_around;
} Menu;

Menu* menu_create (LiRen* r, const char* title);
void  menu_destroy(Menu* m, LiRen* r);

void menu_add_action(Menu* m, const char* label,
                     MenuActionFn on_select, void* user);
void menu_add_toggle(Menu* m, const char* label, bool* target);
void menu_add_slider(Menu* m, const char* label,
                     int* target, int min, int max, int step);
void menu_add_choice(Menu* m, const char* label,
                     const char* const* choices, int choice_count,
                     MenuChoiceGet get, MenuChoiceSet set, void* user);

void menu_set_option_enabled(Menu* m, int index, bool enabled);
int  menu_option_count(const Menu* m);

// per-option style overrides. MenuStyle swaps do not change overrides
// pass LR_INVALID_FONT / TEXTBOX_COLOR_UNSET to clear an override
void menu_set_option_font(Menu* m, int index, LR_FontHandle font);
void menu_set_option_color(Menu* m, int index, int16_t color);

// title style overrides. same as above
void menu_set_title_font(Menu* m, LR_FontHandle font);
void menu_set_title_color(Menu* m, int16_t color);

// default cursor wrap is true
void menu_set_wrap_around(Menu* m, bool wrap);

// per-frame
// menu_handle_input: called once per InputEvent for edge actions
// (A, B, and the first press of UP/DOWN/LEFT/RIGHT). drains via the
// scene's input dispatcher.
//
// menu_update: called once per frame for polled hold-to-repeat nav
// on UP/DOWN/LEFT/RIGHT. uses input_pressed_repeat under the hood,
// so holding a direction repeats nav after an initial delay.
void menu_handle_input(Menu* m, InputEvent e, InputState st, int device_id);
void menu_update(Menu* m, float dt);
void menu_render(Menu* m, LiRen* r, int x, int y, const MenuStyle* style);

#endif
