#include "renderer.h"
#include "timing.h"
#include "input.h"
#include "scene.h"
#include "menu.h"
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

Game g_game = { 0 }; // should this be static?

bool game_init(void);
void game_update(float delta_time);
void game_render(void);
void game_handle_events(float delta_time);
void game_shutdown(void);

int main(int argc, char* argv[]) {
   if (argc > 1)
      LOG_VERBOSITY = atoi(argv[1]);
   
   if (d_dne(game_init())) {
      printf("failed to initialize game\n");
      return 1;
   }

   while (g_game.state == GAME_RUNNING) {
      timing_frame_start();
      
      float delta_time = timing_get_delta_time();  
      game_handle_events(delta_time);  // input & devices
      game_update(delta_time);         // calculations for next frame
      game_render();                   // render next frame

      timing_frame_end();
      if (timing_should_limit_frame()) {
         SDL_Delay(timing_get_frame_duration());
      }
      if (timing_get_frame_count() == 6) {
         // game_shutdown();
      }
   }

   return 0;
}

bool game_init(void) {
   // init renderer
   if (SDL_Init(SDL_INIT_VIDEO) < 0) {
      SDL_Log("SDL could not initialize! SDL_Error: %s", SDL_GetError());
      return false;
   }

   timing_init(60);   
   if (!renderer_init(1)) return false;
   input_init();
   scene_init();

   g_game.state = GAME_RUNNING;
   
   return true;
}

void game_handle_events(float delta_time) {
   SDL_Event e;

   // TODO: ctrl+c to close?
   while (SDL_PollEvent(&e)) {
      switch (e.type) {
         case SDL_QUIT:
            game_shutdown();
            return;
            break;
         case SDL_WINDOWEVENT:
            renderer_handle_window_event(&e);
            break;
         case SDL_APP_LOWMEMORY:
            printf("THE GAME IS LOW ON MEMORY!!!!!!!!\n");
            break;
         case SDL_FINGERDOWN:
            printf("hehe :)\n");
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

void game_shutdown(void) {
   printf("shutting down the game......\n");
   g_game.state = GAME_QUIT;
   input_shutdown();
   renderer_cleanup();
   SDL_Quit();
}
