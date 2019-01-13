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
#include "../../src/util.h"

#include <kos.h>

#include "event.h"

extern struct input_status input;

void initialize_joysticks(void)
{
}

void real_warp_mouse(int x, int y)
{
}

boolean dc_update_input(void);

boolean __update_event_status(void)
{
  boolean retval = false;
  retval |= dc_update_input();
  return retval;
}

void __wait_event(int timeout)
{
  if(timeout)
    delay(timeout);

  while(!__update_event_status())
    vid_waitvbl();
}

// Taken from arch/nds/event.c
// Send a key up/down event to MZX.
void do_unicode_key_event(struct buffered_status *status, boolean down,
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

void do_key_event(struct buffered_status *status, boolean down,
 enum keycode code)
{
  do_unicode_key_event(status, down, code,
   (code >= 32 && code <= 126) ? code : 0);
}

// Send a joystick button up/down event to MZX.
void do_joybutton_event(struct buffered_status *status, boolean down,
 int button)
{
  // Look up the keycode for this joystick button.
  enum keycode stuffed_key = input.joystick_button_map[0][button];
  do_key_event(status, down, stuffed_key);
}

static inline boolean check_key(struct buffered_status *status,
 Uint32 down, Uint32 up, Uint32 key, enum keycode code)
{
  if(down & key)
  {
    do_key_event(status, true, code);
    return true;
  }
  else

  if(up & key)
  {
    do_key_event(status, false, code);
    return true;
  }

  else
  {
    return false;
  }
}

static inline boolean check_joy(struct buffered_status *status,
  Uint32 down, Uint32 up, Uint32 key, Uint32 code)
{
  if(down & key)
  {
    do_joybutton_event(status, true, code);
    return true;
  }
  else

  if(up & key)
  {
    do_joybutton_event(status, false, code);
    return true;
  }

  else
  {
    return false;
  }
}

static Uint32 last_buttons;
static Uint32 last_ltrig, last_rtrig;

boolean dc_update_input(void)
{
  struct buffered_status *status = store_status();
  maple_device_t *maple_pad, *maple_kbd;
  cont_state_t *pad;
  Uint32 down, held, up;
  boolean retval = false;

  maple_pad = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
  if (maple_pad && (pad = (cont_state_t*) maple_dev_status(maple_pad))) {
    down = (pad->buttons ^ last_buttons) & pad->buttons;
    held = (pad->buttons & last_buttons);
    up = (pad->buttons ^ last_buttons) & last_buttons;

    retval |= check_key(status, down, up, CONT_DPAD_UP, IKEY_UP);
    retval |= check_key(status, down, up, CONT_DPAD_DOWN, IKEY_DOWN);
    retval |= check_key(status, down, up, CONT_DPAD_LEFT, IKEY_LEFT);
    retval |= check_key(status, down, up, CONT_DPAD_RIGHT, IKEY_RIGHT);
    retval |= check_joy(status, down, up, CONT_A, 0);
    retval |= check_joy(status, down, up, CONT_B, 1);
    retval |= check_joy(status, down, up, CONT_X, 2);
    retval |= check_joy(status, down, up, CONT_Y, 3);
    retval |= check_joy(status, down, up, CONT_START, 7);
    retval |= ((last_ltrig ^ pad->ltrig) >= 0x80) || ((last_rtrig ^ pad->rtrig) >= 0x80);

    if (last_ltrig >= 0x80 && pad->ltrig < 0x80)
        do_joybutton_event(status, false, 4);
    if (last_ltrig < 0x80 && pad->ltrig >= 0x80)
        do_joybutton_event(status, true, 4);
    if (last_rtrig >= 0x80 && pad->rtrig < 0x80)
        do_joybutton_event(status, false, 5);
    if (last_rtrig < 0x80 && pad->rtrig >= 0x80)
        do_joybutton_event(status, true, 5);

    last_buttons = pad->buttons;
    last_ltrig = pad->ltrig;
    last_rtrig = pad->rtrig;
  } else {
    last_buttons = last_ltrig = last_rtrig = 0;
  }

  return retval;
}
