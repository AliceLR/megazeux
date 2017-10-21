/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

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

// from platform.c
bool ctr_is_2d(void);

void ctr_keyboard_init(struct ctr_render_data *render_data);
void ctr_keyboard_draw(struct ctr_render_data *render_data);
bool ctr_keyboard_update(struct buffered_status *status);
bool ctr_keyboard_force_zoom_out(void);

#endif /* __3DS_KEYBOARD_H__ */
