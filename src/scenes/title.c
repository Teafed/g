#include "scenes/title.h"
#include "scenes/main_menu.h"
#include "scene.h"
#include "game.h"
#include "input.h"
#include "debug.h"

#include <liren/liren.h>
#include <stdlib.h>

typedef struct {
   LR_LayerHandle bg_layer;
   LR_LayerHandle border_layer;
   LR_LayerHandle box_layer;

   LR_FontHandle title_font;
   LR_FontHandle box_font;
   LR_Rect moving_box;
   uint8_t box_color;
   bool    x_forward;
   bool    y_forward;

   int viewport_w, viewport_h;
} TitleData;

// TODO: read this from the actual palette
#define TITLE_BOX_COLOR_MAX 35

// --- helpers ---

static void cycle_color(TitleData* d) {
   d->box_color = (d->box_color + 1) % TITLE_BOX_COLOR_MAX;
}
int temp = 0;
static void bounce_box(TitleData* d, int amt) {
   temp++;
   if (temp % 30 != 0) return;
   
   if (d->x_forward) {
      if (d->moving_box.x + d->moving_box.w < d->viewport_w)
         d->moving_box.x += amt;
      else { d->moving_box.x -= amt; d->x_forward = false; cycle_color(d); }
   } else {
      if (d->moving_box.x >= 0)
         d->moving_box.x -= amt;
      else { d->moving_box.x += amt; d->x_forward = true; cycle_color(d); }
   }
   if (d->y_forward) {
      if (d->moving_box.y + d->moving_box.h < d->viewport_h)
         d->moving_box.y += amt;
      else { d->moving_box.y -= amt; d->y_forward = false; cycle_color(d); }
   } else {
      if (d->moving_box.y >= 0)
         d->moving_box.y -= amt;
      else { d->moving_box.y += amt; d->y_forward = true; cycle_color(d); }
   }
}

static void draw_title_plate(TitleData* d, LiRen* r) {
   lr_draw_fill(r, d->bg_layer, 0);  // mono-white

   LR_Rect border_t = {0, 0, 640, 4};
   LR_Rect border_l = {0, 0, 4, 480};
   LR_Rect border_b = {0, 476, 640, 4};
   LR_Rect border_r = {636, 0, 4, 480};
   LR_Rect title_rect = {90, 20, 420, 60};

   lr_draw_rect(r, d->border_layer, border_t, 10);
   lr_draw_rect(r, d->border_layer, border_l, 10);
   lr_draw_rect(r, d->border_layer, border_b, 10);
   lr_draw_rect(r, d->border_layer, border_r, 10);
   lr_draw_rect(r, d->border_layer, title_rect, 7);

   lr_draw_text(r, d->border_layer, d->title_font, title_rect.x, title_rect.y, 4, "Title screen"); 
}

static void draw_box(TitleData* d, LiRen* r) {
   lr_draw_rect(r, d->box_layer, d->moving_box, d->box_color);
   lr_draw_text(r, d->box_layer, d->box_font, d->moving_box.x, d->moving_box.y, 4, "DVD");
}

// --- vtable callbacks ---

static void title_init(Scene* s, LiRen* r) {
   TitleData* d = (TitleData*)s->data;

   d->bg_layer     = lr_layer_create(r, false);
   d->border_layer = lr_layer_create(r, false);
   d->box_layer    = lr_layer_create(r, false);

   d->title_font = 0;
   d->box_font   = 1;
   d->moving_box = (LR_Rect){0, 0, 100, 100};
   d->box_color  = 5;
   d->x_forward  = true;
   d->y_forward  = true;

   // reset any prior player-device assignments
   input_reset_player_devices();
}

static void title_update(Scene* s, float dt) {
   (void)dt;
   TitleData* d = (TitleData*)s->data;
   bounce_box(d, 4);
}

static void title_render(Scene* s, LiRen* r) {
   TitleData* d = (TitleData*)s->data;

   // cache viewport so update can clamp against it next frame
   lr_get_viewport_size(r, &d->viewport_w, &d->viewport_h);

   draw_title_plate(d, r);
   draw_box(d, r);
}

extern void game_shutdown(void);
static void title_handle_input(Scene* s, InputEvent e, InputState st,
                               int device_id) {
   (void)s;
   if (!st.pressed) return;

   if (e == INPUT_START || e == INPUT_A) {
      // first input claims player 1
      if (input_get_player_device(1) == -1) {
         input_set_player_device(1, device_id);
         scene_stack_replace(game_get_stack(), main_menu_scene_create());
      } else {
         d_log("title: player 1 already assigned, ignoring");
      }
   } else if (e == INPUT_B || e == INPUT_SELECT) {
      game_shutdown();
   }
}

static void title_destroy(Scene* s, LiRen* r) {
   TitleData* d = (TitleData*)s->data;
   lr_layer_destroy(r, d->box_layer);
   lr_layer_destroy(r, d->border_layer);
   lr_layer_destroy(r, d->bg_layer);
   free(d);
}

// --- vtable singleton + constructor ---

static const SceneVTable TITLE_VTABLE = {
   .input_context = CONTEXT_TITLE,
   .init          = title_init,
   .update        = title_update,
   .render        = title_render,
   .handle_input  = title_handle_input,
   .destroy       = title_destroy,
};

Scene* title_scene_create(void) {
   Scene* s = calloc(1, sizeof(Scene));
   if (!s) {
      d_err("failed to allocate Scene (title)");
      return NULL;
   }
   TitleData* d = calloc(1, sizeof(TitleData));
   if (!d) {
      d_err("failed to allocate TitleData");
      free(s);
      return NULL;
   }
   s->vtable = &TITLE_VTABLE;
   s->data   = d;
   s->name   = "title";
   return s;
}
