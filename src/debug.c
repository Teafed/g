#include "debug.h"
#include "file.h"
#include "input.h"
#include "timing.h"

int LOG_VERBOSITY = LOG_NORMAL;

// YEAH
void d_log(const char* fmt, ...) {
   if (LOG_VERBOSITY == LOG_QUIET) return;
   uint32_t frame_count = timing_get_frame_count();
   if (fmt[0] == '\0') { printf("[%d] ", frame_count); return; }
   printf("[%d] ", frame_count);
   va_list args;
   va_start(args, fmt);
   vprintf(fmt, args);
   printf("\n");
   va_end(args);
}

void d_logl(const char* fmt, ...) {
   if (LOG_VERBOSITY == LOG_QUIET) return;
   va_list args;
   va_start(args, fmt);
   vprintf(fmt, args);
   va_end(args);
}

void d_logv(int level, const char* fmt, ...) {
   if (LOG_VERBOSITY < level) return;
   uint32_t frame_count = timing_get_frame_count();
   if (fmt[0] == '\0') { printf("[%d] ", frame_count); return; }
   printf("[%d] ", frame_count);
   va_list args;
   va_start(args, fmt);
   vprintf(fmt, args);
   printf("\n");
   va_end(args);
}

void d__err(const char* func, const char* file, int line, const char* fmt, ...) {
   fprintf(stderr, "[%d] ", timing_get_frame_count());
   va_list args;
   va_start(args, fmt);
   fprintf(stderr, "\033[1;31m[ERR]\033[0m ");
   vfprintf(stderr, fmt, args);
   fprintf(stderr, "\n    at %s (%s:%d)\n", func, file, line);
   va_end(args);
}

bool d__dne(void* ptr, const char* name, const char* func, const char* file, int line) {
   if (ptr == NULL) {
      fprintf(stderr, "[%d] ", timing_get_frame_count());
      fprintf(stderr, "\033[1;31m[DNE]\033[0m %s is NULL!\n", name);
      fprintf(stderr, "    at %s (%s:%d)\n", func, file, line);
      return true;
   }
   return false;
}

const char* d_name_rect(Rect* rect) {
   static char buffer[64];
   snprintf(buffer, sizeof(buffer), "[ %d, %d, %d, %d ]", rect->x, rect->y, rect->w, rect->h);
   return buffer;
}

// RENDERER
const char* d_name_display_resolution(DisplayResolution res) {
   static const char* names[] = { "RES_VGA", "RES_FWVGA" };
   return (res >= 0 && res < 2) ? names[res] : "UNKNOWN!";
}

const char* d_name_resize_mode(ResizeMode mode) {
   static const char* names[] = { "RESIZE_FIT", "RESIZE_FIXED" };
   return (mode >= 0 && mode < 2) ? names[mode] : "UNKNOWN!";
}

const char* d_name_display_mode(DisplayMode mode) {
   static const char* names[] = {
      "DISPLAY_WINDOWED",
      "DISPLAY_BORDERLESS",
      "DISPLAY_FULLSCREEN"
   };
   return (mode >= 0 && mode < 3) ? names[mode] : "UNKNOWN!";
}

const char* d_name_system_data(SystemData data) {
   static const char* names[] = {
      "SYS_CURRENT_FPS",
      "SYS_AVG_FPS",
      "SYS_LAYER_COUNT",
      "SYS_MAX"
   };
   return (data >= 0 && data < SYS_MAX) ? names[data] : "UNKNOWN!";
}

void d_print_renderer_dims(void) {
   const RendererState* g_renderer = renderer_get_debug_state();
   Rect unit_map = g_renderer->unit_map;
   Rect window_map = g_renderer->window_map;
   d_log("");
   d_logl("---------\n");
   d_logl("RENDERER DIMS\n");
   d_var(g_renderer->window_surface->w);
   d_var(g_renderer->window_surface->h);
   d_var(g_renderer->composite_surface->w);
   d_var(g_renderer->composite_surface->h);
   d_logl("display_resolution = %s", d_name_display_resolution(g_renderer->display_resolution)); d_logl("\n");
   d_logl("unit_map =           %s", d_name_rect(&unit_map)); d_logl("\n");
   d_logl("window_map =         %s", d_name_rect(&window_map)); d_logl("\n");
   d_logl("display_mode =       %s", d_name_display_mode(g_renderer->display_mode)); d_logl("\n");
   d_var(g_renderer->last_windowed_width);
   d_var(g_renderer->last_windowed_height);
   for (int i = 0; i < g_renderer->layer_count; i++) {
      d_logl("layer %d: w = %d, h = %d", i, g_renderer->layers[i].surface->w, g_renderer->layers[i].surface->h);
      d_logl("\n");
   }
}

// FILE
const char* d_name_font(FontType type) {
   static const char* names[] = {
      "font_Acer710-CGA_8x8.bmp",   // FONT_ACER_8_8
      "font_Compis_8x16.bmp",       // FONT_COMPIS_8_16
      "font_Sharp-PC3K_8x8.bmp",    // FONT_SHARP_8_8
      "font_Verite_8x8.bmp",        // FONT_VERITE_8_8
      "font_Verite_9x8.bmp"         // FONT_VERITE_9_8
   };
   return (type >= 0 && type < FONT_MAX) ? names[type] : "UNKNOWN!";
}

// TIMING
void d_print_performance_info(void) {
   uint32_t min_ms = 0;
   uint32_t max_ms = 0;
   uint32_t avg_ms = 0;
   uint32_t frames_over = 0;
   timing_get_performance_info(&min_ms, &max_ms, &avg_ms, &frames_over);
   d_log("======= FRAME %d ========", timing_get_frame_count());
   d_log("   last_frame_time = %u ms", timing_get_last_frame_time());
   d_log("    min_frame_time = %u ms", min_ms);
   d_log("    max_frame_time = %u ms", max_ms);
   d_log("    avg_frame_time = %u ms", avg_ms);
   d_log("frames_over_budget = %u frames", frames_over);
   d_log("==========================");
}

// INPUT
const char* d_name_input_event(InputEvent e) {
    static const char* names[] = {
        "INPUT_NONE",      "INPUT_UP",
        "INPUT_RIGHT",     "INPUT_DOWN",
        "INPUT_LEFT",      "INPUT_A",
        "INPUT_B",         "INPUT_C",
        "INPUT_D",         "INPUT_LB",
        "INPUT_LT",        "INPUT_RB",
        "INPUT_RT",        "INPUT_START",
        "INPUT_SELECT",    "INPUT_QUIT",
        "INPUT_MAX"
    };
    return (e >= 0 && e < INPUT_MAX + 1) ? names[e] : "UNKNOWN!";
}

const char* d_name_game_context(GameContext context) {
   static const char* names[] = {
      "CONTEXT_TITLE",
      "CONTEXT_MENU",
      "CONTEXT_DEVICE_SELECT",
      "CONTEXT_PLAY",
      "CONTEXT_DBOX",
      "CONTEXT_MAX"
   };
   return (context < CONTEXT_MAX) ? names[context] : "UNKNOWN!";
}

const char* d_name_input_raw_key(int key) {
   return SDL_GetKeyName(key);
}

void d_print_all_mappings(void) {
   const InputSystem* g_input = input_get_debug_state();
   printf("all input mappings:\n");
   const InputMapping* temp = g_input->mappings;
   for (int i = 0; i < g_input->mapping_count; i++) {
      printf("device_id %d: ", temp[i].device_id);
      printf("raw_key = %s", d_name_input_raw_key(temp[i].raw_key));
      printf(" (%d)\n", temp[i].raw_key);
      printf("             event = %s, ", d_name_input_event(temp[i].event));
      printf("\n");
   }
}

void d_print_current_input_state(int dev) {
   const InputSystem* g_input = input_get_debug_state();
   if (!g_input->devices[dev].connected)
      printf("device %d is not connected, so i won't output its current input state\n", dev);
   printf("current input state for device %d:\n", g_input->devices[dev].device_id);
   const InputState* states = g_input->devices[dev].states;
   for (int i = 0; i < INPUT_MAX; i++) {
      if (states[i].pressed) {
         printf("%s\n", d_name_input_event(i));
      }
   }
}

void d_print_devices(void) {
   const InputSystem* g_input = input_get_debug_state();
   printf("\n=== INPUT DEVICES DEBUG ===\n");
   printf("Player 1 device: %d\n", g_input->player1_device);
   printf("Player 2 device: %d\n", g_input->player2_device);
   printf("Frame: %u\n", g_input->frame_count);
   
   for (int i = 0; i < MAX_INPUT_DEVICES; i++) {
      const InputDevice* device = &g_input->devices[i];
      printf("\nDevice[%d]:\n", i);
      printf("  Connected: %s\n", device->connected ? "YES" : "NO");
      
      if (i == 0) {
         printf("  Type: Keyboard\n");
      } else {
         printf("  Type: Controller\n");
         if (device->info.valid_guid) {
            printf("  Name: %s\n", device->info.name);
            printf("  Remembered: YES\n");
         } else {
            printf("  Remembered: NO\n");
         }
      }
      
      printf("  Last seen frame: %u\n", device->last_seen_frame);
   }
   printf("===========================\n\n");
}

// SCENE
const char* d_name_game_mode_type(GameModeType type) {
   static const char* names[] = {
      "GAME_MODE_ARCADE",
      "GAME_MODE_TRAINING",
      "GAME_MODE_STORY",
      "GAME_MODE_VERSUS",
      "GAME_MODE_MAX"
   };
   return (type < GAME_MODE_MAX) ? names[type] : "UNKNOWN!";
}
const char* d_name_scene_type(SceneType type) {
   static const char* names[] = {
      "SCENE_TITLE",
      "SCENE_MAIN_MENU",
      "SCENE_CHARACTER_SELECT",
      "SCENE_GAMEPLAY",
      "SCENE_SETTINGS",
      "SCENE_MAX"
   };
   return (type < SCENE_MAX) ? names[type] : "UNKNOWN!";
}

void d_print_scene_stack_entry(int index) {
   if (index < 0) return;
   
   if (index < scene_get_stack_depth() && LOG_VERBOSITY >= LOG_VERBOSE) {
      SceneStackEntry* scene = scene_get_scene_entry_at_index(index);
      d_log("");
      d_logl("%s", d_name_scene_type(scene->type));
      d_logl(" | P1 = %d", scene->player_devices[0]);
      d_logl(" | P2 = %d", scene->player_devices[1]);
      d_logl(" | pos = %d\n", scene->menu_position);
   }
   /* else if (index < MAX_SCENE_STACK_DEPTH) {
      SceneStackEntry* scene = scene_get_scene_entry_at_index(index);
      d_log("");
      d_logl("%s", d_name_scene_type(scene->type));
      d_logl(" | P1 = %d", scene->player_devices[0]);
      d_logl(" | P2 = %d", scene->player_devices[1]);
      d_logl(" | pos = %d ***\n", scene->menu_position
   }*/
}

void d_print_scene_stack(void) {
   // e.g. versus, if using keyboard from menu but choosing controller1 for left and keyboard for right
   //       (note device select is not in scene stack)
   // SCENE_TITLE | P1 = -1 | P2 = -1 | pos = 0
   // SCENE_MAIN_MENU | P1 = 0 | P2 = -1 | pos = 3
   // SCENE_CHARACTER_SELECT | P1 = 1 | P2 = 0 | pos = 0
   for (int i = 0; i < MAX_SCENE_STACK_DEPTH; i++) {
      d_print_scene_stack_entry(i);
   }
}

// MENU
const char* d_name_menu_type(MenuType type) {
   static const char* names[] = {
      "MENU_TYPE_MAIN",
      "MENU_TYPE_CHARSEL",
      "MENU_TYPE_SETTINGS",
      "MENU_TYPE_PAUSE",
      "MENU_TYPE_MAX"
   };
   return (type < MENU_TYPE_MAX) ? names[type] : "UNKNOWN!";
}
const char* d_name_option_type(OptionType type) {
   static const char* names[] = {
      "OPTION_TYPE_ACTION",
      "OPTION_TYPE_TOGGLE",
      "OPTION_TYPE_CHOICE",
      "OPTION_TYPE_SLIDER",
      "OPTION_TYPE_SUBMENU",
      "OPTION_TYPE_CHARSEL",
      "OPTION_TYPE_MAX"
   };
   return (type < OPTION_TYPE_MAX) ? names[type] : "UNKNOWN!";
}

const char* d_name_menu_action(MenuAction action) {
   static const char* names[] = {
      "MENU_ACTION_NONE",
      "MENU_ACTION_GAME_SETUP",
      "MENU_ACTION_SCENE_CHANGE",
      "MENU_ACTION_BACK",
      "MENU_ACTION_QUIT",
      "MENU_ACTION_MAX"
   };
   return (action < MENU_ACTION_MAX) ? names[action] : "UNKNOWN!";
}