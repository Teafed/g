#include "game_session.h"
#include "debug.h"

#include <stdlib.h>
#include <string.h>

GameSession* game_session_create(void) {
   GameSession* s = calloc(1, sizeof(GameSession));
   if (!s) {
      d_err("failed to allocate GameSession");
      return NULL;
   }
   game_session_reset(s);
   return s;
}

void game_session_destroy(GameSession* s) {
   if (!s) return;
   free(s);
}

void game_session_reset(GameSession* s) {
   if (!s) return;
   s->mode              = GAME_MODE_MAX;
   s->valid             = false;
   s->confirmed_devices = false;
   s->selected_stage    = -1;
   for (int p = 0; p < MAX_PLAYERS; p++) {
      s->selected_characters[p] = -1;
      s->cpu_players[p]         = false;
   }
}

void game_session_start(GameSession* s, GameModeType mode) {
   if (!s) return;
   game_session_reset(s);
   s->mode  = mode;
   s->valid = true;
   d_logv(2, "game session started: mode=%d", (int)mode);
}
