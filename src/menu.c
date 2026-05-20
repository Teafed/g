#include "menu.h"
#include "debug.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// hold-to-repeat timings for menu navigation
#define MENU_REPEAT_DELAY    0.4f
#define MENU_REPEAT_INTERVAL 0.1f

// scratch for formatted option line
#define MENU_LINE_SCRATCH 128

// --- default style ---

const MenuStyle MENU_DEFAULT_STYLE = {
   .default_font     = LR_INVALID_FONT, // must be filled in by caller
   .default_color    = 15,              // assume white-ish in default palette
   .line_height      = 12,              // ~8x8 font + 4px gap
   .title_gap        = 8,
   .cursor_color_p1  = 14,              // yellow-ish
   .cursor_color_p2  = 12,              // red-ish
   .selected_color   = 15,              // bright/white
   .disabled_color   = 8,               // grey
   .staged_color     = 11,              // cyan-ish; distinct from committed
};

// --- option helpers ---

// allocate the next slot in m->options. returns NULL and logs if full.
// initializes label as an unset-font/color TextBox carrying the bare label.
static MenuOption* append_option(Menu* m, const char* label) {
   if (!m) return NULL;
   if (m->option_count >= MENU_MAX_OPTIONS) {
      d_err("menu '%s' is full (max %d); dropping option '%s'",
            (m->title.text.data && m->title.text.len > 0)
                ? m->title.text.data : "?",
            MENU_MAX_OPTIONS, label ? label : "?");
      return NULL;
   }
   MenuOption* opt = &m->options[m->option_count++];
   memset(opt, 0, sizeof(*opt));
   textbox_init(&opt->label, label, LR_INVALID_FONT, TEXTBOX_COLOR_UNSET);
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

// step a cursor by delta, optionally wrapping at boundaries. skip disabled options.
static void step_cursor(const Menu* m, int* cursor, int delta) {
   if (m->option_count <= 0) return;

   int c = *cursor;
   if (m->wrap_around) {
      // wrap mode: mod into range, then skip disabled
      do {
         c = ((c + delta) % m->option_count + m->option_count) % m->option_count;
      } while (!m->options[c].enabled);
   } else {
      // sticky mode: clamp, then walk past disabled in the requested direction
      int next = c + delta;
      while (next >= 0 && next < m->option_count && !m->options[next].enabled) {
         next += delta;
      }
      if (next < 0 || next >= m->option_count) return;
      c = next;
   }

   *cursor = c;
}

// --- choice option helpers --------------------------------------------------

// used both for rendering and as a starting point when a player begins staging a new value
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
   d_logv(2, "option '%s' set to '%s'",
          opt->label.text.data ? opt->label.text.data : "?",
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

// LEFT/RIGHT on slider/choice. used by both edge input and polled repeat
// returns true if the option type accepted the nudge
static bool nudge_horizontal(MenuOption* opt, MenuPlayerState* ps, int delta) {
   if (!opt || !opt->enabled) return false;
   if (opt->type == MENU_OPT_CHOICE) {
      choice_nudge(opt, ps, delta);
      return true;
   }
   if (opt->type == MENU_OPT_SLIDER && opt->slider.target) {
      int* t = opt->slider.target;
      if (delta < 0 && *t > opt->slider.min) {
         *t -= opt->slider.step;
         if (*t < opt->slider.min) *t = opt->slider.min;
         return true;
      }
      if (delta > 0 && *t < opt->slider.max) {
         *t += opt->slider.step;
         if (*t > opt->slider.max) *t = opt->slider.max;
         return true;
      }
   }
   return false;
}

// --- lifecycle ---

Menu* menu_create(LiRen* r, const char* title) {
   Menu* m = calloc(1, sizeof(Menu));
   if (!m) {
      d_err("failed to allocate Menu '%s'", title ? title : "?");
      return NULL;
   }
   textbox_init(&m->title, title, LR_INVALID_FONT, TEXTBOX_COLOR_UNSET);
   m->option_count = 0;
   m->layer        = lr_layer_create(r, false);
   m->wrap_around  = true;
   m->on_back      = NULL;
   m->on_back_user = NULL;

   // initialize per-player state
   for (int p = 0; p < MAX_PLAYERS; p++) {
      m->players[p].cursor             = 0;
      m->players[p].unconfirmed_choice = -1;
      m->players[p].held_dir           = (InputEvent)-1;
      m->player_active[p]              = MENU_PLAYER_AUTO;
   }

   return m;
}

void menu_destroy(Menu* m, LiRen* r) {
   if (!m) return;
   textbox_free(&m->title);
   for (int i = 0; i < m->option_count; i++) {
      textbox_free(&m->options[i].label);
   }
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
   opt->type           = MENU_OPT_CHOICE;
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

void menu_set_option_font(Menu* m, int index, LR_FontHandle font) {
   if (!m || index < 0 || index >= m->option_count) return;
   textbox_set_font(&m->options[index].label, font);
}

void menu_set_option_color(Menu* m, int index, int16_t color) {
   if (!m || index < 0 || index >= m->option_count) return;
   textbox_set_color(&m->options[index].label, color);
}

void menu_set_title_font(Menu* m, LR_FontHandle font) {
   if (!m) return;
   textbox_set_font(&m->title, font);
}

void menu_set_title_color(Menu* m, int16_t color) {
   if (!m) return;
   textbox_set_color(&m->title, color);
}

void menu_set_wrap_around(Menu* m, bool wrap) {
   if (!m) return;
   m->wrap_around = wrap;
}

void menu_set_on_back(Menu* m, MenuOnBackFn fn, void* user) {
   if (!m) return;
   m->on_back      = fn;
   m->on_back_user = user;
}

void menu_set_player_active(Menu* m, int player, MenuPlayerActive mode) {
   if (!m || player < 0 || player >= MAX_PLAYERS) return;
   m->player_active[player] = mode;
}

// returns true if player p's cursor should render this frame
// AUTO: render only if input has a device assigned to player (p+1)
// FORCE_ON / FORCE_OFF: override
static bool should_render_cursor(const Menu* m, int p) {
   switch (m->player_active[p]) {
   case MENU_PLAYER_FORCE_ON:  return true;
   case MENU_PLAYER_FORCE_OFF: return false;
   case MENU_PLAYER_AUTO:
   default:
      return input_get_player_device(p + 1) != -1;
   }
}

// --- input: edge events ----

void menu_handle_input(Menu* m, InputEvent e, InputState st, int device_id) {
   if (!m) return;

   int player = input_get_player(device_id);
   if (player <= 0 || player > MAX_PLAYERS) return;
   player--;

   MenuPlayerState* ps  = &m->players[player];
   MenuOption*      opt = (ps->cursor >= 0 && ps->cursor < m->option_count)
                            ? &m->options[ps->cursor]
                            : NULL;

   // track direction-held state on every event (press and release), so
   // menu_update knows what to poll. clear on release, set on press
   if (e == INPUT_UP || e == INPUT_DOWN || e == INPUT_LEFT || e == INPUT_RIGHT) {
      if (st.pressed) ps->held_dir = e;
      else if (ps->held_dir == e) ps->held_dir = (InputEvent)-1;
   }

   if (!st.pressed) return;

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
      nudge_horizontal(opt, ps, -1);
      break;

   case INPUT_RIGHT:
      nudge_horizontal(opt, ps, +1);
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
            d_logv(2, "option '%s' toggled to %s",
                   opt->label.text.data ? opt->label.text.data : "?",
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
      } else if (m->on_back) {
         m->on_back(m->on_back_user);
      }
      break;

   default:
      break;
   }
}

// --- input: polled hold-to-repeat ---
//
// for each player whose assigned device exists, poll input_pressed_repeat
// on the direction that player has currently held. UP/DOWN move the cursor
// (suppressed while staging); LEFT/RIGHT nudge slider/choice values.
//
// note: held_dir tracking is maintained by menu_handle_input on press/release.
// menu_update doesn't update held_dir itself; it only acts on what's already
// recorded
void menu_update(Menu* m, float dt) {
   if (!m) return;
   (void)dt; // input_pressed_repeat consults its own internal timer

   for (int p = 0; p < MAX_PLAYERS; p++) {
      MenuPlayerState* ps = &m->players[p];
      InputEvent dir = ps->held_dir;
      if (dir == (InputEvent)-1) continue;

      // resolve the device for this player slot
      int dev = input_get_player_device(p + 1);
      if (dev < 0) continue;

      if (!input_pressed_repeat(dir, dev,
                                MENU_REPEAT_DELAY,
                                MENU_REPEAT_INTERVAL)) continue;

      MenuOption* opt = (ps->cursor >= 0 && ps->cursor < m->option_count)
                          ? &m->options[ps->cursor]
                          : NULL;

      switch (dir) {
      case INPUT_UP:
         if (ps->unconfirmed_choice >= 0) break;
         if (any_option_enabled(m)) step_cursor(m, &ps->cursor, -1);
         break;
      case INPUT_DOWN:
         if (ps->unconfirmed_choice >= 0) break;
         if (any_option_enabled(m)) step_cursor(m, &ps->cursor, +1);
         break;
      case INPUT_LEFT:
         nudge_horizontal(opt, ps, -1);
         break;
      case INPUT_RIGHT:
         nudge_horizontal(opt, ps, +1);
         break;
      default:
         break;
      }
   }
}

// --- rendering --------------------------------------------------------------

// format the suffix portion of an option (everything after the label).
// returns true if a suffix was written. staged_out gets set true if the
// suffix represents an as-yet-uncommitted staged value (i.e. should be
// drawn in style->staged_color rather than the option's normal color).
//
// staging_player: -1 if no player is staging this option; otherwise the
// player whose unconfirmed_choice should be displayed. only one player
// can stage a given option at a time (per Q7: only the player on the
// option can stage).
static bool format_suffix(const MenuOption* opt, int staging_player,
                          const MenuPlayerState* players,
                          char* buf, int buf_size, bool* staged_out) {
   *staged_out = false;
   if (!opt || !buf || buf_size <= 0) return false;

   switch (opt->type) {
   case MENU_OPT_ACTION:
      return false;

   case MENU_OPT_TOGGLE:
      snprintf(buf, buf_size, ": %s",
               (opt->toggle.target && *opt->toggle.target) ? "ON" : "OFF");
      return true;

   case MENU_OPT_SLIDER:
      snprintf(buf, buf_size, ": %d",
               opt->slider.target ? *opt->slider.target : 0);
      return true;

   case MENU_OPT_CHOICE: {
      int idx;
      if (staging_player >= 0 &&
          players[staging_player].unconfirmed_choice >= 0) {
         idx = players[staging_player].unconfirmed_choice;
         *staged_out = true;
      } else {
         idx = choice_current(opt);
      }
      if (idx < 0 || idx >= opt->choice.count) {
         snprintf(buf, buf_size, ": ???");
      } else {
         snprintf(buf, buf_size, ": %s", opt->choice.choices[idx]);
      }
      return true;
   }
   }
   return false;
}

// resolve the effective font for a TextBox: its own font if set, otherwise style default
static LR_FontHandle resolve_font(const TextBox* tb, const MenuStyle* style) {
   if (tb->font != LR_INVALID_FONT) return tb->font;
   return style->default_font;
}

// resolve the effective color for an option in priority order:
// 1. disabled       -> disabled_color
// 2. cursor on this -> selected_color
// 3. tb->color set  -> tb->color
// 4. otherwise      -> style default
static uint8_t resolve_option_color(const MenuOption* opt,
                                    bool cursor_on,
                                    const MenuStyle* style) {
   if (!opt->enabled) return style->disabled_color;
   if (cursor_on)     return style->selected_color;
   if (opt->label.color != TEXTBOX_COLOR_UNSET) return (uint8_t)opt->label.color;
   return style->default_color;
}

// find the player (if any) staging this option
static int find_staging_player(const Menu* m, int option_index) {
   for (int p = 0; p < MAX_PLAYERS; p++) {
      if (m->players[p].cursor == option_index &&
          m->players[p].unconfirmed_choice >= 0) {
         return p;
      }
   }
   return -1;
}

// fetch glyph width for an option's effective font; falls back to 8
static int option_glyph_w(LiRen* r, const MenuOption* opt,
                          const MenuStyle* style) {
   LR_FontHandle f = (opt->label.font != LR_INVALID_FONT)
                        ? opt->label.font : style->default_font;
   if (f == LR_INVALID_FONT) return 8;
   int w = 0;
   lr_get_glyph_size(r, f, &w, NULL);
   return w > 0 ? w : 8;
}

// compute the length of an option line in characters (label + suffix)
static int option_line_chars(const MenuOption* opt) {
   if (!opt) return 0;
   int label_chars = opt->label.text.len;

   char suffix[MENU_LINE_SCRATCH];
   bool staged_unused = false;
   // pass staging_player = -1 so we always get the committed-value form;
   // staging only adds at most a couple chars and isn't worth measuring.
   MenuPlayerState dummy[MAX_PLAYERS];
   memset(dummy, 0, sizeof(dummy));
   if (format_suffix(opt, -1, dummy, suffix, sizeof(suffix), &staged_unused)) {
      return label_chars + (int)strlen(suffix);
   }
   return label_chars;
}

int menu_get_width(const Menu* m, const MenuStyle* style) {
   if (!m) return 0;
   if (!style) style = &MENU_DEFAULT_STYLE;
   int max_chars = 0;
   if (m->title.text.data) max_chars = m->title.text.len;
   for (int i = 0; i < m->option_count; i++) {
      int n = option_line_chars(&m->options[i]);
      if (n > max_chars) max_chars = n;
   }
   
   // TODO: query glyph width from liren. most fonts are width 8 but still
   return max_chars * 8;
}

int menu_get_height(const Menu* m, const MenuStyle* style) {
   if (!m) return 0;
   if (!style) style = &MENU_DEFAULT_STYLE;

   int h = m->option_count * style->line_height;
   if (m->title.text.data && m->title.text.len > 0) {
      h += style->line_height + style->title_gap;
   }
   return h;
}

void menu_render(Menu* m, LiRen* r, int x, int y, const MenuStyle* style) {
   if (!m || !r) return;
   if (!style) style = &MENU_DEFAULT_STYLE;

   // clear the menu's layer each frame; scenes don't need to clear it themselves
   lr_draw_fill(r, m->layer, lr_get_transparent_index(r));

   // glyph dim for default font, used for cursor offset
   int default_gw = 8;
   if (style->default_font != LR_INVALID_FONT) {
      lr_get_glyph_size(r, style->default_font, &default_gw, NULL);
      if (default_gw <= 0) default_gw = 8;
   }

   int cy = y;

   // title (if non-empty)
   if (m->title.text.data && m->title.text.len > 0) {
      textbox_render(&m->title, r, m->layer, x, cy,
                     style->default_font, style->default_color);
      cy += style->line_height + style->title_gap;
   }

   // options
   for (int i = 0; i < m->option_count; i++) {
      MenuOption* opt = &m->options[i];
      int oy = cy + i * style->line_height;

      bool any_cursor_here = false;
      for (int p = 0; p < MAX_PLAYERS; p++) {
         if (m->players[p].cursor == i && should_render_cursor(m, p)) {
            any_cursor_here = true;
            break;
         }
      }

      // resolve label font + color
      LR_FontHandle font  = resolve_font(&opt->label, style);
      uint8_t       color = resolve_option_color(opt, any_cursor_here, style);
      int           gw    = option_glyph_w(r, opt, style);

      // cursor glyphs (one per player whose cursor is here AND is rendered),
      // drawn to the left of the option. p1 first, p2 next to it if both.
      int cursor_x = x - default_gw - 2;
      LR_FontHandle cursor_font = (font != LR_INVALID_FONT)
                                     ? font : style->default_font;
      if (cursor_font != LR_INVALID_FONT) {
         for (int p = 0; p < MAX_PLAYERS; p++) {
            if (m->players[p].cursor != i) continue;
            if (!should_render_cursor(m, p)) continue;
            uint8_t cc = (p == 0) ? style->cursor_color_p1
                                  : style->cursor_color_p2;
            lr_draw_text(r, m->layer, cursor_font, cursor_x, oy, cc, ">");
            cursor_x += default_gw;
         }
      }

      // label
      if (font != LR_INVALID_FONT && opt->label.text.data) {
         lr_draw_text(r, m->layer, font, x, oy, color, opt->label.text.data);
      }

      // suffix
      int staging_player = find_staging_player(m, i);
      char suffix[MENU_LINE_SCRATCH];
      bool staged = false;
      if (format_suffix(opt, staging_player, m->players,
                        suffix, sizeof(suffix), &staged)) {
         uint8_t suffix_color = staged ? style->staged_color : color;
         int suffix_x = x + opt->label.text.len * gw;
         if (font != LR_INVALID_FONT) {
            lr_draw_text(r, m->layer, font, suffix_x, oy, suffix_color, suffix);
         }
      }
   }
}
