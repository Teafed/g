#ifndef SCENE_H
#define SCENE_H

#include "input.h" // for InputEvent, InputState;

typedef enum {
   SCENE_TITLE,
   SCENE_MAIN_MENU,
   SCENE_CHARACTER_SELECT,
   SCENE_GAMEPLAY,
   SCENE_SETTINGS,
   SCENE_MAX
} SceneType;

typedef enum {
   GAME_MODE_ARCADE,
   GAME_MODE_TRAINING,
   GAME_MODE_STORY,
   GAME_MODE_VERSUS,
   GAME_MODE_MAX
} GameModeType;

typedef struct {
   void (*init)(void);
   void (*update)(float delta_time);
   void (*render)(void);
   void (*destroy)(void);
} Scene;

typedef struct {
   GameModeType mode;                     // ARCADE, TRAINING, VERSUS, etc.
   bool valid;                            // false when a session should be reset
   int selected_characters[MAX_PLAYERS];  // -1 if not selected
   bool cpu_players[MAX_PLAYERS];         // true if CPU
   bool confirmed_devices;                // false while on device select screen
   int selected_stage;                    // int for now, -1 if not selected
} GameSession;

typedef struct {
   SceneType current_scene;
   Scene scenes[SCENE_MAX];
   GameSession session;                   // global game state
} SceneManager;

// core functions
void scene_init(void);
void scene_handle_input(InputEvent event, InputState state, int device_id);
void scene_update(float delta_time);
void scene_render(void);
void scene_destroy(void);

// scene management
void scene_change_to(SceneType type);
void scene_start_game_session(GameModeType mode);
void scene_reset_session(void);

// note: init() sets up scene but doesn't draw
//       render() always draws scene state
//       update() modifies what render() will draw

// TITLE
void title_scene_init(void);
void title_scene_update(float delta_time);
void title_scene_render(void);
void title_scene_destroy(void);

void title_scene_handle_input(InputEvent event, InputState state, int device_id);

// MAIN MENU
void main_menu_scene_init(void);
void main_menu_scene_update(float delta_time);
void main_menu_scene_render(void);
void main_menu_scene_destroy(void);

void main_menu_handle_scene_change(SceneType new_scene);
void main_menu_handle_game_setup(GameModeType mode);

// CHARACTER SELECT
void character_select_scene_init(void);
void character_select_scene_update(float delta_time);
void character_select_scene_render(void);
void character_select_scene_destroy(void);

void device_select_handle_input(InputEvent event, InputState state, int device_id);
void character_select_handle_scene_change(SceneType new_scene);

// SETTINGS
void settings_scene_init(void);
void settings_scene_update(float delta_time);
void settings_scene_render(void);
void settings_scene_destroy(void);

void settings_handle_scene_change(SceneType new_scene);

// GAMEPLAY
void gameplay_scene_init(void);
void gameplay_scene_update(float delta_time);
void gameplay_scene_render(void);
void gameplay_scene_destroy(void);

#endif