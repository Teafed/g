#include "text.h"
#include "debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- internal helpers -------------------------------------------------------

// grow s so it can hold at least 'need' bytes (excluding the nul).
// no-op if already large enough.
static void str_grow(Str* s, int need) {
   if (!s) return;
   if (s->cap >= need + 1) return;

   int new_cap = s->cap > 0 ? s->cap : 16;
   while (new_cap < need + 1) new_cap *= 2;

   char* p = realloc(s->data, (size_t)new_cap);
   if (!p) {
      d_err("str_grow: realloc to %d bytes failed", new_cap);
      return;
   }
   s->data = p;
   s->cap  = new_cap;

   // guarantee nul-termination after a fresh alloc
   if (s->len == 0) s->data[0] = '\0';
}

// vsnprintf into Str at offset 'start'. truncates len to start first,
// then appends formatted output. handles re-fitting after measuring.
static void str_vprintf_at(Str* s, int start, const char* fmt, va_list ap) {
   if (!s || !fmt) return;

   // first measure with a copy of ap (vsnprintf consumes its va_list)
   va_list ap2;
   va_copy(ap2, ap);
   int needed = vsnprintf(NULL, 0, fmt, ap2);
   va_end(ap2);

   if (needed < 0) {
      d_err("str_vprintf_at: vsnprintf measurement failed");
      return;
   }

   str_grow(s, start + needed);
   if (s->cap < start + needed + 1) return; // grow failed; already logged

   int wrote = vsnprintf(s->data + start, (size_t)(s->cap - start), fmt, ap);
   if (wrote < 0) {
      d_err("str_vprintf_at: vsnprintf write failed");
      return;
   }
   s->len = start + wrote;
}


// --- StrView ----------------------------------------------------------------

StrView strview_lit(const char* literal) {
   StrView v = { literal, literal ? (int)strlen(literal) : 0 };
   return v;
}

StrView strview_of(const char* data, int len) {
   StrView v = { data, len < 0 ? 0 : len };
   return v;
}

bool strview_eq(StrView a, StrView b) {
   if (a.len != b.len) return false;
   if (a.len == 0)     return true;
   return memcmp(a.data, b.data, (size_t)a.len) == 0;
}

int strview_to_cstr(StrView v, char* buf, int buf_size) {
   if (!buf || buf_size <= 0) return 0;
   int n = v.len;
   if (n > buf_size - 1) n = buf_size - 1;   // reserve room for nul
   if (n > 0 && v.data) memcpy(buf, v.data, (size_t)n);
   buf[n] = '\0';
   return n;
}


// --- Str --------------------------------------------------------------------

Str str_init(void) {
   Str s = { NULL, 0, 0 };
   return s;
}

Str str_from(const char* src) {
   Str s = str_init();
   if (src) str_set(&s, src);
   return s;
}

Str str_from_fmt(const char* fmt, ...) {
   Str s = str_init();
   va_list ap;
   va_start(ap, fmt);
   str_vprintf_at(&s, 0, fmt, ap);
   va_end(ap);
   return s;
}

void str_set(Str* s, const char* src) {
   if (!s) return;
   if (!src) { str_clear(s); return; }
   int n = (int)strlen(src);
   str_grow(s, n);
   if (s->cap < n + 1) return;
   memcpy(s->data, src, (size_t)n + 1);   // includes nul
   s->len = n;
}

void str_setf(Str* s, const char* fmt, ...) {
   if (!s) return;
   va_list ap;
   va_start(ap, fmt);
   str_vprintf_at(s, 0, fmt, ap);
   va_end(ap);
}

void str_append(Str* s, const char* src) {
   if (!s || !src) return;
   int n = (int)strlen(src);
   str_grow(s, s->len + n);
   if (s->cap < s->len + n + 1) return;
   memcpy(s->data + s->len, src, (size_t)n + 1);
   s->len += n;
}

void str_appendf(Str* s, const char* fmt, ...) {
   if (!s) return;
   va_list ap;
   va_start(ap, fmt);
   str_vprintf_at(s, s->len, fmt, ap);
   va_end(ap);
}

void str_clear(Str* s) {
   if (!s) return;
   s->len = 0;
   if (s->data) s->data[0] = '\0';
}

void str_free(Str* s) {
   if (!s) return;
   free(s->data);
   s->data = NULL;
   s->len  = 0;
   s->cap  = 0;
}

StrView str_view(const Str* s) {
   if (!s || !s->data) return strview_of(NULL, 0);
   return strview_of(s->data, s->len);
}

void str_reserve(Str* s, int cap) {
   if (!s || cap <= 0) return;
   str_grow(s, cap);
}


// --- TextBox ----------------------------------------------------------------

void textbox_init(TextBox* tb, const char* text,
                  LR_FontHandle font, int16_t color) {
   if (!tb) return;
   tb->text            = str_from(text);
   tb->font            = font;
   tb->color           = color;
   tb->chars_revealed  = -1;   // "show all" -- typer not active
   tb->delay_remaining = 0.0f;
   tb->char_delay      = 0.0f;
}

void textbox_set_text(TextBox* tb, const char* text) {
   if (!tb) return;
   str_set(&tb->text, text);
   // resetting text mid-typer is the caller's problem; we just clamp the
   // reveal count to the new length so we don't read past the end.
   if (tb->chars_revealed >= 0 && tb->chars_revealed > tb->text.len) {
      tb->chars_revealed = tb->text.len;
   }
}

void textbox_set_textf(TextBox* tb, const char* fmt, ...) {
   if (!tb) return;
   va_list ap;
   va_start(ap, fmt);
   str_vprintf_at(&tb->text, 0, fmt, ap);
   va_end(ap);
   if (tb->chars_revealed >= 0 && tb->chars_revealed > tb->text.len) {
      tb->chars_revealed = tb->text.len;
   }
}

void textbox_set_font(TextBox* tb, LR_FontHandle font) {
   if (tb) tb->font = font;
}

void textbox_set_color(TextBox* tb, int16_t color) {
   if (tb) tb->color = color;
}

void textbox_start_typer(TextBox* tb, float seconds_per_char) {
   if (!tb) return;
   tb->char_delay      = seconds_per_char;
   tb->delay_remaining = seconds_per_char;
   tb->chars_revealed  = (seconds_per_char > 0.0f) ? 0 : -1;
}

void textbox_start_typer_cps(TextBox* tb, float chars_per_sec) {
   if (!tb) return;
   if (chars_per_sec <= 0.0f) {
      textbox_start_typer(tb, 0.0f);
      return;
   }
   textbox_start_typer(tb, 1.0f / chars_per_sec);
}

void textbox_skip_typer(TextBox* tb) {
   if (!tb) return;
   tb->chars_revealed  = -1;
   tb->delay_remaining = 0.0f;
}

bool textbox_typer_done(const TextBox* tb) {
   if (!tb) return true;
   if (tb->char_delay <= 0.0f) return true;
   if (tb->chars_revealed < 0) return true;
   return tb->chars_revealed >= tb->text.len;
}

void textbox_update(TextBox* tb, float dt) {
   // cheap no-op when typer is off or finished
   if (!tb)                    return;
   if (tb->char_delay <= 0.0f) return;
   if (tb->chars_revealed < 0) return;
   if (tb->chars_revealed >= tb->text.len) {
      tb->chars_revealed = -1;    // promote to terminal "show all"
      return;
   }

   tb->delay_remaining -= dt;
   while (tb->delay_remaining <= 0.0f && tb->chars_revealed < tb->text.len) {
      tb->chars_revealed++;
      // each character has its own delay budget. later, an escape-code
      // system can override this per-char (e.g. punctuation pauses) by
      // adding extra time onto delay_remaining as it reveals.
      tb->delay_remaining += tb->char_delay;
   }
   if (tb->chars_revealed >= tb->text.len) {
      tb->chars_revealed  = -1;
      tb->delay_remaining = 0.0f;
   }
}

void textbox_render(const TextBox* tb, LiRen* r, LR_LayerHandle layer,
                    int x, int y,
                    LR_FontHandle fallback_font, uint8_t fallback_color) {
   if (!tb || !r) return;
   if (!tb->text.data || tb->text.len == 0) return;

   LR_FontHandle font = (tb->font != LR_INVALID_FONT) ? tb->font : fallback_font;
   if (font == LR_INVALID_FONT) return;   // nothing to render with

   uint8_t color = (tb->color != TEXTBOX_COLOR_UNSET)
                      ? (uint8_t)tb->color
                      : fallback_color;

   // full-reveal: draw the Str directly, no copy
   if (tb->chars_revealed < 0) {
      lr_draw_text(r, layer, font, x, y, color, tb->text.data);
      return;
   }

   // typer mid-reveal: build a nul-terminated prefix in scratch
   int n = tb->chars_revealed;
   if (n <= 0) return;
   if (n > TEXTBOX_RENDER_SCRATCH - 1) n = TEXTBOX_RENDER_SCRATCH - 1;

   char scratch[TEXTBOX_RENDER_SCRATCH];
   memcpy(scratch, tb->text.data, (size_t)n);
   scratch[n] = '\0';
   lr_draw_text(r, layer, font, x, y, color, scratch);
}

void textbox_free(TextBox* tb) {
   if (!tb) return;
   str_free(&tb->text);
   tb->font            = LR_INVALID_FONT;
   tb->color           = TEXTBOX_COLOR_UNSET;
   tb->chars_revealed  = -1;
   tb->delay_remaining = 0.0f;
   tb->char_delay      = 0.0f;
}
