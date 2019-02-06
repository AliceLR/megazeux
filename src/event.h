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

#define UPDATE_DELAY 16

#define KEY_REPEAT_STACK_SIZE 32

#define STATUS_NUM_KEYCODES 512

#define MAX_JOYSTICKS         16
#define MAX_JOYSTICK_AXES     16
#define MAX_JOYSTICK_BUTTONS  256

#define MOUSE_BUTTON(x)         (1 << ((x) - 1))
#define MOUSE_BUTTON_LEFT       1
#define MOUSE_BUTTON_MIDDLE     2
#define MOUSE_BUTTON_RIGHT      3

// Extended buttons.
// SDL 1.2 and SDL 2 both carry through X11 values.
#ifdef CONFIG_X11

#define MOUSE_BUTTON_WHEELUP    4
#define MOUSE_BUTTON_WHEELDOWN  5
#define MOUSE_BUTTON_WHEELLEFT  6
#define MOUSE_BUTTON_WHEELRIGHT 7
#define MOUSE_BUTTON_X1         8
#define MOUSE_BUTTON_X2         9

#elif defined(CONFIG_SDL)

#include <SDL_version.h>

// SDL 2 maps X1 and X2, but has a separate wheel event that we map.
#if SDL_VERSION_ATLEAST(2,0,0)
#define MOUSE_BUTTON_X1         4
#define MOUSE_BUTTON_X2         5
#define MOUSE_BUTTON_WHEELUP    6
#define MOUSE_BUTTON_WHEELDOWN  7
#define MOUSE_BUTTON_WHEELLEFT  8
#define MOUSE_BUTTON_WHEELRIGHT 9
#endif //SDL_VERSION_ATLEAST(2,0,0)

#endif //extended buttons

// SDL 1.2 and non-SDL default.
// Note: SDL 1.2 has no wheel left/right support.
#ifndef MOUSE_BUTTON_WHEELUP
#define MOUSE_BUTTON_WHEELUP    4
#define MOUSE_BUTTON_WHEELDOWN  5
#define MOUSE_BUTTON_X1         6
#define MOUSE_BUTTON_X2         7
#define MOUSE_BUTTON_WHEELLEFT  8
#define MOUSE_BUTTON_WHEELRIGHT 9
#endif //defaults

// Capture F12 presses to save screenshots if true.
extern boolean enable_f12_hack;

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
  Uint32 mouse_last_x;
  Uint32 mouse_last_y;
  Uint32 real_mouse_x;
  Uint32 real_mouse_y;
  Uint32 real_mouse_last_x;
  Uint32 real_mouse_last_y;
  Uint32 mouse_button;
  Uint32 mouse_repeat;
  Uint32 mouse_repeat_state;
  Uint32 mouse_time;
  Sint32 mouse_drag_state;
  Uint32 mouse_button_state;
  boolean caps_status;
  boolean numlock_status;
  boolean mouse_moved;
  boolean exit_status;
  enum joystick_action joystick_action;
  enum joystick_action joystick_repeat;
  Uint32 joystick_repeat_state;
  Uint32 joystick_repeat_id;
  Uint32 joystick_time;
  boolean joystick_hat[MAX_JOYSTICKS][4];
  boolean joystick_button[MAX_JOYSTICKS][MAX_JOYSTICK_BUTTONS];
  Sint16 joystick_axis[MAX_JOYSTICKS][MAX_JOYSTICK_AXES];
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

  int primary_joystick;
  Sint16 joystick_global_button_map[MAX_JOYSTICKS][MAX_JOYSTICK_BUTTONS];
  Sint16 joystick_global_axis_map[MAX_JOYSTICKS][MAX_JOYSTICK_AXES][2];
  Sint16 joystick_global_hat_map[MAX_JOYSTICKS][4];
  Sint16 joystick_global_action_map[MAX_JOYSTICKS][NUM_JOYSTICK_ACTIONS];
  Sint16 joystick_game_button_map[MAX_JOYSTICKS][MAX_JOYSTICK_BUTTONS];
  Sint16 joystick_game_axis_map[MAX_JOYSTICKS][MAX_JOYSTICK_AXES][2];
  Sint16 joystick_game_hat_map[MAX_JOYSTICKS][4];
  Sint16 joystick_game_action_map[MAX_JOYSTICKS][NUM_JOYSTICK_ACTIONS];

  boolean unfocus_pause;
};

// regular keycode_internal treats numpad keys as unique keys
// wrt_numlock translates them to numeric/navigation based on numlock status
enum keycode_type
{
  keycode_pc_xt,
  keycode_internal,
  keycode_internal_wrt_numlock,
  keycode_unicode
};

CORE_LIBSPEC void init_event(void);

struct buffered_status *store_status(void);

CORE_LIBSPEC boolean update_event_status(void);
CORE_LIBSPEC Uint32 update_event_status_delay(void);
CORE_LIBSPEC void update_event_status_intake(void);
CORE_LIBSPEC void force_release_all_keys(void);
CORE_LIBSPEC Uint32 get_key(enum keycode_type type);
CORE_LIBSPEC Uint32 get_last_key(enum keycode_type type);
CORE_LIBSPEC Uint32 get_key_status(enum keycode_type type, Uint32 index);
CORE_LIBSPEC void get_mouse_position(int *x, int *y);
CORE_LIBSPEC void get_real_mouse_position(int *x, int *y);
CORE_LIBSPEC void get_mouse_movement(int *delta_x, int *delta_y);
CORE_LIBSPEC void get_real_mouse_movement(int *delta_x, int *delta_y);
CORE_LIBSPEC Uint32 get_mouse_press(void);
CORE_LIBSPEC Uint32 get_mouse_press_ext(void);
CORE_LIBSPEC boolean get_mouse_held(int button);
CORE_LIBSPEC Uint32 get_mouse_status(void);
CORE_LIBSPEC void warp_mouse(Uint32 x, Uint32 y);
CORE_LIBSPEC void warp_real_mouse_x(Uint32 x);
CORE_LIBSPEC void warp_real_mouse_y(Uint32 y);
CORE_LIBSPEC Uint32 get_mouse_drag(void);
CORE_LIBSPEC boolean get_alt_status(enum keycode_type type);
CORE_LIBSPEC boolean get_shift_status(enum keycode_type type);
CORE_LIBSPEC boolean get_ctrl_status(enum keycode_type type);
CORE_LIBSPEC void initialize_joysticks(void);
CORE_LIBSPEC void key_press(struct buffered_status *status, enum keycode key,
 Uint16 unicode_key);
CORE_LIBSPEC void key_release(struct buffered_status *status, enum keycode key);
CORE_LIBSPEC boolean get_exit_status(void);
CORE_LIBSPEC boolean set_exit_status(boolean value);
CORE_LIBSPEC boolean peek_exit_input(void);
CORE_LIBSPEC Uint32 get_joystick_action(void);

// Implemented by "drivers" (SDL, Wii, and NDS currently)
void __wait_event(int timeout);
boolean __update_event_status(void);

#ifdef CONFIG_SDL
// Currently only supported by SDL.
boolean __peek_exit_input(void);

// Older SDL versions lack SDL_WaitEventTimeout, and our compatibility
// implementation needs this function to work properly.
boolean update_autorepeat(void);
#endif

#ifdef CONFIG_NDS
const struct buffered_status *load_status(void);
#endif

void wait_event(int timeout);
void force_last_key(enum keycode_type type, int val);
void warp_mouse_x(Uint32 x);
void warp_mouse_y(Uint32 y);
Uint32 get_mouse_x(void);
Uint32 get_mouse_y(void);
Uint32 get_real_mouse_x(void);
Uint32 get_real_mouse_y(void);
Uint32 get_last_key_released(enum keycode_type type);
void set_unfocus_pause(boolean value);
void set_num_buffered_events(Uint8 value);

void joystick_map_button(int joystick, int button, const char *value,
 boolean is_global);
void joystick_map_axis(int joystick, int axis, const char *neg,
 const char *pos, boolean is_global);
void joystick_map_hat(int joystick, const char *up, const char *down,
 const char *left, const char *right, boolean is_global);
void joystick_map_action(int joystick, const char *action, int value,
 boolean is_global);
void joystick_reset_game_map(void);
void joystick_set_game_mode(boolean enable);
void joystick_set_game_bindings(boolean enable);
void joystick_set_legacy_loop_hacks(boolean enable);
void joystick_button_press(struct buffered_status *status,
 int joystick, int button);
void joystick_button_release(struct buffered_status *status,
 int joystick, int button);
void joystick_hat_update(struct buffered_status *status,
 int joystick, boolean up, boolean down, boolean left, boolean right);
void joystick_axis_update(struct buffered_status *status,
 int joystick, int axis, Sint16 value);
void joystick_release_all(struct buffered_status *status, int joystick);

void real_warp_mouse(int x, int y);

__M_END_DECLS

#endif // __EVENT_H
