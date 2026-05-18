//dsjafsafh
///jsdfjsdakf;ksdkafjlsa
//jfkdsajfd;sajfkldasjf
//jfdsafjk;sagj;gsad
//fdhsuahgjksdahlgjk


#include "scene.h"
#include "debug.h"

#include <stdlib.h>
#include <string.h>

// the stack is a fixed-size array. scenes are pushed/popped at runtime but
// nesting is shallow in practice (title alone; or gameplay + pause)
#define SCENE_STACK_MAX 8

struct SceneStack {
   Scene* scenes[SCENE_STACK_MAX];
   int    depth;
   LiRen* renderer;
};

// --- input handler shim ---
//
// input.h's context system wires one InputHandler per GameContext. our
// vtable's handle_input takes a Scene*, so we can't register the vtable
// callback directly. instead, for each context we register a small
// dispatch function that looks up the current top scene and forwards.
//
// because the dispatcher is identical for every context, we just point
// every context at the same shim. when the active context changes (via
// input_set_context on push/pop), the shim gets called for whichever
// context is active, but it always dispatches to the top scene's
// handle_input. the GameContext value the input system passes around is
// effectively just "which scene is on top right now."
//
// the shim needs access to the stack to find the top scene. since input
// callbacks don't carry user data (see InputHandler typedef), we keep a
// file-static pointer to the stack. set on stack_create, cleared on
// stack_destroy. only one stack exists per process.

static SceneStack* g_stack_for_input = NULL;

static void scene_input_shim(InputEvent e, InputState st, int device_id) {
   if (!g_stack_for_input) return;
   if (g_stack_for_input->depth <= 0) return;

   Scene* top = g_stack_for_input->scenes[g_stack_for_input->depth - 1];
   if (top && top->vtable && top->vtable->handle_input) {
      top->vtable->handle_input(top, e, st, device_id);
   }
}

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

// apply the top scene's input context. called whenever the top changes
// (push, pop, replace, replace_all). no-op if the stack is empty
static void apply_top_context(SceneStack* s) {
   if (s->depth <= 0) return;
   Scene* top = s->scenes[s->depth - 1];
   if (top && top->vtable) {
      input_set_context(top->vtable->input_context);
   }
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

   // wire every input context to our shim. the shim ignores the context
   // value and dispatches to whichever scene is on top.
   g_stack_for_input = s;
   for (int ctx = 0; ctx < CONTEXT_MAX; ctx++) {
      input_set_context_handler((GameContext)ctx, scene_input_shim);
   }

   return s;
}

void scene_stack_destroy(SceneStack* s) {
   if (!s) return;

   // tear down top-to-bottom
   while (s->depth > 0) {
      s->depth--;
      scene_release(s, s->depth);
   }

   // clear shim's stack pointer so stray inputs after destroy are no-op
   if (g_stack_for_input == s) g_stack_for_input = NULL;

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
   apply_top_context(s);
}

void scene_stack_pop(SceneStack* s) {
   if (!s || s->depth <= 0) return;

   d_logv(2, "scene pop: %s (depth %d -> %d)",
          (s->scenes[s->depth - 1] && s->scenes[s->depth - 1]->name)
             ? s->scenes[s->depth - 1]->name : "?",
          s->depth, s->depth - 1);

   s->depth--;
   scene_release(s, s->depth);

   // restore the now-top scene's input context. if the stack is empty,
   // apply_top_context is a no-op
   apply_top_context(s);
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
   apply_top_context(s);
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
   apply_top_context(s);
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

void scene_stack_handle_input(SceneStack* s,
                              InputEvent e, InputState st, int device_id) {
   // only uses for callers that want to bypass the input system's context
   // dispatch and feed input directly to the top scene
   if (!s || s->depth <= 0) return;
   Scene* top = s->scenes[s->depth - 1];
   if (top && top->vtable && top->vtable->handle_input) {
      top->vtable->handle_input(top, e, st, device_id);
   }
}
