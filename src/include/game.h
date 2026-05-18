#ifndef GAME_H
#define GAME_H

// central home for accessors that hand out game-wide singletons owned by
// main.c. each `game_get_*` returns a pointer to state that lives for the
// entire program lifetime, set up during game_init() and torn down during
// game_shutdown().
//
// scenes and components use these accessors to reach engine services
// without main.c having to thread everything through scene callbacks.

typedef struct SceneStack  SceneStack;
typedef struct GameSession GameSession;
typedef struct GameAssets  GameAssets;

SceneStack*  game_get_stack  (void);
GameSession* game_get_session(void);
GameAssets*  game_get_assets (void);

#endif