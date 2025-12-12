#include "timing.h"
#include "renderer.h"
#include "input.h"
#include "scene.h"
#include "debug.h"
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

bool game_init(float scale_factor, int framerate) {
   // init SDL
   if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
      d_err("SDL could not initialize! SDL_Error: %s", SDL_GetError());
      return false;
   }

   timing_init(framerate);   
   if (!renderer_init(scale_factor)) return false;
   input_init();
   scene_init();

   g_game.state = GAME_RUNNING;
   d_log("Initialized :)\n");
   
   return true;
}

void game_handle_events(float delta_time) {
   SDL_Event e;

   while (SDL_PollEvent(&e)) {
      switch (e.type) {
      case SDL_QUIT:
         game_shutdown();
         return;
      case SDL_WINDOWEVENT:
         renderer_handle_window_event(&e);
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
   renderer_present();
}

void game_escape(uint32_t timer) {            
   if (timer >= 255) game_shutdown();
   else renderer_draw_system_quit((uint8_t)timer);
}

void game_shutdown(void) {
   d_logl("\n");
   d_log("shutting down the game......");
   g_game.state = GAME_QUIT;
   scene_destroy();
   input_shutdown();
   renderer_cleanup();
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