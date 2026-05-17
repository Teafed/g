#include "timing.h"
#include "input.h"
#include "scene.h"
#include "debug.h"
#include "game_assets.h"

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
static LiRen*      g_liren  = NULL;
static GameAssets* g_assets = NULL;

GameAssets* game_get_assets(void) { return g_assets; }

// game palette (teaf-v5). engine owns it; liren just borrows it via LR_Config.
// indices are referenced throughout the game; see liren's LR_DEFAULT_PALETTE
// for the canonical naming.
static const uint32_t GAME_PALETTE[] = {
   0xFEFEFDFF,  //  0 - mono-white
   0xAAAAAAFF,  //  1 - mono-lgrey
   0x777777FF,  //  2 - mono-grey
   0x494949FF,  //  3 - mono-dgrey
   0x1E1E1EFF,  //  4 - mono-dark
   0x161616FF,  //  5 - mono-black
   0xE8BE96FF,  //  6 - orange-cream
   0xEAAA6DFF,  //  7 - orange-teaf
   0xF08629FF,  //  8 - orange-normal
   0x9B5A3CFF,  //  9 - brown-light
   0x5B3523FF,  // 10 - brown-dark
   0xE8896DFF,  // 11 - red-piggy
   0xAD2E35FF,  // 12 - red-ivwy
   0x7E2023FF,  // 13 - red-normal
   0x4C1313FF,  // 14 - red-dark
   0x280A0AFF,  // 15 - red-black
   0xB3DBD8FF,  // 16 - teal-cream
   0x57DCD7FF,  // 17 - teal-frankie
   0x27917FFF,  // 18 - teal-ocean
   0x022D25FF,  // 19 - teal-dark
   0xA2CAD8FF,  // 20 - blue-light
   0x6494B0FF,  // 21 - blue-sky
   0x5D5D6BFF,  // 22 - blue-concrete
   0xF5ABB9FF,  // 23 - pink-cream
   0xD69699FF,  // 24 - pink-craige
   0xF46BA9FF,  // 25 - pink-deep
   0x68466BFF,  // 26 - magenta-normal
   0x453247FF,  // 27 - magenta-dark
   0xCDACE0FF,  // 28 - purple-cream
   0x9B5AC3FF,  // 29 - purple-katja
   0x7041A3FF,  // 30 - purple-normal
   0x442882FF,  // 31 - purple-dark
   0xA6B18EFF,  // 32 - green-cloudy
   0x4E631EFF,  // 33 - green-jacket
   0x394816FF,  // 34 - green-dirty
   0x242D0EFF,  // 35 - green-dark
   0xF0FD93FF,  // 36 - yellow-light
   0xD9E074FF,  // 37 - yellow-moon
   0xC7E336FF,  // 38 - yellow-neon
   0xDCB22FFF,  // 39 - yellow-warm
   0x000000FF,  // 40 - true-black
   0xFFFFFFFF,  // 41 - true-white  (transparent sentinel)
};
#define GAME_PALETTE_SIZE        ((int)(sizeof(GAME_PALETTE)/sizeof(GAME_PALETTE[0])))
#define GAME_PALETTE_TRANSPARENT (GAME_PALETTE_SIZE - 1)

#define COL_MONO_DARK 4

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
   LR_Config cfg = {
      .scale_factor            = scale_factor,
      .resolution              = LR_RES_FWVGA,
      .resize_mode             = LR_RESIZE_FIT,
      .window_mode             = LR_WINDOW_WINDOWED,
      .palette                 = GAME_PALETTE,
      .palette_size            = GAME_PALETTE_SIZE,
      .transparent_color_index = GAME_PALETTE_TRANSPARENT,
      .log_fn                  = liren_log_adapter,
      .log_user_data           = NULL,
   };
   g_liren = lr_create(&cfg);
   if (!g_liren) {
      d_err("failed to create LiRen renderer");
      return false;
   }
   lr_set_clear_color(g_liren, COL_MONO_DARK);

   g_assets = game_assets_load(g_liren, "assets/sheets/");
   if (!g_assets) {
      d_err("failed to load game assets");
      lr_destroy(g_liren);
      g_liren = NULL;
      return false;
   }
   
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
   if (g_assets) {
      game_assets_destroy(g_assets);
      g_assets = NULL;
   }
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