#ifndef GAME_SESSION_H
#define GAME_SESSION_H

#include "input.h"   // for MAX_PLAYERS
#include <stdbool.h>

typedef enum {
   GAME_MODE_ARCADE,
   GAME_MODE_TRAINING,
   GAME_MODE_STORY,
   GAME_MODE_VERSUS,
   GAME_MODE_MAX
} GameModeType;

typedef struct GameSession {
   GameModeType mode;
   bool valid;                           // false when reset / not yet started
   int selected_characters[MAX_PLAYERS]; // -1 if not selected
   bool cpu_players[MAX_PLAYERS];
   bool confirmed_devices;               // false during device select
   int selected_stage;                   // -1 if not selected
} GameSession;

GameSession* game_session_create (void);
void         game_session_destroy(GameSession* s);
void game_session_reset(GameSession* s);
void game_session_start(GameSession* s, GameModeType mode);

#endif
