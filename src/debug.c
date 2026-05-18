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

// TODO: make reverse where u can find enum from filename
//       prolly should make filename array defined in file.h

// TIMING

void d_timing_print_state(void) {
   const TimingState* t = timing_get_debug_state();
   d_log("");
   d_var(t->target_fps);
   d_logl("    "); d_var(t->target_frame_time);
   d_logl("    "); d_var(t->frame_start_time);
   d_logl("    "); d_var(t->last_frame_time);
   d_logl("    "); d_var(t->min_frame_time);
   d_logl("    "); d_var(t->max_frame_time);
   d_logl("    "); d_var(t->avg_frame_time);
   d_logl("    "); d_var(t->frames_over_budget);
   d_logl("    "); d_var(t->game_start_time);
   d_logl("    "); d_var(t->total_game_time);
}

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
    return (e >= 0 && e < INPUT_MAX) ? names[e] : "UNKNOWN!";
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
   d_log("");
   d_logl("P1 device = %d, ", g_input->player1_device);
   d_logl("P2 device = %d\n", g_input->player2_device);
   
   for (int i = 0; i < MAX_INPUT_DEVICES; i++) {
      const InputDevice* device = &g_input->devices[i];
      d_logl("\nDevice[%d]:\n", i);
      d_logl("  Connected: %s\n", device->connected ? "YES" : "NO");
      
      if (i == 0) {
         d_logl("  Type: Keyboard\n");
      } else {
         d_logl("  Type: Controller\n");
         if (device->info.valid_guid) {
            d_logl("  Name: %s\n", device->info.name);
            d_logl("  Remembered: YES\n");
         } else {
            d_logl("  Remembered: NO\n");
         }
      }
      
      d_logl("  Last seen frame: %u\n", device->last_seen_frame);
   }
   d_logl("\n");
}
