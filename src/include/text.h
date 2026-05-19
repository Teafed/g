#ifndef TEXT_H
#define TEXT_H

#include <liren/liren.h>
#include <stdbool.h>
#include <stdint.h>

// --- sentinel for "color not set" -------------------------------------------
//
// uint8_t palette indices have no spare value, so TextBox stores color as
// int16_t. TEXTBOX_COLOR_UNSET means "fall back to the caller-provided
// default at render time".
//
// TODO: once liren grows palette-by-name access (or any "this is not a
// valid color" primitive of its own), move this sentinel into liren as
// LR_INVALID_COLOR / LR_COLOR_NONE and drop it from here. it's a TextBox
// concern only because liren doesn't yet expose one.
#define TEXTBOX_COLOR_UNSET ((int16_t)-1)


// --- StrView: borrowed, immutable view --------------------------------------
//
// zero allocation. caller owns the backing storage. cheap to pass by value.
// note: NOT guaranteed nul-terminated; always use len. to pass a StrView
// to a const char* API (printf, lr_draw_text), copy to a local buffer
// via strview_to_cstr first.
typedef struct {
   const char* data;
   int         len;
} StrView;

// wrap a c-literal. computes len with strlen. cheap but not free at call time.
StrView strview_lit(const char* literal);
StrView strview_of (const char* data, int len);
bool    strview_eq (StrView a, StrView b);

// copy v's bytes into buf and nul-terminate. truncates if buf_size is too
// small (still nul-terminates). returns bytes written, NOT counting the nul.
// buf_size must be > 0.
int strview_to_cstr(StrView v, char* buf, int buf_size);


// --- Str: owned, heap-allocated, growable -----------------------------------
//
// always nul-terminated when data != NULL (len does not include the nul).
// after str_free, fields are zeroed; safe to re-use the struct.
//
// NOTE: utf-8 is not handled here. Str is byte-oriented. when fonts grow
// utf-8 support we either add a parallel utf-8 path or a flag on the font;
// Str itself stays as-is.
typedef struct {
   char* data;
   int   len;
   int   cap;
} Str;

Str  str_init    (void);
Str  str_from    (const char* src);
Str  str_from_fmt(const char* fmt, ...);

void str_set    (Str* s, const char* src);
void str_setf   (Str* s, const char* fmt, ...);
void str_append (Str* s, const char* src);
void str_appendf(Str* s, const char* fmt, ...);
void str_clear  (Str* s);              // keep capacity, len = 0
void str_free   (Str* s);

StrView str_view(const Str* s);        // borrow as view

// reserve at least 'cap' bytes (excluding the implicit nul). idempotent.
void str_reserve(Str* s, int cap);


// --- TextBox: owns a Str, has optional font/color and typer state -----------
//
// font == LR_INVALID_FONT      -> "ask the caller" (render fallback path)
// color == TEXTBOX_COLOR_UNSET -> "ask the caller"
//
// typer timing is PER-CHARACTER: char_delay is seconds-until-next-reveal.
// this makes it natural to add per-character delay variation later (e.g.
// escape codes that pause longer on punctuation) without refactoring.
// callers that prefer chars/sec can use textbox_start_typer_cps which
// internally computes 1/cps.
//
// callers that don't use typer effects never call textbox_update;
// char_delay stays 0 and textbox_render shows all characters.
typedef struct {
   Str           text;
   LR_FontHandle font;
   int16_t       color;

   // typer state.
   // char_delay == 0       -> no typer; render shows all text
   // chars_revealed == -1  -> typer finished (terminal state, "show all")
   // chars_revealed >= 0   -> typer active; this many chars revealed so far
   int   chars_revealed;
   float delay_remaining;   // seconds until next char is revealed
   float char_delay;        // default seconds-per-character
} TextBox;

void textbox_init(TextBox* tb, const char* text,
                  LR_FontHandle font, int16_t color);

void textbox_set_text  (TextBox* tb, const char* text);
void textbox_set_textf (TextBox* tb, const char* fmt, ...);

void textbox_set_font  (TextBox* tb, LR_FontHandle font);
void textbox_set_color (TextBox* tb, int16_t color);

// typer
void textbox_start_typer    (TextBox* tb, float seconds_per_char);
void textbox_start_typer_cps(TextBox* tb, float chars_per_sec); // 1/cps wrapper
void textbox_skip_typer     (TextBox* tb);   // jump to fully revealed
bool textbox_typer_done     (const TextBox* tb);

// per-frame: cheap no-op when char_delay == 0 or typer is done.
void textbox_update(TextBox* tb, float dt);

// render with fallbacks: if tb->font == LR_INVALID_FONT, use fallback_font;
// if tb->color == TEXTBOX_COLOR_UNSET, use fallback_color.
// when typer is active, only the first chars_revealed bytes are drawn.
//
// NOTE: liren's lr_draw_text takes a nul-terminated string. when the typer
// is mid-reveal we copy the prefix into a small scratch buffer; for that
// reason text longer than TEXTBOX_RENDER_SCRATCH gets truncated at the
// edge during typer reveal (full-reveal path uses the Str directly).
void textbox_render(const TextBox* tb, LiRen* r, LR_LayerHandle layer,
                    int x, int y,
                    LR_FontHandle fallback_font, uint8_t fallback_color);

void textbox_free(TextBox* tb);

// max chars rendered when typer is mid-reveal. text longer than this still
// renders fully once the typer finishes (no copy needed).
#define TEXTBOX_RENDER_SCRATCH 256

#endif
