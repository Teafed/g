#ifndef TIMING_H
#define TIMING_H

#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
   uint32_t target_fps;
   uint32_t target_frame_time;     // ms per frame
   uint32_t frame_start_time;
   uint32_t last_frame_time;
   uint32_t frame_count;
   
   // performance tracking
   uint32_t min_frame_time;
   uint32_t max_frame_time;
   uint32_t avg_frame_time;
   uint32_t frames_over_budget;
   
   uint32_t game_start_time;
   uint32_t total_game_time;
} TimingState;

void timing_init(uint32_t target_fps);
void timing_frame_start(void);
void timing_frame_end(void);
bool timing_should_limit_frame(void);

uint32_t timing_get_frame_duration(void);
float timing_get_delta_time(void);
uint32_t timing_get_frame_count(void);
uint32_t timing_get_game_time_ms(void);
float timing_get_current_fps(void);
void timing_get_performance_info(uint32_t* min_ms, uint32_t* max_ms, uint32_t* avg_ms, uint32_t* frames_over);

#endif