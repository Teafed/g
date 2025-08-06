#include "debug.h"
#include "renderer.h"
#include "file.h"
#include "input.h"
#include "timing.h"

int LOG_VERBOSITY = LOG_NORMAL;

// YEAH
void d_log(const char* fmt, ...) {
   if (LOG_VERBOSITY == LOG_QUIET) return;
   va_list args;
   va_start(args, fmt);
   vprintf(fmt, args);
   printf("\n");
   va_end(args);
}

void d_logv(int level, const char* fmt, ...) {
   if (LOG_VERBOSITY < level) return;
   va_list args;
   va_start(args, fmt);
   vprintf(fmt, args);
   printf("\n");
   va_end(args);
}

void d__err(const char* func, const char* file, int line, const char* fmt, ...) {
   va_list args;
   va_start(args, fmt);
   fprintf(stderr, "\033[1;31m[ERROR]\033[0m ");
   vfprintf(stderr, fmt, args);
   fprintf(stderr, "\n    at %s (%s:%d)\n", func, file, line);
   va_end(args);
}

bool d__dne(void* ptr, const char* name, const char* func, const char* file, int line) {
    if (ptr == NULL) {
        fprintf(stderr, "\033[1;31m[NULL]\033[0m %s is NULL!\n", name);
        fprintf(stderr, "    at %s (%s:%d)\n", func, file, line);
        return true;
    }
    return false;
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

const char* d_name_rect(SDL_Rect* rect) {
   // NOTE can't call d_name_rect twice in one printf function, otherwise both arguments will output the same. alternate:
   // void d_name_rect(SDL_Rect* rect, char* out, size_t size) {
   //    snprintf(out, size, "[ %d, %d, %d, %d ]", rect->x, rect->y, rect->w, rect->h);
   // }
   // just declare two char buffers before calling
   static char buffer[64];
   snprintf(buffer, sizeof(buffer), "[ %d, %d, %d, %d ]", rect->x, rect->y, rect->w, rect->h);
   return buffer;
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
      "TITLE", "MENU", "DEVICE_SELECT", "PLAY", "DBOX"
   };
   return (context < CONTEXT_MAX) ? names[context] : "UNKNOWN!";
}

const char* d_name_input_raw_key(int key) {
   return SDL_GetKeyName(key);
}

void d_print_all_mappings(void) {
   printf("all input mappings:\n");
   InputMapping* temp = g_input.mappings;
   for (int i = 0; i < g_input.mapping_count; i++) {
      printf("device_id %d: ", temp[i].device_id);
      printf("raw_key = %s", d_name_input_raw_key(temp[i].raw_key));
      printf(" (%d)\n", temp[i].raw_key);
      printf("             event = %s, ", d_name_input_event(temp[i].event));
      printf("\n");
   }
}

void d_print_current_input_state(int dev) {
   if (!g_input.devices[dev].connected)
      printf("device %d is not connected, so i won't output its current input state\n", dev);
   printf("current input state for device %d:\n", g_input.devices[dev].device_id);
   InputState* states = g_input.devices[dev].states;
   for (int i = 0; i < INPUT_MAX; i++) {
      if (states[i].pressed) {
         printf("%s\n", d_name_input_event(i));
      }
   }
}

void d_print_devices(void) {
   printf("\n=== INPUT DEVICES DEBUG ===\n");
   printf("Player 1 device: %d\n", g_input.player1_device);
   printf("Player 2 device: %d\n", g_input.player2_device);
   printf("Frame: %u\n", g_input.frame_count);
   
   for (int i = 0; i < MAX_INPUT_DEVICES; i++) {
      InputDevice* device = &g_input.devices[i];
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
const char* d_name_scene_type(SceneType type) {
   static const char* names[] = {
      "SCENE_TITLE", "SCENE_MAIN_MENU", "SCENE_CHARACTER_SELECT", "SCENE_GAMEPLAY", "SCENE_SETTINGS"
   };
   return (type < SCENE_MAX) ? names[type] : "UNKNOWN!";
}

void d_print_scene_stack_entry(int index) {
   if (index < 0) return;
   
   if (index < scene_get_stack_depth()) {
        SceneStackEntry* scene = scene_get_scene_entry_at_index(index);
        printf("%s", d_name_scene_type(scene->type));
        printf(" | P1 = %d", scene->player_devices[0]);
        printf(" | P2 = %d", scene->player_devices[1]);
        printf(" | pos = %d\n", scene->menu_position);
   }
   /* else if (index < MAX_SCENE_STACK_DEPTH) {
        SceneStackEntry* scene = scene_get_scene_entry_at_index(index);
        printf("%s", d_name_scene_type(scene->type));
        printf(" | P1 = %d", scene->player_devices[0]);
        printf(" | P2 = %d", scene->player_devices[1]);
        printf(" | pos = %d ***\n", scene->menu_position
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
const char* d_name_menu_action(MenuAction action) {
   static const char* names[] = {
      "MENU_ACTION_NONE",
      "MENU_ACTION_SUBMENU",
      "MENU_ACTION_GAME_SETUP",
      "MENU_ACTION_SCENE_CHANGE",
      "MENU_ACTION_BACK",
      "MENU_ACTION_QUIT",
      "MENU_ACTION_MAX"
   };
   return (action < MENU_ACTION_MAX) ? names[action] : "UNKNOWN!";
}
