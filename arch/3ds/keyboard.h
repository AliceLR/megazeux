#ifndef __3DS_KEYBOARD_H__
#define __3DS_KEYBOARD_H__

#include "../../src/event.h"
#include "render.h"

typedef struct
{
  u16 x, y, w, h;
  enum keycode keycode;
  u8 flags;
} touch_area_t;

void ctr_keyboard_init(struct ctr_render_data *render_data);
void ctr_keyboard_draw(struct ctr_render_data *render_data);
bool ctr_keyboard_update(struct buffered_status *status);

#endif /* __3DS_KEYBOARD_H__ */
