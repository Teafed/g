#ifndef DEBUG_H
#define DEBUG_H

#include "def.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

#define LOG_QUIET 0
#define LOG_NORMAL 1
#define LOG_VERBOSE 2
#define LOG_DEBUG 3

// logging
void    d_log(const char* fmt, ...); // appends \n unless string is empty
void    d_logl(const char* fmt, ...); // log without frame count or new line
void    d_logv(int level, const char* fmt, ...); // TODO: any way to check if i forgot to include an argument referenced in string?
#define say(...) d_logl(__VA_ARGS__) // shorthand

void    d__err(const char* func, const char* file, int line, const char* fmt, ...);
#define d_err(...) d__err(__func__, __FILE__, __LINE__, __VA_ARGS__)

// TODO: make this check multiple variables
// TODO: make a non-debug dne()?
bool    d__dne(void* ptr, const char* name, const char* func, const char* file, int line);
#define d_dne(ptr) d__dne((void*)(ptr), #ptr, __func__, __FILE__, __LINE__)

#include "renderer.h"
const char* d_name_rect(Rect* rect);

static inline void d__var_int(int x, const char* name) {
   printf("%s = %d\n", name, x);
}
static inline void d__var_uint(unsigned int x, const char* name) {
   printf("%s = %u\n", name, x);
}
static inline void d__var_float(double x, const char* name) {
   printf("%s = %f\n", name, x);
}
static inline void d__var_char(char x, const char* name) {
   printf("%s = \"%c\"\n", name, x);
}
static inline void d__var_str(const char* x, const char* name) {
   printf("%s = \"%s\"\n", name, x);
}
static inline void d__var_rect(Rect* x, const char* name) {
   printf("%s = %s\n", name, d_name_rect(x));
}
static inline void d__var_ptr(void* x, const char* name) {
   printf("%s = %p\n", name, x);
}
#define d_var(x) _Generic((x), \
   int: d__var_int, \
   unsigned int: d__var_uint, \
   ui8: d__var_uint, \
   float: d__var_float, \
   double: d__var_float, \
   char: d__var_char, \
   const char*: d__var_str, \
   Rect*: d__var_rect, \
   default: d__var_ptr \
)(x, #x)

// RENDERER
#include "renderer.h" // for DisplayResolution, WindowMode
const char* d_name_display_resolution(DisplayResolution res);
const char* d_name_resize_mode(ResizeMode mode);
const char* d_name_window_mode(WindowMode mode);
const char* d_name_system_data(SystemData data);
void d_print_renderer_dims(void);

// FILE
#include "file.h" // for FontType
const char* d_name_font(FontType type);

// TIMING
void d_print_performance_info(void);

// INPUT
#include "input.h" // for InputEvent, GameContext
const char* d_name_input_event(InputEvent e);
const char* d_name_game_context(GameContext context);
const char* d_name_input_raw_key(int key);
void d_print_all_mappings(void);
void d_print_current_input_state(int dev);
void d_print_devices(void);

// SCENE
#include "scene.h"
const char* d_name_scene_type(SceneType scene);
const char* d_name_game_mode_type(GameModeType type);

// MENU
#include "menu.h"
const char* d_name_menu_type(MenuType type);
const char* d_name_option_type(OptionType type);
const char* d_name_menu_action(MenuAction action);

#endif