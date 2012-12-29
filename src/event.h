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

struct buffered_status
{
  enum keycode key_pressed;
  enum keycode key;
  enum keycode key_repeat;
  enum keycode key_release;
  Uint16 unicode;
  Uint16 unicode_repeat;
  Uint32 keypress_time;
  Uint32 mouse_x;
  Uint32 mouse_y;
  Uint32 real_mouse_x;
  Uint32 real_mouse_y;
  Uint32 mouse_button;
  Uint32 mouse_repeat;
  Uint32 mouse_repeat_state;
  Uint32 mouse_time;
  Sint32 mouse_drag_state;
  Uint32 mouse_button_state;
  bool caps_status;
  bool numlock_status;
  bool mouse_moved;
  Sint8 axis[16][16];
  Uint8 keymap[512];
};

struct input_status
{
  struct buffered_status *buffer;
  Uint16 load_offset;
  Uint16 store_offset;

  enum keycode key_repeat_stack[KEY_REPEAT_STACK_SIZE];
  Uint16 unicode_repeat_stack[KEY_REPEAT_STACK_SIZE];
  Uint32 repeat_stack_pointer;
  
  enum keycode joystick_button_map[16][256];
  enum keycode joystick_axis_map[16][16][2];

  bool unfocus_pause;
};

enum keycode_type
{
  keycode_pc_xt,
  keycode_internal,
  keycode_unicode
};

CORE_LIBSPEC void init_event(void);

struct buffered_status *store_status(void);

CORE_LIBSPEC bool update_event_status(void);
CORE_LIBSPEC Uint32 update_event_status_delay(void);
CORE_LIBSPEC Uint32 get_key(enum keycode_type type);
CORE_LIBSPEC void get_mouse_position(int *x, int *y);
CORE_LIBSPEC void get_real_mouse_position(int *x, int *y);
CORE_LIBSPEC Uint32 get_mouse_press(void);
CORE_LIBSPEC Uint32 get_mouse_press_ext(void);
CORE_LIBSPEC Uint32 get_mouse_status(void);
CORE_LIBSPEC void warp_mouse(Uint32 x, Uint32 y);
CORE_LIBSPEC Uint32 get_mouse_drag(void);
CORE_LIBSPEC bool get_alt_status(enum keycode_type type);
CORE_LIBSPEC bool get_shift_status(enum keycode_type type);
CORE_LIBSPEC bool get_ctrl_status(enum keycode_type type);
CORE_LIBSPEC void initialize_joysticks(void);
CORE_LIBSPEC void key_press(struct buffered_status *status, enum keycode key,
 Uint16 unicode_key);
CORE_LIBSPEC void key_release(struct buffered_status *status, enum keycode key);
CORE_LIBSPEC void wait_for_key_release(Uint32 index);
CORE_LIBSPEC void wait_for_mouse_release(Uint32 mouse_button);

// Implemented by "drivers" (SDL, Wii, and NDS currently)
void __wait_event(void);
bool __update_event_status(void);

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
void set_unfocus_pause(bool value);
void set_num_buffered_events(Uint8 value);

void real_warp_mouse(Uint32 x, Uint32 y);

__M_END_DECLS

#endif // __EVENT_H
