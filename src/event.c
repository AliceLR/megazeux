/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#include "event.h"
#include "graphics.h"
#include "util.h"
#include "platform.h"

#include <stdlib.h>
#include <string.h>

#define KEY_REPEAT_START    250
#define KEY_REPEAT_RATE     33

#define MOUSE_REPEAT_START  200
#define MOUSE_REPEAT_RATE   10

static Uint32 last_update_time;

struct input_status input;

static Uint8 num_buffered_events = 1;

struct buffered_status *store_status(void)
{
  return &input.buffer[input.store_offset];
}

static const struct buffered_status *load_status(void)
{
  return (const struct buffered_status *)&input.buffer[input.load_offset];
}

static void bump_status(void)
{
  Uint16 last_store_offset = input.store_offset;

  input.store_offset = (input.store_offset + 1) % num_buffered_events;
  input.load_offset = (input.store_offset + 1) % num_buffered_events;

  // No event buffering; nothing to do
  if(input.store_offset == input.load_offset)
    return;

  // Some events can "echo" from the previous buffer
  memcpy(store_status(), &input.buffer[last_store_offset],
         sizeof(struct buffered_status));
}

void init_event(void)
{
  input.buffer = ccalloc(num_buffered_events, sizeof(struct buffered_status));
  input.load_offset = num_buffered_events - 1;
  input.store_offset = 0;
}

static Uint32 convert_internal_xt(enum keycode key)
{
  switch(key)
  {
    case IKEY_ESCAPE: return 0x01;
    case IKEY_F1: return 0x3B;
    case IKEY_F2: return 0x3C;
    case IKEY_F3: return 0x3D;
    case IKEY_F4: return 0x3E;
    case IKEY_F5: return 0x3F;
    case IKEY_F6: return 0x40;
    case IKEY_F7: return 0x41;
    case IKEY_F8: return 0x42;
    case IKEY_F9: return 0x43;
    case IKEY_F10: return 0x44;
    case IKEY_F11: return 0x57;
    case IKEY_F12: return 0x58;
    case IKEY_BACKQUOTE: return 0x29;
    case IKEY_1: return 0x02;
    case IKEY_2: return 0x03;
    case IKEY_3: return 0x04;
    case IKEY_4: return 0x05;
    case IKEY_5: return 0x06;
    case IKEY_6: return 0x07;
    case IKEY_7: return 0x08;
    case IKEY_8: return 0x09;
    case IKEY_9: return 0x0A;
    case IKEY_0: return 0x0B;
    case IKEY_MINUS: return 0x0C;
    case IKEY_EQUALS: return 0x0D;
    case IKEY_BACKSLASH: return 0x2B;
    case IKEY_BACKSPACE: return 0x0E;
    case IKEY_TAB: return 0x0F;
    case IKEY_q: return 0x10;
    case IKEY_w: return 0x11;
    case IKEY_e: return 0x12;
    case IKEY_r: return 0x13;
    case IKEY_t: return 0x14;
    case IKEY_y: return 0x15;
    case IKEY_u: return 0x16;
    case IKEY_i: return 0x17;
    case IKEY_o: return 0x18;
    case IKEY_p: return 0x19;
    case IKEY_LEFTBRACKET: return 0x1A;
    case IKEY_RIGHTBRACKET: return 0x1B;
    case IKEY_CAPSLOCK: return 0x3A;
    case IKEY_a: return 0x1E;
    case IKEY_s: return 0x1F;
    case IKEY_d: return 0x20;
    case IKEY_f: return 0x21;
    case IKEY_g: return 0x22;
    case IKEY_h: return 0x23;
    case IKEY_j: return 0x24;
    case IKEY_k: return 0x25;
    case IKEY_l: return 0x26;
    case IKEY_SEMICOLON: return 0x27;
    case IKEY_QUOTE: return 0x28;
    case IKEY_RETURN: return 0x1C;
    case IKEY_LSHIFT: return 0x2A;
    case IKEY_z: return 0x2C;
    case IKEY_x: return 0x2D;
    case IKEY_c: return 0x2E;
    case IKEY_v: return 0x2F;
    case IKEY_b: return 0x30;
    case IKEY_n: return 0x31;
    case IKEY_m: return 0x32;
    case IKEY_COMMA: return 0x33;
    case IKEY_PERIOD: return 0x34;
    case IKEY_SLASH: return 0x35;
    case IKEY_RSHIFT: return 0x36;
    case IKEY_LCTRL: return 0x1D;
    case IKEY_LSUPER: return 0x5B;
    case IKEY_LALT: return 0x38;
    case IKEY_SPACE: return 0x39;
    case IKEY_RALT: return 0x38;
    case IKEY_RSUPER: return 0x5C;
    case IKEY_MENU: return 0x5D;
    case IKEY_RCTRL: return 0x1D;
    case IKEY_SYSREQ: return 0x37;
    case IKEY_SCROLLOCK: return 0x46;
    case IKEY_BREAK: return 0xC5;
    case IKEY_INSERT: return 0x52;
    case IKEY_HOME: return 0x47;
    case IKEY_PAGEUP: return 0x49;
    case IKEY_DELETE: return 0x53;
    case IKEY_END: return 0x4F;
    case IKEY_PAGEDOWN: return 0x51;
    case IKEY_NUMLOCK: return 0x45;
    case IKEY_KP_DIVIDE: return 0x35;
    case IKEY_KP_MULTIPLY: return 0x37;
    case IKEY_KP_MINUS: return 0x4A;
    case IKEY_KP7: return 0x47;
    case IKEY_KP8: return 0x48;
    case IKEY_KP9: return 0x49;
    case IKEY_KP4: return 0x4B;
    case IKEY_KP5: return 0x4C;
    case IKEY_KP6: return 0x4D;
    case IKEY_KP_PLUS: return 0x4E;
    case IKEY_KP1: return 0x4F;
    case IKEY_KP2: return 0x50;
    case IKEY_KP3: return 0x51;
    case IKEY_KP_ENTER: return 0x1C;
    case IKEY_KP0: return 0x52;
    case IKEY_KP_PERIOD: return 0x53;
    case IKEY_UP: return 0x48;
    case IKEY_LEFT: return 0x4B;
    case IKEY_DOWN: return 0x50;
    case IKEY_RIGHT: return 0x4D;

    // All others are currently undefined; returns 0
    default: return 0;
  }
}

static enum keycode convert_xt_internal(Uint32 key, enum keycode *second)
{
  *second = IKEY_UNKNOWN;
  switch(key)
  {
    case 0x01: return IKEY_ESCAPE;
    case 0x3B: return IKEY_F1;
    case 0x3C: return IKEY_F2;
    case 0x3D: return IKEY_F3;
    case 0x3E: return IKEY_F4;
    case 0x3F: return IKEY_F5;
    case 0x40: return IKEY_F6;
    case 0x41: return IKEY_F7;
    case 0x42: return IKEY_F8;
    case 0x43: return IKEY_F9;
    case 0x44: return IKEY_F10;
    case 0x57: return IKEY_F11;
    case 0x58: return IKEY_F12;
    case 0x29: return IKEY_BACKQUOTE;
    case 0x02: return IKEY_1;
    case 0x03: return IKEY_2;
    case 0x04: return IKEY_3;
    case 0x05: return IKEY_4;
    case 0x06: return IKEY_5;
    case 0x07: return IKEY_6;
    case 0x08: return IKEY_7;
    case 0x09: return IKEY_8;
    case 0x0A: return IKEY_9;
    case 0x0B: return IKEY_0;
    case 0x0C: return IKEY_MINUS;
    case 0x0D: return IKEY_EQUALS;
    case 0x2B: return IKEY_BACKSLASH;
    case 0x0E: return IKEY_BACKSPACE;
    case 0x0F: return IKEY_TAB;
    case 0x10: return IKEY_q;
    case 0x11: return IKEY_w;
    case 0x12: return IKEY_e;
    case 0x13: return IKEY_r;
    case 0x14: return IKEY_t;
    case 0x15: return IKEY_y;
    case 0x16: return IKEY_u;
    case 0x17: return IKEY_i;
    case 0x18: return IKEY_o;
    case 0x19: return IKEY_p;
    case 0x1A: return IKEY_LEFTBRACKET;
    case 0x1B: return IKEY_RIGHTBRACKET;
    case 0x3A: return IKEY_CAPSLOCK;
    case 0x1E: return IKEY_a;
    case 0x1F: return IKEY_s;
    case 0x20: return IKEY_d;
    case 0x21: return IKEY_f;
    case 0x22: return IKEY_g;
    case 0x23: return IKEY_h;
    case 0x24: return IKEY_j;
    case 0x25: return IKEY_k;
    case 0x26: return IKEY_l;
    case 0x27: return IKEY_SEMICOLON;
    case 0x28: return IKEY_QUOTE;
    case 0x1C:
      *second = IKEY_KP_ENTER;
      return IKEY_RETURN;
    case 0x2A: return IKEY_LSHIFT;
    case 0x2C: return IKEY_z;
    case 0x2D: return IKEY_x;
    case 0x2E: return IKEY_c;
    case 0x2F: return IKEY_v;
    case 0x30: return IKEY_b;
    case 0x31: return IKEY_n;
    case 0x32: return IKEY_m;
    case 0x33: return IKEY_COMMA;
    case 0x34: return IKEY_PERIOD;
    case 0x35:
      *second = IKEY_KP_DIVIDE;
      return IKEY_SLASH;
    case 0x36: return IKEY_RSHIFT;
    case 0x1D:
      *second = IKEY_RCTRL;
      return IKEY_LCTRL;
    case 0x5B: return IKEY_LSUPER;
    case 0x38:
      *second = IKEY_RALT;
      return IKEY_LALT;
    case 0x39: return IKEY_SPACE;
    case 0x5C: return IKEY_RSUPER;
    case 0x5D: return IKEY_MENU;
    case 0x37:
      *second = IKEY_KP_MULTIPLY;
      return IKEY_SYSREQ;
    case 0x46: return IKEY_SCROLLOCK;
    case 0xC5: return IKEY_BREAK;
    case 0x52:
      *second = IKEY_KP0;
      return IKEY_INSERT;
    case 0x47:
      *second = IKEY_KP7;
      return IKEY_HOME;
    case 0x49:
      *second = IKEY_KP9;
      return IKEY_PAGEUP;
    case 0x53:
      *second = IKEY_KP_PERIOD;
      return IKEY_DELETE;
    case 0x4F:
      *second = IKEY_KP1;
      return IKEY_END;
    case 0x51:
      *second = IKEY_KP3;
      return IKEY_PAGEDOWN;
    case 0x45: return IKEY_NUMLOCK;
    case 0x4A: return IKEY_KP_MINUS;
    case 0x48:
      *second = IKEY_UP;
      return IKEY_KP8;
    case 0x4B:
      *second = IKEY_LEFT;
       return IKEY_KP4;
    case 0x4C: return IKEY_KP5;
    case 0x4D:
      *second = IKEY_RIGHT;
      return IKEY_KP6;
    case 0x4E: return IKEY_KP_PLUS;
    case 0x50:
      *second = IKEY_DOWN;
      return IKEY_KP2;
    default: return IKEY_UNKNOWN;
  }
}

static bool update_autorepeat(void)
{
  struct buffered_status *status = store_status();
  bool rval = false;

  // Repeat code
  Uint8 last_key_state = status->keymap[status->key_repeat];
  Uint8 last_mouse_state = status->mouse_repeat_state;

  if(last_key_state)
  {
    Uint32 new_time = get_ticks();
    Uint32 ms_difference = new_time - status->keypress_time;

    if(last_key_state == 1)
    {
      if(ms_difference > KEY_REPEAT_START)
      {
        status->keypress_time = new_time;
        status->keymap[status->key_repeat] = 2;
        status->key = status->key_repeat;
        status->unicode = status->unicode_repeat;
        rval = true;
      }
    }
    else
    {
      if(ms_difference > KEY_REPEAT_RATE)
      {
        status->keypress_time = new_time;
        status->key = status->key_repeat;
        status->unicode = status->unicode_repeat;
        rval = true;
      }
    }
  }
  else

  if(input.repeat_stack_pointer)
  {
    // Take repeat off the stack.
    input.repeat_stack_pointer--;
    status->key_repeat =
     input.key_repeat_stack[input.repeat_stack_pointer];
    status->unicode_repeat =
     input.unicode_repeat_stack[input.repeat_stack_pointer];

    status->keypress_time = get_ticks();
  }

  if(last_mouse_state)
  {
    Uint32 new_time = get_ticks();
    Uint32 ms_difference = new_time - status->mouse_time;

    if(status->mouse_drag_state == -1)
      status->mouse_drag_state = 0;
    else
      status->mouse_drag_state = 1;

    if(last_mouse_state == 1)
    {
      if(ms_difference > MOUSE_REPEAT_START)
      {
        status->mouse_time = new_time;
        status->mouse_repeat_state = 2;
        status->mouse_button = status->mouse_repeat;
        rval = true;
      }
    }
    else
    {
      if(ms_difference > MOUSE_REPEAT_RATE)
      {
        status->mouse_time = new_time;
        status->mouse_button = status->mouse_repeat;
        rval = true;
      }
    }
  }

  bump_status();
  return rval;
}

bool update_event_status(void)
{
  struct buffered_status *status = store_status();
  bool rval;

  status->key = IKEY_UNKNOWN;
  status->unicode = 0;
  status->mouse_moved = 0;
  status->mouse_button = 0;

  rval  = __update_event_status();
  rval |= update_autorepeat();

  return rval;
}

void wait_event(void)
{
  struct buffered_status *status = store_status();

  status->key = IKEY_UNKNOWN;
  status->unicode = 0;
  status->mouse_moved = 0;
  status->mouse_button = 0;

  __wait_event();
  update_autorepeat();
}

Uint32 update_event_status_delay(void)
{
  int rval = update_event_status();
  int delay_ticks;

  if(!last_update_time)
    last_update_time = get_ticks();

  delay_ticks = UPDATE_DELAY - (get_ticks() - last_update_time);

  last_update_time = get_ticks();

  if(delay_ticks < 0)
    delay_ticks = 0;

  delay(delay_ticks);
  return rval;
}

static enum keycode emit_keysym_wrt_numlock(enum keycode key)
{
  const struct buffered_status *status = load_status();

  if(status->numlock_status)
  {
    switch(key)
    {
      case IKEY_KP0:         return IKEY_0;
      case IKEY_KP1:         return IKEY_1;
      case IKEY_KP2:         return IKEY_2;
      case IKEY_KP3:         return IKEY_3;
      case IKEY_KP4:         return IKEY_4;
      case IKEY_KP5:         return IKEY_5;
      case IKEY_KP6:         return IKEY_6;
      case IKEY_KP7:         return IKEY_7;
      case IKEY_KP8:         return IKEY_8;
      case IKEY_KP9:         return IKEY_9;
      case IKEY_KP_PERIOD:   return IKEY_PERIOD;
      case IKEY_KP_ENTER:    return IKEY_RETURN;
      default: break;
    }
  }
  else
  {
    switch(key)
    {
      case IKEY_KP0:         return IKEY_INSERT;
      case IKEY_KP1:         return IKEY_END;
      case IKEY_KP2:         return IKEY_DOWN;
      case IKEY_KP3:         return IKEY_PAGEDOWN;
      case IKEY_KP4:         return IKEY_LEFT;
      case IKEY_KP5:         return IKEY_SPACE; // kinda arbitrary
      case IKEY_KP6:         return IKEY_RIGHT;
      case IKEY_KP7:         return IKEY_HOME;
      case IKEY_KP8:         return IKEY_UP;
      case IKEY_KP9:         return IKEY_PAGEUP;
      case IKEY_KP_PERIOD:   return IKEY_DELETE;
      case IKEY_KP_ENTER:    return IKEY_RETURN;
      default: break;
    }
  }
  return key;
}

Uint32 get_key(enum keycode_type type)
{
  const struct buffered_status *status = load_status();

  switch(type)
  {
    case keycode_pc_xt:
      return convert_internal_xt(status->key);

    case keycode_internal:
      return emit_keysym_wrt_numlock(status->key);

    default:
      return status->unicode;
  }
}

Uint32 get_key_status(enum keycode_type type, Uint32 index)
{
  const struct buffered_status *status = load_status();

  switch(type)
  {
    case keycode_pc_xt:
    {
      enum keycode first, second;
      first = convert_xt_internal(index, &second);
      return (status->keymap[first] || status->keymap[second]);
    }

    case keycode_internal:
      return status->keymap[index];

    default:
      return 0;
  }
}

Uint32 get_last_key(enum keycode_type type)
{
  const struct buffered_status *status = load_status();

  switch(type)
  {
    case keycode_pc_xt:
      return convert_internal_xt(status->key_pressed);

    case keycode_internal:
      return status->key_pressed;

    default:
      return 0;
  }
}

Uint32 get_last_key_released(enum keycode_type type)
{
  const struct buffered_status *status = load_status();

  switch(type)
  {
    case keycode_pc_xt:
      return convert_internal_xt(status->key_release);

    case keycode_internal:
      return status->key_release;

    default:
      return 0;
  }
}

void get_mouse_position(int *x, int *y)
{
  const struct buffered_status *status = load_status();
  *x = status->mouse_x;
  *y = status->mouse_y;
}

void get_real_mouse_position(int *x, int *y)
{
  const struct buffered_status *status = load_status();
  *x = status->real_mouse_x;
  *y = status->real_mouse_y;
}

Uint32 get_mouse_x(void)
{
  const struct buffered_status *status = load_status();
  return status->mouse_x;
}

Uint32 get_mouse_y(void)
{
  const struct buffered_status *status = load_status();
  return status->mouse_y;
}

Uint32 get_real_mouse_x(void)
{
  const struct buffered_status *status = load_status();
  return status->real_mouse_x;
}

Uint32 get_real_mouse_y(void)
{
  const struct buffered_status *status = load_status();
  return status->real_mouse_y;
}

Uint32 get_mouse_drag(void)
{
  const struct buffered_status *status = load_status();
  return status->mouse_drag_state;
}

Uint32 get_mouse_press(void)
{
  const struct buffered_status *status = load_status();

  if(status->mouse_button <= MOUSE_BUTTON_RIGHT)
    return status->mouse_button;

  return 0;
}

Uint32 get_mouse_press_ext(void)
{
  const struct buffered_status *status = load_status();
  return status->mouse_button;
}

Uint32 get_mouse_status(void)
{
  const struct buffered_status *status = load_status();
  return status->mouse_button_state;
}

void warp_mouse(Uint32 x, Uint32 y)
{
  int mx_real, my_real, mx = (x * 8) + 4, my = (y * 14) + 7;
  struct buffered_status *status = store_status();

  status->mouse_x = x;
  status->mouse_y = y;
  status->real_mouse_x = mx;
  status->real_mouse_y = my;

  set_screen_coords(mx, my, &mx_real, &my_real);
  real_warp_mouse(mx_real, my_real);
}

void warp_mouse_x(Uint32 x)
{
  struct buffered_status *status = store_status();
  int mx_real, my_real, mx = (x * 8) + 4;

  status->mouse_x = x;
  status->real_mouse_x = mx;

  set_screen_coords(mx, status->real_mouse_y, &mx_real, &my_real);
  real_warp_mouse(mx_real, my_real);
}

void warp_mouse_y(Uint32 y)
{
  struct buffered_status *status = store_status();
  int mx_real, my_real, my = (y * 14) + 7;

  status->mouse_y = y;
  status->real_mouse_y = my;

  set_screen_coords(status->real_mouse_x, my, &mx_real, &my_real);
  real_warp_mouse(mx_real, my_real);
}

void warp_real_mouse_x(Uint32 mx)
{
  struct buffered_status *status = store_status();
  int mx_real, my_real, x = mx / 8;

  status->mouse_x = x;
  status->real_mouse_x = mx;

  set_screen_coords(mx, status->real_mouse_y, &mx_real, &my_real);
  real_warp_mouse(mx_real, my_real);
}

void warp_real_mouse_y(Uint32 my)
{
  struct buffered_status *status = store_status();
  int mx_real, my_real, y = my / 14;

  status->mouse_y = y;
  status->real_mouse_y = my;

  set_screen_coords(status->real_mouse_x, my, &mx_real, &my_real);
  real_warp_mouse(mx_real, my_real);
}

void force_last_key(enum keycode_type type, int val)
{
  struct buffered_status *status = store_status();

  switch(type)
  {
    case keycode_pc_xt:
    {
      enum keycode second;
      status->key_pressed = convert_xt_internal(val, &second);
      break;
    }

    case keycode_internal:
    {
      status->key_pressed = (enum keycode)val;
      break;
    }

    default:
      break;
  }
}

bool get_alt_status(enum keycode_type type)
{
  return get_key_status(type, IKEY_LALT) ||
         get_key_status(type, IKEY_RALT);
}

bool get_shift_status(enum keycode_type type)
{
  return get_key_status(type, IKEY_LSHIFT) ||
         get_key_status(type, IKEY_RSHIFT);
}

bool get_ctrl_status(enum keycode_type type)
{
  return get_key_status(type, IKEY_LCTRL) ||
         get_key_status(type, IKEY_RCTRL);
}

void map_joystick_axis(int joystick, int axis, enum keycode min_key,
 enum keycode max_key)
{
  input.joystick_axis_map[joystick][axis][0] = min_key;
  input.joystick_axis_map[joystick][axis][1] = max_key;
}

void map_joystick_button(int joystick, int button, enum keycode key)
{
  input.joystick_button_map[joystick][button] = key;
}

void set_unfocus_pause(bool value)
{
  input.unfocus_pause = value;
}

void set_num_buffered_events(Uint8 value)
{
  num_buffered_events = MAX(1, value);
}

void key_press(struct buffered_status *status, enum keycode key,
 Uint16 unicode_key)
{
  status->keymap[key] = 1;
  status->key_pressed = key;
  status->key = key;
  status->unicode = unicode_key;
  status->key_repeat = key;
  status->unicode_repeat = unicode_key;
  status->keypress_time = get_ticks();
  status->key_release = IKEY_UNKNOWN;
}

void key_release(struct buffered_status *status, enum keycode key)
{
  status->keymap[key] = 0;
  status->key_repeat = IKEY_UNKNOWN;
  status->unicode_repeat = 0;
  status->key_release = key;
}

void wait_for_key_release(Uint32 index)
{
  while(get_key_status(keycode_internal, index) >= 1)
    update_event_status_delay();
}

void wait_for_mouse_release(Uint32 mouse_button)
{
  while(get_mouse_status())
    update_event_status_delay();
}
