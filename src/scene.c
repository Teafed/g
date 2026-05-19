//dsjafsafh
///jsdfjsdakf;ksdkafjlsa
//jfkdsajfd;sajfkldasjf
//jfdsafjk;sagj;gsad
//fdhsuahgjksdahlgjk


#include "scene.h"
#include "input.h"
#include "debug.h"

#include <stdlib.h>
#include <string.h>

// the stack is a fixed-size array. scenes are pushed/popped at runtime but
// nesting is shallow in practice (title alone; or gameplay + pause)
#define SCENE_STACK_MAX 8
// per-call dispatch capacity. anything beyond this many edges per device
// per frame is dropped (loudly). 32 is well above realistic mashing
#define DISPATCH_BATCH 32

struct SceneStack {
   Scene* scenes[SCENE_STACK_MAX];
   int    depth;
   LiRen* renderer;
};

// --- internal helpers ---

static void scene_release(SceneStack* s, int index) {
   Scene* sc = s->scenes[index];
   if (!sc) return;
   if (sc->vtable && sc->vtable->destroy) {
      sc->vtable->destroy(sc, s->renderer);
   }
   free(sc);
   s->scenes[index] = NULL;
}

static void scene_run_init(SceneStack* s, Scene* sc) {
   if (!sc || !sc->vtable || !sc->vtable->init) return;
   sc->vtable->init(sc, s->renderer);
}

// --- lifecycle ---

SceneStack* scene_stack_create(LiRen* r) {
   SceneStack* s = calloc(1, sizeof(SceneStack));
   if (!s) {
      d_err("failed to allocate SceneStack");
      return NULL;
   }
   s->renderer = r;
   s->depth    = 0;
   return s;
}

void scene_stack_destroy(SceneStack* s) {
   if (!s) return;

   // tear down top-to-bottom
   while (s->depth > 0) {
      s->depth--;
      scene_release(s, s->depth);
   }

   free(s);
}

// --- transitions ---

void scene_stack_push(SceneStack* s, Scene* new_scene) {
   if (!s || !new_scene) return;
   if (s->depth >= SCENE_STACK_MAX) {
      d_err("scene stack overflow (max %d); refusing to push %s",
            SCENE_STACK_MAX, new_scene->name ? new_scene->name : "?");
      if (new_scene->vtable && new_scene->vtable->destroy) {
         new_scene->vtable->destroy(new_scene, s->renderer);
      }
      free(new_scene);
      return;
   }

   d_logv(2, "scene push: %s (depth %d -> %d)",
          new_scene->name ? new_scene->name : "?",
          s->depth, s->depth + 1);

   s->scenes[s->depth++] = new_scene;
   scene_run_init(s, new_scene);
}

void scene_stack_pop(SceneStack* s) {
   if (!s || s->depth <= 0) return;

   d_logv(2, "scene pop: %s (depth %d -> %d)",
          (s->scenes[s->depth - 1] && s->scenes[s->depth - 1]->name)
             ? s->scenes[s->depth - 1]->name : "?",
          s->depth, s->depth - 1);

   s->depth--;
   scene_release(s, s->depth);
}

void scene_stack_replace(SceneStack* s, Scene* new_scene) {
   if (!s || !new_scene) return;

   d_logv(2, "scene replace: %s -> %s",
          (s->depth > 0 && s->scenes[s->depth - 1]->name)
             ? s->scenes[s->depth - 1]->name : "(empty)",
          new_scene->name ? new_scene->name : "?");

   if (s->depth > 0) {
      s->depth--;
      scene_release(s, s->depth);
   }

   if (s->depth >= SCENE_STACK_MAX) {
      d_err("scene stack overflow during replace (shouldn't happen)");
      // leak-safe drop
      if (new_scene->vtable && new_scene->vtable->destroy) {
         new_scene->vtable->destroy(new_scene, s->renderer);
      }
      free(new_scene);
      return;
   }

   s->scenes[s->depth++] = new_scene;
   scene_run_init(s, new_scene);
}

void scene_stack_replace_all(SceneStack* s, Scene* new_scene) {
   if (!s || !new_scene) return;

   d_logv(2, "scene replace_all: clearing %d scene(s), pushing %s",
          s->depth, new_scene->name ? new_scene->name : "?");

   while (s->depth > 0) {
      s->depth--;
      scene_release(s, s->depth);
   }

   s->scenes[s->depth++] = new_scene;
   scene_run_init(s, new_scene);
}

// --- helpers ---

Scene* scene_stack_top(SceneStack* s) {
   if (!s || s->depth <= 0) return NULL;
   return s->scenes[s->depth - 1];
}

int scene_stack_depth(SceneStack* s) {
   return s ? s->depth : 0;
}

// --- per-frame dispatch ---

void scene_stack_update(SceneStack* s, float dt) {
   if (!s || s->depth <= 0) return;
   Scene* top = s->scenes[s->depth - 1];
   if (top && top->vtable && top->vtable->update) {
      top->vtable->update(top, dt);
   }
}

void scene_stack_render(SceneStack* s, LiRen* r) {
   if (!s) return;
   // bottom-to-top
   // scenes below the active one stay drawn under any pushed overlay
   // topmost scene is responsible for whether it obscures lower layers
   for (int i = 0; i < s->depth; i++) {
      Scene* sc = s->scenes[i];
      if (sc && sc->vtable && sc->vtable->render) {
         sc->vtable->render(sc, r);
      }
   }
}

// drain edge events from every connected device, forward to top scene's handle_input
void scene_stack_dispatch_input(SceneStack* s) {
   if (!s || s->depth <= 0) return;
   Scene* top = s->scenes[s->depth - 1];
   if (!top || !top->vtable || !top->vtable->handle_input) return;

   for (int dev = 0; dev < MAX_INPUT_DEVICES; dev++) {
      if (!input_is_device_connected(dev)) continue;

      InputEventEntry batch[DISPATCH_BATCH];
      int n;
      bool device_done = false;
      do {
         n = input_drain_events(dev, batch, DISPATCH_BATCH);
         for (int i = 0; i < n; i++) {
            InputEventEntry* e = &batch[i];
            InputState st = {
               .pressed    = e->is_press,
               .released   = !e->is_press,
               .held       = e->is_press,
               .duration   = 0.0f,
               .prev_state = false,
            };

            if (s->depth <= 0) return;
            Scene* cur = s->scenes[s->depth - 1];
            if (!cur || !cur->vtable || !cur->vtable->handle_input) continue;

            cur->vtable->handle_input(cur, e->event, st, dev);

            // if the top scene changed, stop queued events to whatever is on top now
            // remaining events in the batch dropped intentionally
            if (s->depth <= 0 || s->scenes[s->depth - 1] != cur) {
               while (input_drain_events(dev, batch, DISPATCH_BATCH) > 0) {
                  // discard
               }
               device_done = true;
               break;
            }
         }
      } while (n == DISPATCH_BATCH && !device_done);
   }
}
