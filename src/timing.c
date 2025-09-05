#include "timing.h"
#include "debug.h"
#include <SDL2/SDL.h>

static TimingState g_timing = { 0 };

void timing_init(ui32 target_fps) {
   if (target_fps == 0) { d_log("cmon man. fps set to 60"); target_fps = 60; }
   g_timing.target_fps = target_fps;
   g_timing.target_frame_time = 1000 / target_fps;  // ms per frame
   g_timing.frame_start_time = SDL_GetTicks();
   g_timing.last_frame_time = g_timing.target_frame_time;
   g_timing.frame_count = 0;
   
   g_timing.min_frame_time = UINT32_MAX;
   g_timing.max_frame_time = 0;
   g_timing.avg_frame_time = g_timing.target_frame_time;
   g_timing.frames_over_budget = 0;
   
   g_timing.game_start_time = SDL_GetTicks();
   g_timing.total_game_time = 0;
}

void timing_frame_start(void) {
   g_timing.frame_start_time = SDL_GetTicks();
}

void timing_frame_end(void) {
   ui32 current_time = SDL_GetTicks();
   ui32 frame_duration = current_time - g_timing.frame_start_time;
   
   g_timing.last_frame_time = frame_duration;
   g_timing.frame_count++;
   g_timing.total_game_time = current_time - g_timing.game_start_time;
   
   if (frame_duration < g_timing.min_frame_time) {
      g_timing.min_frame_time = frame_duration;
   }
   if (frame_duration > g_timing.max_frame_time) {
      g_timing.max_frame_time = frame_duration;
   }
   if (frame_duration > g_timing.target_frame_time) {
      g_timing.frames_over_budget++;
   }
   
   // simple rolling average - maybe improve at some point
   g_timing.avg_frame_time = (g_timing.avg_frame_time * 9 + frame_duration) / 10;
}

bool timing_should_limit_frame(void) {
   return (g_timing.last_frame_time < g_timing.target_frame_time);
}

ui32 timing_get_last_frame_time(void) {
   return g_timing.last_frame_time;
}

ui32 timing_get_frame_duration(void) {
   if (g_timing.last_frame_time >= g_timing.target_frame_time) {
      return 0;  // already over budget...
   }
   return g_timing.target_frame_time - g_timing.last_frame_time;
}

float timing_get_delta_time(void) {
   static ui32 previous_frame_start = 0;
   
   if (previous_frame_start == 0) {
      previous_frame_start = g_timing.frame_start_time;
      return 1.0f / g_timing.target_fps;  // return target delta for first frame
   }
   
   float delta = (g_timing.frame_start_time - previous_frame_start) / 1000.0f;
   previous_frame_start = g_timing.frame_start_time;
   
   // only clamp if delta is way off from target (like 5+ frames worth)
   float expected_delta = g_timing.target_frame_time / 1000.0f;
   if (delta > expected_delta * 5.0f) {
      delta = expected_delta;  // reset to target on major hiccups
   }
   return delta;
}

ui32 timing_get_frame_count(void) {
   return g_timing.frame_count;
}

ui32 timing_get_game_time_ms(void) {
   return g_timing.total_game_time;
}

float timing_get_current_fps(void) {
   if (g_timing.last_frame_time == 0) return 0.0f;
   return 1000.0f / g_timing.last_frame_time;
}

Timer* timer_start(void) {
   Timer* timer = malloc(sizeof(Timer));
   timer->start_time = SDL_GetTicks();
   return timer;
}

ui32 timer_end(Timer* timer) {
   ui32 time = SDL_GetTicks() - timer->start_time;
   free(timer);
   return time;
}

void timing_get_performance_info(ui32* min_ms, ui32* max_ms, 
                                 ui32* avg_ms, ui32* frames_over) {
   if (min_ms) *min_ms = g_timing.min_frame_time;
   if (max_ms) *max_ms = g_timing.max_frame_time;
   if (avg_ms) *avg_ms = g_timing.avg_frame_time;
   if (frames_over) *frames_over = g_timing.frames_over_budget;
}

const TimingState* timing_get_debug_state(void) {
   return &g_timing;
}