#ifndef SCENE_H
#define SCENE_H

#include "input.h" // for InputEvent, InputState;

#define MAX_SCENE_STACK_DEPTH 8

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
   void (*handle_input)(InputEvent event, InputState state, int device_id);
} Scene;

typedef struct {
   GameModeType mode;                     // ARCADE, TRAINING, VERSUS, etc.
   bool valid;                            // false when a session should be reset
   int selected_characters[MAX_PLAYERS];  // -1 if not selected
   bool cpu_players[MAX_PLAYERS];         // true if CPU
   bool confirmed_devices;                // false while on device select screen
   int selected_stage;                    // int for now, -1 if not selected
} GameSession;

// stores info about previous scenes
typedef struct {
   SceneType type;
   int menu_position;                     // just UI state to restore
   int player_devices[MAX_PLAYERS];       // -1 if CPU/not assigned
} SceneStackEntry;

// scene management with stack
typedef struct {
   SceneStackEntry stack[MAX_SCENE_STACK_DEPTH];
   int stack_depth;
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
void scene_push(SceneType new_scene);
void scene_pop(void);
void scene_change_to(SceneType type);

void scene_start_game_session(GameModeType mode);
void scene_reset_session(void);

// utility functions
GameSession* scene_get_session(void);
SceneType scene_get_current_scene(void);
SceneStackEntry* scene_get_scene_entry_at_index(int index);
int scene_get_stack_depth(void);

// note: init() sets up scene but doesn't draw
//       handle_input() is called before update()
//       render() always draws scene state
//       update() modifies what render() will draw

// TITLE
void title_scene_init(void);
void title_scene_handle_input(InputEvent event, InputState state, int device_id);
void title_scene_update(float delta_time);
void title_scene_render(void);

// MAIN MENU
void main_menu_scene_init(void);
void main_menu_scene_handle_input(InputEvent event, InputState state, int device_id);
void main_menu_scene_update(float delta_time);
void main_menu_scene_render(void);

void main_menu_handle_scene_change(SceneType new_scene);
void main_menu_handle_game_setup(GameModeType mode);

// CHARACTER SELECT
void character_select_scene_init(void);
void character_select_scene_handle_input(InputEvent event, InputState state, int device_id);
void character_select_scene_update(float delta_time);
void character_select_scene_render(void);

void character_select_handle_scene_change(SceneType new_scene);

// SETTINGS
void settings_scene_init(void);
void settings_scene_handle_input(InputEvent event, InputState state, int device_id);
void settings_scene_update(float delta_time);
void settings_scene_render(void);

void settings_handle_scene_change(SceneType new_scene);

#endif