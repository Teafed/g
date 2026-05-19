#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>


#define MAX_PLAYERS 2
#define MAX_INPUT_DEVICES 4
#define MAX_KEYS_PER_DEVICE 32
#define MAX_DEVICE_NAME_LENGTH 64
#define INPUT_EVENT_BUFFER_SIZE 64
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

// input state for a single input. produced by the polling half of the api
typedef struct {
   bool pressed;     // just pressed this frame
   bool held;        // currently being held
   bool released;    // just released this frame
   float duration;   // how long held in seconds
   bool prev_state;  // previous frame state for edge detection
} InputState;

// edge event in the per-device ring buffer. recorded by input_update, consumed by drain
typedef struct {
   InputEvent event;
   bool       is_press;   // true = press edge, false = release edge
   float      timestamp;  // seconds since input_init; monotonic
} InputEventEntry;

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

   // edge event ring buffer
   InputEventEntry events[INPUT_EVENT_BUFFER_SIZE];
   int             event_count;

   // hold-to-repeat state, per (device, event)
   float repeat_accum[INPUT_MAX];
   bool  repeat_fired_initial[INPUT_MAX]; // has the post-initial-delay first repeat fired
} InputDevice;

// input mapping
typedef struct {
   int raw_key;
   InputEvent event;
   int device_id;
} InputMapping;

// main input system
typedef struct {
   InputDevice devices[MAX_INPUT_DEVICES];
   InputMapping mappings[256];
   int mapping_count;

   // monotonic clock fed from input_update's delta_time. used for event timestamps
   float time_seconds;

   // player device assignments (-1 if unassigned)
   int player1_device;
   int player2_device;
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

// polled queries (current frame state)
void input_cache_device_state(int device_id);
bool input_is_raw_pressed(InputEvent event, int device_id);
bool input_pressed(InputEvent event, int device_id);
bool input_held(InputEvent event, int device_id);
bool input_released(InputEvent event, int device_id);
float input_duration(InputEvent event, int device_id);
InputState input_get_state(int device_id, InputEvent event);

// hold-to-repeat helper
bool input_pressed_repeat(InputEvent event, int device_id,
                          float initial_delay, float repeat_interval);

// event buffer access
int  input_drain_events(int device_id, InputEventEntry* out, int max);
void input_peek_events(int device_id, const InputEventEntry** out_entries, int* out_count);

// utility functions
const InputSystem* input_get_debug_state(void); // read-only pointer
int input_find_device_by_guid(SDL_JoystickGUID guid);
bool input_is_device_connected(int device_id);
void input_set_player_device(int player, int device_id);
void input_reset_player_devices(void); // unassign both
int input_get_player_device(int player);
void input_get_player_devices(int* p1, int* p2);
int input_get_player(int device_id); // returns 0 if unassigned
#endif
