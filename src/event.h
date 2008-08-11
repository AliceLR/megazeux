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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef EVENT_H
#define EVENT_H

#include "SDL.h"

#define KEY_REPEAT_START 250
#define KEY_REPEAT_RATE 33

#define MOUSE_REPEAT_START 200
#define MOUSE_REPEAT_RATE 10

#define UPDATE_DELAY 30

#define KEY_REPEAT_STACK_SIZE 32

typedef struct
{
  Uint8 SDL_keymap[512];
  SDLKey last_SDL_pressed;
  SDLKey last_SDL;
  SDLKey last_SDL_repeat;
  SDLKey last_SDL_release;
  Uint16 last_unicode;
  Uint16 last_unicode_repeat;
  SDLKey last_SDL_repeat_stack[KEY_REPEAT_STACK_SIZE];
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
  Uint32 unfocus_pause;

  // Joystick map information
  SDLKey joystick_button_map[16][256];
  SDLKey joystick_axis_map[16][16][2];
  Uint32 last_axis[16][16];

  Uint32 last_update_time;
} input_status;

typedef enum
{
  keycode_pc_xt,
  keycode_SDL,
  keycode_unicode
} keycode_type;

Uint32 process_event(SDL_Event *event);
void wait_event();
Uint32 update_autorepeat();
Uint32 update_event_status();
Uint32 update_event_status_delay();
Uint32 get_key(keycode_type type);
Uint32 get_last_key(keycode_type type);
void force_last_key(keycode_type type, int val);
Uint32 get_key_status(keycode_type type, Uint32 index);
Uint32 get_mouse_movement(int *x, int *y);
void get_mouse_position(int *x, int *y);
void get_real_mouse_position(int *x, int *y);
Uint32 get_mouse_press();
Uint32 get_mouse_press_ext();
Uint32 get_mouse_status();
Uint32 get_mouse_status_ext();
void warp_mouse(Uint32 x, Uint32 y);
void warp_mouse_x(Uint32 x);
void warp_mouse_y(Uint32 y);
Uint32 get_mouse_x();
Uint32 get_mouse_y();
Uint32 get_mouse_drag();
Uint32 convert_SDL_xt(SDLKey key);
SDLKey convert_xt_SDL(Uint32 key, SDLKey *second);
Uint32 get_last_key_released(keycode_type type);
int get_alt_status(keycode_type type);
int get_shift_status(keycode_type type);
int get_ctrl_status(keycode_type type);
void initialize_joysticks();
void map_joystick_axis(int joystick, int axis, SDLKey min_key,
 SDLKey max_key);
void map_joystick_button(int joystick, int button, SDLKey key);
void set_refocus_pause(int val);

extern input_status input;

#endif
