#ifndef MENU_H
#define MENU_H

#include "input.h"
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

// single option in a menu
typedef struct {
   const char*    label;      // static string, not copied
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
   int cursor; // index into options[]; clamped to [0, option_count)
   int unconfirmed_choice; // staged value for the option under the cursor
} MenuPlayerState;

// scenes own Menu* in their per-scene data struct
// any scenes that want submenus have to track the active Menu* and swap it themselves
typedef struct {
   const char* title;
   MenuOption  options[MENU_MAX_OPTIONS];
   int         option_count;

   MenuPlayerState players[MAX_PLAYERS];

   LR_LayerHandle layer;
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

// per-frame
void menu_handle_input(Menu* m, InputEvent e, InputState st, int device_id);
void menu_render      (Menu* m, LiRen* r, int x, int y);

#endif
