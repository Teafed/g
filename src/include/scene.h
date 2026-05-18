#ifndef SCENE_H
#define SCENE_H

#include "input.h"
#include <liren/liren.h>

typedef struct Scene Scene;

// scenes never store state at file scope; everything lives in data,
// lets two instances of the same scene type coexist (pause over gameplay)
typedef struct {
   // scenes should never call input_set_context themselves
   GameContext input_context;

   void (*init) (Scene* s, LiRen* r);
   void (*update) (Scene* s, float dt);
   void (*render) (Scene* s, LiRen* r);
   void (*handle_input) (Scene* s, InputEvent e, InputState st, int device_id);
   void (*destroy) (Scene* s, LiRen* r);
} SceneVTable;

struct Scene {
   const SceneVTable* vtable;
   void* data;         // per-scene state, owned by the scene
   const char* name;   // for logging (static string, not copied)
};

typedef struct SceneStack SceneStack;

SceneStack* scene_stack_create(LiRen* r);
void scene_stack_destroy(SceneStack* s);
void scene_stack_push(SceneStack* s, Scene* new_scene);
void scene_stack_pop(SceneStack* s);
void scene_stack_replace(SceneStack* s, Scene* new_scene);
void scene_stack_replace_all(SceneStack* s, Scene* new_scene);
Scene* scene_stack_top(SceneStack* s);
int scene_stack_depth(SceneStack* s);
void scene_stack_update(SceneStack* s, float dt);
void scene_stack_render(SceneStack* s, LiRen* r);
void scene_stack_handle_input(SceneStack* s, InputEvent e, InputState st, int device_id);

#endif
