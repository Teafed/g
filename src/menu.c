#include "menu.h"
#include "debug.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// --- option helpers ---

// allocate the next slot in m->options. returns NULL and logs if full
static MenuOption* append_option(Menu* m, const char* label) {
   if (!m) return NULL;
   if (m->option_count >= MENU_MAX_OPTIONS) {
      d_err("menu '%s' is full (max %d); dropping option '%s'",
            m->title ? m->title : "?", MENU_MAX_OPTIONS,
            label ? label : "?");
      return NULL;
   }
   MenuOption* opt = &m->options[m->option_count++];
   memset(opt, 0, sizeof(*opt));
   opt->label   = label;
   opt->enabled = true;
   return opt;
}
// cursors won't move if everything is disabled
static bool any_option_enabled(const Menu* m) {
   for (int i = 0; i < m->option_count; i++) {
      if (m->options[i].enabled) return true;
   }
   return false;
}

// step a cursor by delta, wrapping at boundaries and skipping disabled options
static void step_cursor(const Menu* m, int* cursor, int delta) {
   if (m->option_count <= 0) return;

   int c = *cursor;
   do {
      c = ((c + delta) % m->option_count + m->option_count) % m->option_count;
   } while (!m->options[c].enabled);

   *cursor = c;
}

// --- choice option helpers ---

// used both for rendering and as the starting point when a player begins staging a new value
static int choice_current(const MenuOption* opt) {
   if (!opt->choice.get) return 0;
   return opt->choice.get(opt->choice.user);
}

// commit a staged value for a player on a choice option
static void choice_commit(MenuOption* opt, MenuPlayerState* ps) {
   if (ps->unconfirmed_choice < 0) return;
   if (opt->choice.set) {
      opt->choice.set(opt->choice.user, ps->unconfirmed_choice);
   }
   d_logv(2, "option '%s' set to '%s'", opt->label,
          opt->choice.choices[ps->unconfirmed_choice]);
   ps->unconfirmed_choice = -1;
}

static void choice_nudge(const MenuOption* opt, MenuPlayerState* ps, int delta) {
   int next = (ps->unconfirmed_choice < 0)
                 ? choice_current(opt) + delta
                 : ps->unconfirmed_choice + delta;
   if (next < 0) next = 0;
   if (next >= opt->choice.count) next = opt->choice.count - 1;
   ps->unconfirmed_choice = next;
}

// --- lifecycle ---

Menu* menu_create(LiRen* r, const char* title) {
   Menu* m = calloc(1, sizeof(Menu));
   if (!m) {
      d_err("failed to allocate Menu '%s'", title ? title : "?");
      return NULL;
   }
   m->title        = title;
   m->option_count = 0;
   m->layer        = lr_layer_create(r, false);

   // initialize per-player state: cursor at 0, no staged choice.
   for (int p = 0; p < MAX_PLAYERS; p++) {
      m->players[p].cursor             = 0;
      m->players[p].unconfirmed_choice = -1;
   }

   return m;
}

void menu_destroy(Menu* m, LiRen* r) {
   if (!m) return;
   if (m->layer != LR_INVALID_LAYER) {
      lr_layer_destroy(r, m->layer);
   }
   free(m);
}

// --- option builders ---

void menu_add_action(Menu* m, const char* label,
                     MenuActionFn on_select, void* user) {
   MenuOption* opt = append_option(m, label);
   if (!opt) return;
   opt->type             = MENU_OPT_ACTION;
   opt->action.on_select = on_select;
   opt->action.user      = user;
}

void menu_add_toggle(Menu* m, const char* label, bool* target) {
   if (!target) {
      d_err("menu_add_toggle('%s'): null target", label ? label : "?");
      return;
   }
   MenuOption* opt = append_option(m, label);
   if (!opt) return;
   opt->type          = MENU_OPT_TOGGLE;
   opt->toggle.target = target;
}

void menu_add_slider(Menu* m, const char* label,
                     int* target, int min, int max, int step) {
   if (!target) {
      d_err("menu_add_slider('%s'): null target", label ? label : "?");
      return;
   }
   MenuOption* opt = append_option(m, label);
   if (!opt) return;
   opt->type          = MENU_OPT_SLIDER;
   opt->slider.target = target;
   opt->slider.min    = min;
   opt->slider.max    = max;
   opt->slider.step   = step;
}

void menu_add_choice(Menu* m, const char* label,
                     const char* const* choices, int choice_count,
                     MenuChoiceGet get, MenuChoiceSet set, void* user) {
   if (!choices || choice_count <= 0 || !get || !set) {
      d_err("menu_add_choice('%s'): invalid args", label ? label : "?");
      return;
   }
   if (choice_count > MENU_MAX_CHOICES) choice_count = MENU_MAX_CHOICES;

   MenuOption* opt = append_option(m, label);
   if (!opt) return;
   opt->type          = MENU_OPT_CHOICE;
   opt->choice.choices = choices;
   opt->choice.count   = choice_count;
   opt->choice.get     = get;
   opt->choice.set     = set;
   opt->choice.user    = user;
}

// --- option mutation ---

void menu_set_option_enabled(Menu* m, int index, bool enabled) {
   if (!m || index < 0 || index >= m->option_count) return;
   m->options[index].enabled = enabled;

   // move cursor if it's on a disabled one
   if (!enabled && any_option_enabled(m)) {
      for (int p = 0; p < MAX_PLAYERS; p++) {
         if (m->players[p].cursor == index) {
            step_cursor(m, &m->players[p].cursor, 1);
         }
      }
   }
}

int menu_option_count(const Menu* m) {
   return m ? m->option_count : 0;
}

// --- input ---

void menu_handle_input(Menu* m, InputEvent e, InputState st, int device_id) {
   if (!m || !st.pressed) return;

   int player = input_get_player(device_id);
   if (player <= 0 || player > MAX_PLAYERS) return;
   player--;

   MenuPlayerState* ps  = &m->players[player];
   MenuOption*      opt = (ps->cursor >= 0 && ps->cursor < m->option_count)
                            ? &m->options[ps->cursor]
                            : NULL;

   switch (e) {
   case INPUT_UP:
      // while staging a choice, vertical movement is suppressed
      if (ps->unconfirmed_choice >= 0) break;
      if (any_option_enabled(m)) step_cursor(m, &ps->cursor, -1);
      break;

   case INPUT_DOWN:
      if (ps->unconfirmed_choice >= 0) break;
      if (any_option_enabled(m)) step_cursor(m, &ps->cursor, +1);
      break;

   case INPUT_LEFT:
      if (!opt || !opt->enabled) break;
      if (opt->type == MENU_OPT_CHOICE) {
         choice_nudge(opt, ps, -1);
      } else if (opt->type == MENU_OPT_SLIDER) {
         if (opt->slider.target && *opt->slider.target > opt->slider.min) {
            *opt->slider.target -= opt->slider.step;
            if (*opt->slider.target < opt->slider.min)
               *opt->slider.target = opt->slider.min;
         }
      }
      break;

   case INPUT_RIGHT:
      if (!opt || !opt->enabled) break;
      if (opt->type == MENU_OPT_CHOICE) {
         choice_nudge(opt, ps, +1);
      } else if (opt->type == MENU_OPT_SLIDER) {
         if (opt->slider.target && *opt->slider.target < opt->slider.max) {
            *opt->slider.target += opt->slider.step;
            if (*opt->slider.target > opt->slider.max)
               *opt->slider.target = opt->slider.max;
         }
      }
      break;

   case INPUT_A:
      if (!opt || !opt->enabled) break;
      switch (opt->type) {
      case MENU_OPT_ACTION:
         if (opt->action.on_select) opt->action.on_select(opt->action.user);
         break;
      case MENU_OPT_TOGGLE:
         if (opt->toggle.target) {
            *opt->toggle.target = !*opt->toggle.target;
            d_logv(2, "option '%s' toggled to %s", opt->label,
                   *opt->toggle.target ? "true" : "false");
         }
         break;
      case MENU_OPT_CHOICE:
         choice_commit(opt, ps);
         break;
      case MENU_OPT_SLIDER:
         // no-op for now
         break;
      }
      break;

   case INPUT_B:
      // cancel an in-progress choice stage
      if (opt && opt->type == MENU_OPT_CHOICE && ps->unconfirmed_choice >= 0) {
         ps->unconfirmed_choice = -1;
      }
      break;

   default:
      break;
   }
}

// --- rendering ---

// format one option's text into buffer. used by the default render path
static void format_option(const MenuOption* opt, char* buf, int buf_size) {
   if (!opt || !buf || buf_size <= 0) return;

   switch (opt->type) {
   case MENU_OPT_ACTION:
      snprintf(buf, buf_size, "%s", opt->label ? opt->label : "?");
      break;

   case MENU_OPT_TOGGLE:
      snprintf(buf, buf_size, "%s: %s",
               opt->label ? opt->label : "?",
               (opt->toggle.target && *opt->toggle.target) ? "ON" : "OFF");
      break;

   case MENU_OPT_SLIDER:
      snprintf(buf, buf_size, "%s: %d",
               opt->label ? opt->label : "?",
               opt->slider.target ? *opt->slider.target : 0);
      break;

   case MENU_OPT_CHOICE: {
      int current = choice_current(opt);
      if (current < 0 || current >= opt->choice.count) {
         snprintf(buf, buf_size, "%s: ???", opt->label ? opt->label : "?");
      } else {
         snprintf(buf, buf_size, "%s: %s",
                  opt->label ? opt->label : "?",
                  opt->choice.choices[current]);
      }
      break;
   }
   }
}

void menu_render(Menu* m, LiRen* r, int x, int y) {
   if (!m || !r) return;

   // once a font/text API is wired up, the title and options get drawn here
   lr_draw_fill(r, m->layer, lr_get_transparent_index(r));

   (void)x;
   (void)y;
   (void)format_option;

   // TODO: once the menu component owns a font handle (via menu_create
   // taking an LR_FontHandle, or via game_get_assets), draw title at
   // (x, y) and each option at (x, y + (i+1) * line_height). highlight
   // any option an assigned player has its cursor on (different highlight
   // colors per player).
}
