/* MegaZeux
 *
 * Copyright (C) 2002 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef __EVENT_H
#define __EVENT_H

#include "compat.h"

__M_BEGIN_DECLS

#include "platform.h"
#include "keysym.h"

#define UPDATE_DELAY 30

#define KEY_REPEAT_STACK_SIZE 32

#define MOUSE_BUTTON(x)         (1 << ((x) - 1))
#define MOUSE_BUTTON_LEFT       1
#define MOUSE_BUTTON_MIDDLE     2
#define MOUSE_BUTTON_RIGHT      3
#define MOUSE_BUTTON_WHEELUP    4
#define MOUSE_BUTTON_WHEELDOWN  5

struct input_status
{
  Uint8 keymap[512];
  enum keycode last_key_pressed;
  enum keycode last_key;
  enum keycode last_key_repeat;
  enum keycode last_key_release;
  Uint16 last_unicode;
  Uint16 last_unicode_repeat;
  enum keycode last_key_repeat_stack[KEY_REPEAT_STACK_SIZE];
  Uint16 last_unicode_repeat_stack[KEY_REPEAT_STACK_SIZE];
  Uint32 repeat_stack_pointer;
  Uint32 last_keypress_time;
  Uint32 mouse_x;
  Uint32 mouse_y;
  Uint32 real_mouse_x;
  Uint32 real_mouse_y;
  Uint32 mouse_moved;
  Uint32 last_mouse_button;
  Uint32 last_mouse_repeat;
  Uint32 last_mouse_repeat_state;
  Uint32 last_mouse_time;
  Sint32 mouse_drag_state;
  Uint32 mouse_button_state;
  Uint32 caps_status;
  Sint32 numlock_status;
  Uint32 unfocus_pause;

  // Joystick map information
  enum keycode joystick_button_map[16][256];
  enum keycode joystick_axis_map[16][16][2];
  Uint32 last_axis[16][16];

  Uint32 last_update_time;
};

enum keycode_type
{
  keycode_pc_xt,
  keycode_internal,
  keycode_unicode
};

CORE_LIBSPEC Uint32 update_event_status(void);
CORE_LIBSPEC Uint32 update_event_status_delay(void);
CORE_LIBSPEC Uint32 get_key(enum keycode_type type);
CORE_LIBSPEC void get_mouse_position(int *x, int *y);
CORE_LIBSPEC void get_real_mouse_position(int *x, int *y);
CORE_LIBSPEC Uint32 get_mouse_press(void);
CORE_LIBSPEC Uint32 get_mouse_press_ext(void);
CORE_LIBSPEC Uint32 get_mouse_status(void);
CORE_LIBSPEC void warp_mouse(Uint32 x, Uint32 y);
CORE_LIBSPEC Uint32 get_mouse_drag(void);
CORE_LIBSPEC int get_alt_status(enum keycode_type type);
CORE_LIBSPEC int get_shift_status(enum keycode_type type);
CORE_LIBSPEC int get_ctrl_status(enum keycode_type type);
CORE_LIBSPEC void initialize_joysticks(void);

extern struct input_status input;

void wait_event(void);
Uint32 get_last_key(enum keycode_type type);
void force_last_key(enum keycode_type type, int val);
Uint32 get_key_status(enum keycode_type type, Uint32 index);
void warp_mouse_x(Uint32 x);
void warp_mouse_y(Uint32 y);
void warp_real_mouse_x(Uint32 x);
void warp_real_mouse_y(Uint32 y);
Uint32 get_mouse_x(void);
Uint32 get_mouse_y(void);
Uint32 get_real_mouse_x(void);
Uint32 get_real_mouse_y(void);
Uint32 get_last_key_released(enum keycode_type type);
void map_joystick_axis(int joystick, int axis, enum keycode min_key,
 enum keycode max_key);
void map_joystick_button(int joystick, int button, enum keycode key);
void set_refocus_pause(int val);

Uint32 update_autorepeat(void);

void real_warp_mouse(Uint32 x, Uint32 y);

#ifdef CONFIG_NDS
void nds_inject_input(void);
#endif

__M_END_DECLS

#endif // __EVENT_H
