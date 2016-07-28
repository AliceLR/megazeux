/* MegaZeux
 *
 * Copyright (C) 2016 Adrian Siekierka <asiekierka@gmail.com>
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

#include "../../src/event.h"
#include "../../src/platform.h"
#include "../../src/graphics.h"

#include <3ds.h>

extern struct input_status input;
bool touch_held;

bool update_hid(void);

bool __update_event_status(void)
{
  bool retval = false;
  retval |= aptMainLoop();
  retval |= update_hid();
  return retval;
}

void __wait_event(void)
{
  gspWaitForVBlank();
}

// Taken from arch/nds/event.c
// Send a key up/down event to MZX.
static void do_unicode_key_event(struct buffered_status *status, bool down,
 enum keycode code, int unicode)
{
  if(down)
  {
    key_press(status, code, unicode);
  }
  else
  {
    status->keymap[code] = 0;
    if(status->key_repeat == code)
    {
      status->key_repeat = IKEY_UNKNOWN;
      status->unicode_repeat = 0;
    }
    status->key_release = code;
  }
}

static void do_key_event(struct buffered_status *status, bool down,
 enum keycode code)
{
  do_unicode_key_event(status, down, code, code);
}

// Send a joystick button up/down event to MZX.
static void do_joybutton_event(struct buffered_status *status, bool down,
 int button)
{
  // Look up the keycode for this joystick button.
  enum keycode stuffed_key = input.joystick_button_map[0][button];
  do_key_event(status, down, stuffed_key);
}

static inline bool check_key(struct buffered_status *status,
  Uint32 down, Uint32 up, Uint32 key, enum keycode code)
{
  if(down & key)
  {
    do_key_event(status, true, code);
    return true;
  }
  else if(up & key)
  {
    do_key_event(status, false, code);
    return true;
  }
  else
    return false;
}

static inline bool check_joy(struct buffered_status *status,
  Uint32 down, Uint32 up, Uint32 key, Uint32 code)
{
  if(down & key)
  {
    do_joybutton_event(status, true, code);
    return true;
  }
  else if(up & key)
  {
    do_joybutton_event(status, false, code);    
    return true;
  }
  else
    return false;
}

bool ctr_update_touch(struct buffered_status *status)
{
  touchPosition touch;
  int mx, my;

  hidTouchRead(&touch);
  mx = touch.px * 2;
  my = touch.py * 2 - 32;
  if(my < 0) my = 0;
  if(my > 350) my = 350;

  if(mx != status->real_mouse_x || my != status->real_mouse_y)
  {
    status->real_mouse_x = mx;
    status->real_mouse_y = my;
    status->mouse_x = mx / 8;
    status->mouse_y = my / 14;
    status->mouse_moved = true;

    focus_pixel(mx, my);

    return true;
  }
  else
    return false;
}

bool update_hid(void)
{
  struct buffered_status *status = store_status();
  Uint32 down, up;
  bool retval = false;

  hidScanInput();
  down = hidKeysDown();
  up = hidKeysUp();

  retval |= check_key(status, down, up, KEY_UP, IKEY_UP);
  retval |= check_key(status, down, up, KEY_DOWN, IKEY_DOWN);
  retval |= check_key(status, down, up, KEY_LEFT, IKEY_LEFT);
  retval |= check_key(status, down, up, KEY_RIGHT, IKEY_RIGHT);
  retval |= check_joy(status, down, up, KEY_A, 0);
  retval |= check_joy(status, down, up, KEY_B, 1);
  retval |= check_joy(status, down, up, KEY_X, 2);
  retval |= check_joy(status, down, up, KEY_Y, 3);
  retval |= check_joy(status, down, up, KEY_L, 4);
  retval |= check_joy(status, down, up, KEY_SELECT, 6);
  retval |= check_joy(status, down, up, KEY_START, 7);
  retval |= check_joy(status, down, up, KEY_ZL, 8);
  retval |= check_joy(status, down, up, KEY_ZR, 9);

  if(down & KEY_TOUCH)
  {
    status->mouse_button = MOUSE_BUTTON_LEFT;
    status->mouse_repeat = MOUSE_BUTTON_LEFT;
    status->mouse_button_state |= MOUSE_BUTTON(MOUSE_BUTTON_LEFT);
    status->mouse_repeat_state = 1;
    status->mouse_drag_state = -1;
    status->mouse_time = get_ticks();
    touch_held = true;
    retval = true;
  }
  else if(up & KEY_TOUCH)
  {
    status->mouse_button_state &= ~MOUSE_BUTTON(MOUSE_BUTTON_LEFT);
    status->mouse_repeat = 0;
    status->mouse_repeat_state = 0;
    status->mouse_drag_state = -0;
    touch_held = false;
    retval = true;
  }

  if(touch_held)
  {
    retval |= ctr_update_touch(status);
  }

  return retval;
}
