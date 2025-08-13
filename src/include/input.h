#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>


#define MAX_PLAYERS 2
#define MAX_INPUT_DEVICES 4
#define MAX_KEYS_PER_DEVICE 32
#define MAX_DEVICE_NAME_LENGTH 64
#define SDL_CONTROLLER_BUTTON_LEFTTRIGGER (SDL_CONTROLLER_BUTTON_MAX)
#define SDL_CONTROLLER_BUTTON_RIGHTTRIGGER (SDL_CONTROLLER_BUTTON_MAX + 1)
#define SDL_CONTROLLER_BUTTON_LEFTSTICK_RIGHT (SDL_CONTROLLER_BUTTON_MAX + 2)
#define SDL_CONTROLLER_BUTTON_LEFTSTICK_LEFT (SDL_CONTROLLER_BUTTON_MAX + 3)
#define SDL_CONTROLLER_BUTTON_LEFTSTICK_DOWN (SDL_CONTROLLER_BUTTON_MAX + 4)
#define SDL_CONTROLLER_BUTTON_LEFTSTICK_UP (SDL_CONTROLLER_BUTTON_MAX + 5)

// input events
typedef enum {
   INPUT_NONE = 0,
   // directional
   INPUT_UP,
   INPUT_RIGHT,
   INPUT_DOWN,
   INPUT_LEFT,
   // face buttons
   INPUT_A,
   INPUT_B,
   INPUT_C,
   INPUT_D,
   // triggers
   INPUT_LB,
   INPUT_LT,
   INPUT_RB,
   INPUT_RT,
   // system
   INPUT_START,
   INPUT_SELECT,
   INPUT_QUIT, // only accessed via escape key
   INPUT_MAX
} InputEvent;

// TODO: device select context, set keybind context
typedef enum {
   CONTEXT_TITLE,
   CONTEXT_MENU,
   CONTEXT_DEVICE_SELECT,
   CONTEXT_PLAY,
   CONTEXT_DBOX,
   CONTEXT_MAX
} GameContext;

// input state for a single input
typedef struct {
   bool pressed;     // just pressed this frame
   bool held;        // currently being held
   bool released;    // just released this frame
   float duration;   // how long held in seconds
   bool prev_state;  // previous frame state for edge detection
} InputState;

// device info for remembering controllers
typedef struct {
   char name[MAX_DEVICE_NAME_LENGTH];
   SDL_JoystickGUID guid;
   int sdl_joystick_id; // SDL's internal ID
   bool valid_guid;     // whether we have a valid GUID for this device
} DeviceInfo;

// the input device
typedef struct {
   int device_id;                   // internal device ID (0-3)
   bool connected;                  // connected or nah
   InputState states[INPUT_MAX];    // state of input for each type of InputEvent
   DeviceInfo info;                 // for remembering disconnected controllers
   SDL_GameController* controller;  // SDL controller handle
   uint32_t last_seen_frame;        // for cleanup of stale devices

   // cache raw button/axis states
   bool raw_buttons[SDL_CONTROLLER_BUTTON_MAX + 6]; // extension for triggers and left anal stick
   int16_t raw_axes[SDL_CONTROLLER_AXIS_MAX];
   bool cache_valid;
} InputDevice;

// input mapping
typedef struct {
   int raw_key;
   InputEvent event;
   int device_id;
} InputMapping;

// handle input events in different contexts
typedef void (*InputHandler)(InputEvent event, InputState state, int device_id);

// main input system
typedef struct {
   InputDevice devices[MAX_INPUT_DEVICES];
   InputMapping mappings[256];
   int mapping_count;

   GameContext current_context;
   InputHandler context_handlers[CONTEXT_MAX];

   // player device assignments (-1 if unassigned)
   int player1_device;
   int player2_device;
   
   // timing
   // TODO: update to use timing.h
   float delta_time;
   uint32_t frame_count;
   
   // input buffering for reliability
   bool input_buffer_enabled;
   float input_buffer_time;  // time to buffer inputs (in ms)
} InputSystem;

// core functions
void input_init(void);
void input_update(float delta_time);
void input_shutdown(void);

// device management
void input_scan_devices(void);
void input_remember_device(int device_id, const char* name, SDL_JoystickGUID guid, int sdl_id);
void input_cleanup_disconnected_devices(void);

// mapping functions
void input_setup_default_mappings(void);
void input_add_mapping(int raw_key, InputEvent event, int device_id);

// context system
void input_setup_handlers(void);
void input_set_context(GameContext context);
void input_set_context_handler(GameContext context, InputHandler handler);

// input queries
void input_cache_device_state(int device_id);
bool input_is_raw_pressed(InputEvent event, int device_id);
bool input_pressed(InputEvent event, int device_id);
bool input_held(InputEvent event, int device_id);
bool input_released(InputEvent event, int device_id);
float input_duration(InputEvent event, int device_id);

// buffered input queries
bool input_pressed_buffered(InputEvent event, int device_id, float buffer_time);
bool input_combo_pressed(InputEvent primary, InputEvent secondary, int device_id);

// utility functions
const InputSystem* input_get_debug_state(void); // read-only pointer
int input_find_device_by_guid(SDL_JoystickGUID guid);
bool input_is_device_connected(int device_id);
void input_set_player_device(int player, int device_id);
int input_get_player_device(int player);
int input_get_player(int device_id); // returns 0 if unassigned
#endif