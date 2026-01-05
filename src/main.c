#include "timing.h"
#include "input.h"
#include "scene.h"
#include "debug.h"

#include <liren/liren.h>
#include <SDL2/SDL.h>

extern int LOG_VERBOSITY;

typedef enum {
   GAME_RUNNING,
   GAME_QUIT
} GameState;

typedef struct {
   GameState state;
} Game;

Game g_game = { 0 };
static LiRen* g_liren = NULL;

bool game_init(float scale_factor, int framerate);
void game_update(float delta_time);
void game_render(void);
void game_handle_events(float delta_time);
void game_escape(uint32_t timer);
void game_shutdown(void);
bool game_handle_flags(int argc, char *argv[], int* logging_mode, float* scale_factor, int* framerate);

int main(int argc, char* argv[]) {
   // initialize w flags
   int logging_mode = LOG_VERBOSITY;
   float scale_factor = 1.0f;
   int framerate = 60;
   if (!game_handle_flags(argc, argv, &logging_mode, &scale_factor, &framerate))
      return 1;
   
   if (!game_init(scale_factor, framerate)) {
      d_err("failed to initialize game");
      return 1;
   }
   
   while (g_game.state == GAME_RUNNING) {
      timing_frame_start();
      float delta_time = timing_get_delta_time();

      game_handle_events(delta_time);  // input & devices
      game_update(delta_time);         // calculations for next frame
      game_render();                   // render next frame
      
      timing_frame_end();
      
      if (timing_should_limit_frame()) { SDL_Delay(timing_get_frame_duration()); }
      // if (timing_get_frame_count() == 3) { game_shutdown(); }
   }

   return 0;
}

static void liren_log_adapter(void* user, const char* msg) {
   (void)user;
   // route to log system
   d_log("%s", msg);
}


bool game_init(float scale_factor, int framerate) {
   // init SDL
   if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
      d_err("SDL could not initialize! SDL_Error: %s", SDL_GetError());
      return false;
   }

   timing_init(framerate);

   // liren config
   LR_Config cfg;
   cfg.scale_factor  = scale_factor;
   cfg.resolution    = LR_RES_FWVGA;          // or LR_RES_VGA if you prefer
   cfg.resize_mode   = LR_RESIZE_FIT;         // matches your old behaviour
   cfg.window_mode   = LR_WINDOW_WINDOWED;
   cfg.log_fn        = liren_log_adapter;
   cfg.log_user_data = NULL;
   g_liren = lr_create(&cfg);
   if (!g_liren) {
      d_err("failed to create LiRen renderer");
      return false;
   }
   lr_set_clear_color(g_liren, 4);
   
   input_init();
   scene_init(g_liren);

   g_game.state = GAME_RUNNING;
   d_log("Initialized :)\n");
   
   return true;
}

void game_handle_events(float delta_time) {
   SDL_Event e;
   
   // TODO: use liren_handle_event
   while (SDL_PollEvent(&e)) {
      switch (e.type) {
      case SDL_QUIT:
         game_shutdown();
         return;
      case SDL_WINDOWEVENT:
         if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            if (g_liren) {
               lr_on_window_resized(g_liren, e.window.data1, e.window.data2);
            }
         }
         break;
      case SDL_APP_LOWMEMORY:
         d_log("THE GAME IS LOW ON MEMORY!!!!!!!!");
         break;
      case SDL_FINGERDOWN:
         d_log("hehe :)");
         game_shutdown();
         break;
      }
   }

   // handle all the input logic
   input_update(delta_time);
}

void game_update(float delta_time) {
   scene_update(delta_time);
}

void game_render(void) {
   if (!g_liren) return;

   lr_begin_frame(g_liren);
   scene_render(g_liren);
   lr_end_frame(g_liren);
}

void game_escape(uint32_t timer) {            
   if (timer >= 255) game_shutdown();
   // TODO: reimplement quit overlay in liren
   // else renderer_draw_system_quit((uint8_t)timer);
}

void game_shutdown(void) {
   d_logl("\n");
   d_log("shutting down the game......");
   g_game.state = GAME_QUIT;
   
   scene_destroy();
   input_shutdown();
   if (g_liren) {
      lr_destroy(g_liren);
      g_liren = NULL;
   }
   SDL_Quit();
}

bool game_handle_flags(int argc, char *argv[], int* logging_mode, float* scale_factor, int* framerate) {
   for (int i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
         char flag = argv[i][1];
         
         if (i + 1 >= argc) {
            fprintf(stderr, "Missing value for -%c\n", flag);
            return false;
         }
         switch (flag) {
         case 'l':
            *logging_mode = atoi(argv[++i]);
            LOG_VERBOSITY = *logging_mode;
            break;
         case 's':
            *scale_factor = atof(argv[++i]);
            break;
         case 'f':
            *framerate = atoi(argv[++i]);
            break;
         default:
            fprintf(stderr, "Unknown flag: -%c\n", flag);
            return false;
         }
      }
   }
   return true;
}