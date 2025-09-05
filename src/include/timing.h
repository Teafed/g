#ifndef TIMING_H
#define TIMING_H

#include "def.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
   ui32 target_fps;
   ui32 target_frame_time; // ms per frame
   ui32 frame_start_time;
   ui32 last_frame_time;
   ui32 frame_count;
   
   // performance tracking
   ui32 min_frame_time;
   ui32 max_frame_time;
   ui32 avg_frame_time;
   ui32 frames_over_budget; // incremented if frame took longer than target_frame_time
   
   ui32 game_start_time;
   ui32 total_game_time; // updated on frame end
} TimingState;

typedef struct {
   ui32 start_time;
} Timer;

void timing_init(ui32 target_fps);
void timing_frame_start(void);
void timing_frame_end(void);
bool timing_should_limit_frame(void); // true if last frame was quicker than target frame time

ui32 timing_get_last_frame_time(void);
ui32 timing_get_frame_duration(void);
float timing_get_delta_time(void); // last frame time in seconds
ui32 timing_get_frame_count(void);
ui32 timing_get_game_time_ms(void);
float timing_get_current_fps(void);


Timer* timer_start(void);
ui32 timer_end(Timer* timer);

void timing_get_performance_info(ui32* min_ms, ui32* max_ms, ui32* avg_ms, ui32* frames_over);
const TimingState* timing_get_debug_state(void); // read-only pointer

#endif