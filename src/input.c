#include "input.h"
#include "debug.h"
#include <stdio.h>
#include <string.h>

extern void game_shutdown(void);

InputSystem g_input = { 0 }; // should this be static?

// core functions
void input_init(void) {
   memset(&g_input, 0, sizeof(InputSystem));
   g_input.player1_device = -1;
   g_input.player2_device = -1;
   g_input.current_context = CONTEXT_TITLE;
   g_input.input_buffer_enabled = true;
   g_input.input_buffer_time = 0.1f;  // 100ms input buffer
   g_input.frame_count = 0;

   // initialize all devices as unknown and disconnected
   for (int i = 0; i < MAX_INPUT_DEVICES; i++) {
      g_input.devices[i].device_id = i;
      g_input.devices[i].connected = false;
      g_input.devices[i].controller = NULL;
      g_input.devices[i].last_seen_frame = 0;
      memset(&g_input.devices[i].info, 0, sizeof(DeviceInfo));
   }

   input_setup_default_mappings();
   input_scan_devices();
   input_setup_handlers();
}

void input_update(float delta_time) {
   g_input.delta_time = delta_time;
   g_input.frame_count++;
   
   input_scan_devices(); // TODO: call this only if detect device activity?

   // update all input states for all devices
   for (int dev = 0; dev < MAX_INPUT_DEVICES; dev++) {
      InputDevice* device = &g_input.devices[dev];
      if (!device->connected) continue;
      input_cache_device_state(dev);

      for (int event = 0; event < INPUT_MAX; event++) {
         InputState* state = &device->states[event];

         bool currently_pressed = input_is_raw_pressed(event, dev);

         // update state flags using previous frame state
         state->pressed = currently_pressed && !state->prev_state;
         state->released = !currently_pressed && state->prev_state;
         state->held = currently_pressed;
         
         // store current state for next frame
         state->prev_state = currently_pressed;
         
         // if (currently_pressed) d_log("[%d]: %s", dev, d_name_input_event(event));
         if (event == INPUT_QUIT) {
            if (state->held) {
               // d_var(state->duration);
               int wait = (state->duration * 1000.0f);
               if (wait >= 255) game_shutdown();
               // renderer_draw_system_quit((uint8_t)wait);
            }
            else if (state->released) {
               // renderer_draw_system_quit(0);
            }
         }
         
         // update duration
         if (state->held) {
            state->duration += delta_time;
         } else {
            state->duration = 0.0f;
         }

         // route input to current context handler
         if (state->pressed && g_input.context_handlers[g_input.current_context]) {
            g_input.context_handlers[g_input.current_context](event, *state, dev);
         }
      }
   }
   
   // periodic cleanup
   if (g_input.frame_count % 1800 == 0) {  // every 30 seconds at 60fps
      input_cleanup_disconnected_devices();
   }
}

void input_shutdown(void) {
   // close all controllers
   for (int i = 1; i < MAX_INPUT_DEVICES; i++) {
      if (g_input.devices[i].controller) {
         SDL_GameControllerClose(g_input.devices[i].controller);
         g_input.devices[i].controller = NULL;
      }
   }
}

// device management
void input_scan_devices(void) {
   // device 0 is always keyboard
   g_input.devices[0].connected = true;
   g_input.devices[0].last_seen_frame = g_input.frame_count;
   
   // scan for game controllers
   int num_joysticks = SDL_NumJoysticks();
   
   for (int sdl_idx = 0; sdl_idx < num_joysticks; sdl_idx++) {
      if (!SDL_IsGameController(sdl_idx)) continue;
      
      SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(sdl_idx);
      const char* name = SDL_GameControllerNameForIndex(sdl_idx);
      if (!name) name = "Unknown Controller";
      
      // try to find existing device with this GUID
      int device_id = input_find_device_by_guid(guid);

      if (device_id == -1) {
         // give new controller device_id
         for (int i = 1; i < MAX_INPUT_DEVICES; i++) {
            if (!g_input.devices[i].connected && !g_input.devices[i].info.valid_guid) {
               device_id = i;
               break;
            }
         }
      }
      if (device_id != -1) {
         InputDevice* device = &g_input.devices[device_id];
         
         // open controller if not already open
         if (!device->controller) {
            device->controller = SDL_GameControllerOpen(sdl_idx);
            if (!device->controller) {
               printf("SDL_GameControllerOpen failed: %s (Device %d)\n", name, device_id);
               break;
            }
            printf("Controller opened: %s (Device %d)\n", name, device_id);
         }
         
         device->connected = (device->controller != NULL);
         device->last_seen_frame = g_input.frame_count;
         
         // remember device info
         input_remember_device(device_id, name, guid, sdl_idx);
      }
   }
   
   // check for disconnected controllers
   for (int i = 1; i < MAX_INPUT_DEVICES; i++) {
      InputDevice* device = &g_input.devices[i];
      if (device->controller && !SDL_GameControllerGetAttached(device->controller)) {
         printf("Controller disconnected: %s (Device %d)\n", 
               device->info.name, device->device_id);
         SDL_GameControllerClose(device->controller);
         device->controller = NULL;
         device->connected = false;
         // keep device info for reconnection
      }
   }
}

void input_remember_device(int device_id, const char* name, SDL_JoystickGUID guid, int sdl_id) {
   if (device_id < 0 || device_id >= MAX_INPUT_DEVICES) return;
   
   DeviceInfo* info = &g_input.devices[device_id].info;
   strncpy(info->name, name, MAX_DEVICE_NAME_LENGTH - 1);
   info->name[MAX_DEVICE_NAME_LENGTH - 1] = '\0';
   info->guid = guid;
   info->sdl_joystick_id = sdl_id;
   info->valid_guid = true;
}

void input_cleanup_disconnected_devices(void) {
   // clean up devices that haven't been seen for a while
   for (int i = 1; i < MAX_INPUT_DEVICES; i++) {
      InputDevice* device = &g_input.devices[i];
      if (!device->connected && 
         (g_input.frame_count - device->last_seen_frame) > 1800) {  // 30 seconds at 60fps
         memset(&device->info, 0, sizeof(DeviceInfo));
      }
   }
}

// mapping
void input_setup_default_mappings(void) {
   // keyboard mappings (device 0)
   input_add_mapping(SDLK_w,           INPUT_UP, 0);
   input_add_mapping(SDLK_UP,          INPUT_UP, 0);
   input_add_mapping(SDLK_SPACE,       INPUT_UP, 0);
   input_add_mapping(SDLK_d,           INPUT_RIGHT, 0);
   input_add_mapping(SDLK_RIGHT,       INPUT_RIGHT, 0);
   input_add_mapping(SDLK_s,           INPUT_DOWN, 0);
   input_add_mapping(SDLK_DOWN,        INPUT_DOWN, 0);
   input_add_mapping(SDLK_a,           INPUT_LEFT, 0);
   input_add_mapping(SDLK_LEFT,        INPUT_LEFT, 0);

   input_add_mapping(SDLK_j,           INPUT_A, 0);
   input_add_mapping(SDLK_k,           INPUT_B, 0);
   input_add_mapping(SDLK_l,           INPUT_C, 0);
   input_add_mapping(SDLK_SEMICOLON,   INPUT_D, 0);

   input_add_mapping(SDLK_o,           INPUT_LB, 0);
   input_add_mapping(SDLK_p,           INPUT_LT, 0);
   input_add_mapping(SDLK_i,           INPUT_RB, 0);
   input_add_mapping(SDLK_u,           INPUT_RT, 0);

   input_add_mapping(SDLK_RETURN,      INPUT_START, 0);
   input_add_mapping(SDLK_BACKSLASH,   INPUT_SELECT, 0);
   input_add_mapping(SDLK_ESCAPE,      INPUT_QUIT, 0);

   // controller mappings for devices 1-3
   for (int dev = 1; dev < MAX_INPUT_DEVICES; dev++) {
      input_add_mapping(SDL_CONTROLLER_BUTTON_DPAD_UP, INPUT_UP, dev);
      input_add_mapping(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, INPUT_RIGHT, dev);
      input_add_mapping(SDL_CONTROLLER_BUTTON_DPAD_DOWN, INPUT_DOWN, dev);
      input_add_mapping(SDL_CONTROLLER_BUTTON_DPAD_LEFT, INPUT_LEFT, dev);

      input_add_mapping(SDL_CONTROLLER_BUTTON_LEFTSTICK_UP, INPUT_UP, dev);
      input_add_mapping(SDL_CONTROLLER_BUTTON_LEFTSTICK_RIGHT, INPUT_RIGHT, dev);
      input_add_mapping(SDL_CONTROLLER_BUTTON_LEFTSTICK_DOWN, INPUT_DOWN, dev);
      input_add_mapping(SDL_CONTROLLER_BUTTON_LEFTSTICK_LEFT, INPUT_LEFT, dev);

      input_add_mapping(SDL_CONTROLLER_BUTTON_A, INPUT_A, dev);
      input_add_mapping(SDL_CONTROLLER_BUTTON_B, INPUT_B, dev);
      input_add_mapping(SDL_CONTROLLER_BUTTON_X, INPUT_C, dev);
      input_add_mapping(SDL_CONTROLLER_BUTTON_Y, INPUT_D, dev);

      input_add_mapping(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, INPUT_LB, dev);
      input_add_mapping(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, INPUT_RB, dev);
      input_add_mapping(SDL_CONTROLLER_BUTTON_LEFTTRIGGER, INPUT_LT, dev);
      input_add_mapping(SDL_CONTROLLER_BUTTON_RIGHTTRIGGER, INPUT_RT, dev);

      input_add_mapping(SDL_CONTROLLER_BUTTON_START, INPUT_START, dev);
      input_add_mapping(SDL_CONTROLLER_BUTTON_BACK, INPUT_SELECT, dev);
   }
}

void input_add_mapping(int raw_key, InputEvent event, int device_id) {
   if (g_input.mapping_count >= 256) return;

   InputMapping* mapping = &g_input.mappings[g_input.mapping_count];
   mapping->raw_key = raw_key;
   mapping->event = event;
   mapping->device_id = device_id;
   g_input.mapping_count++;
}

// context system
extern void scene_handle_input(InputEvent event, InputState state, int device_id);
void input_setup_handlers(void) {
   // setup function to register all handlers
   input_set_context_handler(CONTEXT_TITLE, scene_handle_input);
   input_set_context_handler(CONTEXT_MENU, scene_handle_input);
   input_set_context_handler(CONTEXT_PLAY, scene_handle_input);
   input_set_context_handler(CONTEXT_DBOX, scene_handle_input);
}

void input_set_context(GameContext context) {
   g_input.current_context = context;
   d_logv(2, "input context changed to %s", d_name_game_context(context));
}

void input_set_context_handler(GameContext context, InputHandler handler) {
   g_input.context_handlers[context] = handler;
}

// input queries
void input_cache_device_state(int device_id) {
   if (device_id == 0) return; // keyboard doesn't need caching
   
   InputDevice* device = &g_input.devices[device_id];
   if (!device->controller || !SDL_GameControllerGetAttached(device->controller)) {
      device->cache_valid = false;
      return;
   }
   
   // cache all button states at once
   for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++) {
      device->raw_buttons[i] = SDL_GameControllerGetButton(device->controller, i);
   }
   
   // cache axes
   for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++) {
      device->raw_axes[i] = SDL_GameControllerGetAxis(device->controller, i);
   }

   // handle triggers as buttons
   const int16_t trigger_threshold = 8192; // about 25% pressed
   device->raw_buttons[SDL_CONTROLLER_BUTTON_MAX] = 
      SDL_GameControllerGetAxis(device->controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > trigger_threshold;
   device->raw_buttons[SDL_CONTROLLER_BUTTON_MAX + 1] = 
      SDL_GameControllerGetAxis(device->controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > trigger_threshold;

   // handle left stick as directional buttons
   const int16_t stick_threshold = 16384; // about 50%
   int16_t left_x = SDL_GameControllerGetAxis(device->controller, SDL_CONTROLLER_AXIS_LEFTX);
   int16_t left_y = SDL_GameControllerGetAxis(device->controller, SDL_CONTROLLER_AXIS_LEFTY);

   device->raw_buttons[SDL_CONTROLLER_BUTTON_MAX + 2] = (left_x > stick_threshold); // stick right
   device->raw_buttons[SDL_CONTROLLER_BUTTON_MAX + 3] = (left_x < -stick_threshold); // stick left
   device->raw_buttons[SDL_CONTROLLER_BUTTON_MAX + 4] = (left_y > stick_threshold); // stick down
   device->raw_buttons[SDL_CONTROLLER_BUTTON_MAX + 5] = (left_y < -stick_threshold); // stick up
   
   device->cache_valid = true;
}

bool input_is_raw_pressed(InputEvent event, int device_id) {
   // look for mappings that match this event and device
   for (int i = 0; i < g_input.mapping_count; i++) {
      InputMapping* mapping = &g_input.mappings[i];
      if (mapping->event != event || mapping->device_id != device_id) continue;

      if (device_id == 0) {
         // keyboard
         const Uint8* keyboard_state = SDL_GetKeyboardState(NULL);
         SDL_Scancode scancode = SDL_GetScancodeFromKey(mapping->raw_key);
         if (keyboard_state[scancode]) return true;
      } else {
         // game controller using cached state
         InputDevice* device = &g_input.devices[device_id];
         if (device->cache_valid && mapping->raw_key < SDL_CONTROLLER_BUTTON_MAX + 6) { // + 6 is so important
            if (device->raw_buttons[mapping->raw_key]) return true;
         }
      }
   }
   return false; // if no mappings were pressed
}

bool input_pressed(InputEvent event, int device_id) {
   if (device_id < 0 || device_id >= MAX_INPUT_DEVICES) return false;
   if (!g_input.devices[device_id].connected) return false;
   return g_input.devices[device_id].states[event].pressed;
}

bool input_held(InputEvent event, int device_id) {
   if (device_id < 0 || device_id >= MAX_INPUT_DEVICES) return false;
   if (!g_input.devices[device_id].connected) return false;
   return g_input.devices[device_id].states[event].held;
}

bool input_released(InputEvent event, int device_id) {
   if (device_id < 0 || device_id >= MAX_INPUT_DEVICES) return false;
   if (!g_input.devices[device_id].connected) return false;
   return g_input.devices[device_id].states[event].released;
}

float input_duration(InputEvent event, int device_id) {
   if (device_id < 0 || device_id >= MAX_INPUT_DEVICES) return 0.0f;
   if (!g_input.devices[device_id].connected) return 0.0f;
   return g_input.devices[device_id].states[event].duration;
}

// buffered input queries
bool input_pressed_buffered(InputEvent event, int device_id, float buffer_time) {
   if (!g_input.input_buffer_enabled) return input_pressed(event, device_id);
   
   if (device_id < 0 || device_id >= MAX_INPUT_DEVICES) return false;
   if (!g_input.devices[device_id].connected) return false;
   
   InputState* state = &g_input.devices[device_id].states[event];
   return state->pressed || (state->released && state->duration <= buffer_time);
}

bool input_combo_pressed(InputEvent primary, InputEvent secondary, int device_id) {
   return input_held(primary, device_id) && input_pressed(secondary, device_id);
}

// utility functions
int input_find_device_by_guid(SDL_JoystickGUID guid) {
   for (int i = 1; i < MAX_INPUT_DEVICES; i++) {  // skip keyboard (device 0)
      if (g_input.devices[i].info.valid_guid && 
         memcmp(&g_input.devices[i].info.guid, &guid, sizeof(SDL_JoystickGUID)) == 0) {
         return i;
      }
   }
   return -1;
}

bool input_is_device_connected(int device_id) {
   if (device_id >= 0 && device_id < MAX_INPUT_DEVICES)
      return g_input.devices[device_id].connected;
   return 0;
}

void input_set_player_device(int player, int device_id) {
   if (player == 1) {
      g_input.player1_device = device_id;
      printf("Player 1 assigned to device %d\n", device_id);
   } else if (player == 2) {
      g_input.player2_device = device_id;
      printf("Player 2 assigned to device %d\n", device_id);
   }
}

int input_get_player_device(int player) {
   return (player == 1) ? g_input.player1_device : g_input.player2_device;
}


// TODO: pressing F2 takes a screenshot, and other function key utilities
