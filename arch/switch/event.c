/* MegaZeux
 *
 * Copyright (C) 2016, 2018 Adrian Siekierka <asiekierka@gmail.com>
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

#include <switch.h>

#include "event.h"

extern struct input_status input;
static bool allow_focus_changes = true;
static bool is_dragging = false;

bool update_hid(void);

bool __update_event_status(void)
{
  bool retval = false;
  retval |= appletMainLoop();
  retval |= update_hid();
  return retval;
}

void __wait_event(int timeout)
{
  if (timeout) delay(timeout);
  while (!__update_event_status())
    gfxWaitForVsync();
}

// Taken from arch/nds/event.c
// Send a key up/down event to MZX.
void do_unicode_key_event(struct buffered_status *status, bool down,
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

void do_key_event(struct buffered_status *status, bool down,
 enum keycode code)
{
  do_unicode_key_event(status, down, code, code >= 32 && code <= 126 ? code : 0);
}

// Send a joystick button up/down event to MZX.
void do_joybutton_event(struct buffered_status *status, bool down,
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
static inline bool nx_update_joystick(struct buffered_status *status)
{
  JoystickPosition pos;
  int dmx, dmy, nmx, nmy;

  hidJoystickRead(&pos, CONTROLLER_P1_AUTO, JOYSTICK_RIGHT);
  dmx = pos.dx / 3;
  dmy = pos.dy / 3;

  if(dmx != 0 || dmy != 0)
  {
    nmx = status->real_mouse_x + dmx;
    nmy = status->real_mouse_y + dmy;
    if(nmx < 0) nmx = 0;
    if(nmx >= 640) nmx = 639;
    if(nmy < 0) nmy = 0;
    if(nmy >= 350) nmy = 349;

    if((Uint32) nmx != status->real_mouse_x || (Uint32) nmy != status->real_mouse_y)
    {
      status->real_mouse_x = nmx;
      status->real_mouse_y = nmy;
      status->mouse_x = nmx / 8;
      status->mouse_y = nmy / 14;
      status->mouse_moved = true;

      focus_pixel(nmx, nmy);

      return true;
    }
  }

  return false;
}

static inline bool nx_update_touch(struct buffered_status *status, touchPosition *touch)
{
  int mx, my;

  mx = touch->px / 2;
  my = (touch->py / 2) - 10;
  if(mx < 0) mx = 0;
  if(mx >= 640) mx = 639;
  if(my < 0) my = 0;
  if(my >= 350) my = 349;

  if((Uint32) mx != status->real_mouse_x || (Uint32) my != status->real_mouse_y)
  {
    status->real_mouse_x = mx;
    status->real_mouse_y = my;
    status->mouse_x = mx / 8;
    status->mouse_y = my / 14;
    status->mouse_moved = true;

    allow_focus_changes = true;
    focus_pixel(mx, my);
    allow_focus_changes = false;

    return true;
  }
  else
    return false;
}

bool check_kbd_modifier(struct buffered_status *status, HidKeyboardModifier modifier, Uint32 code)
{
  if(hidKeyboardModifierDown(modifier))
  {
    do_key_event(status, true, code);
    return true;
  }
  else if(hidKeyboardModifierUp(modifier))
  {
    do_key_event(status, false, code);
    return true;
  }
  else
    return false;
}

bool check_kbd(struct buffered_status *status, HidKeyboardScancode scancode, Uint32 code)
{
  if(hidKeyboardDown(scancode))
  {
    do_key_event(status, true, code);
    return true;
  }
  else if(hidKeyboardUp(scancode))
  {
    do_key_event(status, false, code);
    return true;
  }
  else
    return false;
}

bool update_hid(void)
{
  struct buffered_status *status = store_status();
  Uint32 down, held, up;
  bool retval = false;
  touchPosition touch;

  hidScanInput();
  down = hidKeysDown(CONTROLLER_P1_AUTO);
  held = hidKeysHeld(CONTROLLER_P1_AUTO);
  up = hidKeysUp(CONTROLLER_P1_AUTO);

  retval |= check_kbd_modifier(status, KBD_MOD_LCTRL, IKEY_LCTRL);
  retval |= check_kbd_modifier(status, KBD_MOD_LSHIFT, IKEY_LSHIFT);
  retval |= check_kbd_modifier(status, KBD_MOD_LALT, IKEY_LALT);
  retval |= check_kbd_modifier(status, KBD_MOD_LMETA, IKEY_LSUPER);
  retval |= check_kbd_modifier(status, KBD_MOD_RCTRL, IKEY_RCTRL);
  retval |= check_kbd_modifier(status, KBD_MOD_RSHIFT, IKEY_RSHIFT);
  retval |= check_kbd_modifier(status, KBD_MOD_RALT, IKEY_RALT);
  retval |= check_kbd_modifier(status, KBD_MOD_RMETA, IKEY_RSUPER);

  retval |= check_kbd(status, KBD_A, IKEY_a);
  retval |= check_kbd(status, KBD_B, IKEY_b);
  retval |= check_kbd(status, KBD_C, IKEY_c);
  retval |= check_kbd(status, KBD_D, IKEY_d);
  retval |= check_kbd(status, KBD_E, IKEY_e);
  retval |= check_kbd(status, KBD_F, IKEY_f);
  retval |= check_kbd(status, KBD_G, IKEY_g);
  retval |= check_kbd(status, KBD_H, IKEY_h);
  retval |= check_kbd(status, KBD_I, IKEY_i);
  retval |= check_kbd(status, KBD_J, IKEY_j);
  retval |= check_kbd(status, KBD_K, IKEY_k);
  retval |= check_kbd(status, KBD_L, IKEY_l);
  retval |= check_kbd(status, KBD_M, IKEY_m);
  retval |= check_kbd(status, KBD_N, IKEY_n);
  retval |= check_kbd(status, KBD_O, IKEY_o);
  retval |= check_kbd(status, KBD_P, IKEY_p);
  retval |= check_kbd(status, KBD_Q, IKEY_q);
  retval |= check_kbd(status, KBD_R, IKEY_r);
  retval |= check_kbd(status, KBD_S, IKEY_s);
  retval |= check_kbd(status, KBD_T, IKEY_t);
  retval |= check_kbd(status, KBD_U, IKEY_u);
  retval |= check_kbd(status, KBD_V, IKEY_v);
  retval |= check_kbd(status, KBD_W, IKEY_w);
  retval |= check_kbd(status, KBD_X, IKEY_x);
  retval |= check_kbd(status, KBD_Y, IKEY_y);
  retval |= check_kbd(status, KBD_Z, IKEY_z);

  retval |= check_kbd(status, KBD_1, IKEY_1);
  retval |= check_kbd(status, KBD_2, IKEY_2);
  retval |= check_kbd(status, KBD_3, IKEY_3);
  retval |= check_kbd(status, KBD_4, IKEY_4);
  retval |= check_kbd(status, KBD_5, IKEY_5);
  retval |= check_kbd(status, KBD_6, IKEY_6);
  retval |= check_kbd(status, KBD_7, IKEY_7);
  retval |= check_kbd(status, KBD_8, IKEY_8);
  retval |= check_kbd(status, KBD_9, IKEY_9);
  retval |= check_kbd(status, KBD_0, IKEY_0);

  retval |= check_kbd(status, KBD_ENTER, IKEY_RETURN);
  retval |= check_kbd(status, KBD_ESC, IKEY_ESCAPE);
  retval |= check_kbd(status, KBD_BACKSPACE, IKEY_BACKSPACE);
  retval |= check_kbd(status, KBD_TAB, IKEY_TAB);
  retval |= check_kbd(status, KBD_SPACE, IKEY_SPACE);
  retval |= check_kbd(status, KBD_MINUS, IKEY_MINUS);
  retval |= check_kbd(status, KBD_EQUAL, IKEY_EQUALS);
  retval |= check_kbd(status, KBD_LEFTBRACE, IKEY_LEFTBRACKET);
  retval |= check_kbd(status, KBD_RIGHTBRACE, IKEY_RIGHTBRACKET);
  retval |= check_kbd(status, KBD_BACKSLASH, IKEY_BACKSLASH);
//  retval |= check_kbd(status, KBD_HASHTILDE, ); ??
  retval |= check_kbd(status, KBD_SEMICOLON, IKEY_SEMICOLON);
  retval |= check_kbd(status, KBD_APOSTROPHE, IKEY_QUOTE);
  retval |= check_kbd(status, KBD_GRAVE, IKEY_BACKQUOTE);
  retval |= check_kbd(status, KBD_COMMA, IKEY_COMMA);
  retval |= check_kbd(status, KBD_DOT, IKEY_PERIOD);
  retval |= check_kbd(status, KBD_SLASH, IKEY_SLASH);
  retval |= check_kbd(status, KBD_CAPSLOCK, IKEY_CAPSLOCK);

  retval |= check_kbd(status, KBD_F1, IKEY_F1);
  retval |= check_kbd(status, KBD_F2, IKEY_F2);
  retval |= check_kbd(status, KBD_F3, IKEY_F3);
  retval |= check_kbd(status, KBD_F4, IKEY_F4);
  retval |= check_kbd(status, KBD_F5, IKEY_F5);
  retval |= check_kbd(status, KBD_F6, IKEY_F6);
  retval |= check_kbd(status, KBD_F7, IKEY_F7);
  retval |= check_kbd(status, KBD_F8, IKEY_F8);
  retval |= check_kbd(status, KBD_F9, IKEY_F9);
  retval |= check_kbd(status, KBD_F10, IKEY_F10);
  retval |= check_kbd(status, KBD_F11, IKEY_F11);
  retval |= check_kbd(status, KBD_F12, IKEY_F12);

  retval |= check_kbd(status, KBD_SYSRQ, IKEY_SYSREQ);
  retval |= check_kbd(status, KBD_SCROLLLOCK, IKEY_SCROLLOCK);
  retval |= check_kbd(status, KBD_PAUSE, IKEY_BREAK);
  retval |= check_kbd(status, KBD_INSERT, IKEY_INSERT);
  retval |= check_kbd(status, KBD_HOME, IKEY_HOME);
  retval |= check_kbd(status, KBD_PAGEUP, IKEY_PAGEUP);
  retval |= check_kbd(status, KBD_DELETE, IKEY_DELETE);
  retval |= check_kbd(status, KBD_END, IKEY_END);
  retval |= check_kbd(status, KBD_PAGEDOWN, IKEY_PAGEDOWN);
  retval |= check_kbd(status, KBD_RIGHT, IKEY_RIGHT);
  retval |= check_kbd(status, KBD_LEFT, IKEY_LEFT);
  retval |= check_kbd(status, KBD_DOWN, IKEY_DOWN);
  retval |= check_kbd(status, KBD_UP, IKEY_UP);

  retval |= check_kbd(status, KBD_NUMLOCK, IKEY_NUMLOCK);
  retval |= check_kbd(status, KBD_KPSLASH, IKEY_KP_DIVIDE);
  retval |= check_kbd(status, KBD_KPASTERISK, IKEY_KP_MULTIPLY);
  retval |= check_kbd(status, KBD_KPMINUS, IKEY_KP_MINUS);
  retval |= check_kbd(status, KBD_KPPLUS, IKEY_KP_PLUS);
  retval |= check_kbd(status, KBD_KPENTER, IKEY_KP_ENTER);
  retval |= check_kbd(status, KBD_KP1, IKEY_KP1);
  retval |= check_kbd(status, KBD_KP2, IKEY_KP2);
  retval |= check_kbd(status, KBD_KP3, IKEY_KP3);
  retval |= check_kbd(status, KBD_KP4, IKEY_KP4);
  retval |= check_kbd(status, KBD_KP5, IKEY_KP5);
  retval |= check_kbd(status, KBD_KP6, IKEY_KP6);
  retval |= check_kbd(status, KBD_KP7, IKEY_KP7);
  retval |= check_kbd(status, KBD_KP8, IKEY_KP8);
  retval |= check_kbd(status, KBD_KP9, IKEY_KP9);
  retval |= check_kbd(status, KBD_KP0, IKEY_KP0);
  retval |= check_kbd(status, KBD_KPDOT, IKEY_KP_PERIOD);

  retval |= check_key(status, down, up, KEY_UP, IKEY_UP);
  retval |= check_key(status, down, up, KEY_DOWN, IKEY_DOWN);
  retval |= check_key(status, down, up, KEY_LEFT, IKEY_LEFT);
  retval |= check_key(status, down, up, KEY_RIGHT, IKEY_RIGHT);
  retval |= check_joy(status, down, up, KEY_A, 0);
  retval |= check_joy(status, down, up, KEY_B, 1);
  retval |= check_joy(status, down, up, KEY_X, 2);
  retval |= check_joy(status, down, up, KEY_Y, 3);
  retval |= check_joy(status, down, up, KEY_L | KEY_SL, 4);
  retval |= check_joy(status, down, up, KEY_R | KEY_SR, 5);
  retval |= check_joy(status, down, up, KEY_MINUS, 6);
  retval |= check_joy(status, down, up, KEY_PLUS, 7);
  retval |= check_joy(status, down, up, KEY_ZL, 8);
  retval |= check_joy(status, down, up, KEY_ZR, 9);

  if ((down | held | up) & KEY_TOUCH)
  {
    hidTouchRead(&touch, 0);

    if((down & KEY_TOUCH))
    {
      status->mouse_button = MOUSE_BUTTON_LEFT;
      status->mouse_repeat = MOUSE_BUTTON_LEFT;
      status->mouse_button_state |= MOUSE_BUTTON(MOUSE_BUTTON_LEFT);
      status->mouse_repeat_state = 1;
      status->mouse_drag_state = -1;
      status->mouse_time = get_ticks();
      is_dragging = true;
      allow_focus_changes = false;
      retval = true;
    }

    if(is_dragging)
    {
      if(up & KEY_TOUCH)
      {
        status->mouse_button_state &= ~MOUSE_BUTTON(MOUSE_BUTTON_LEFT);
        status->mouse_repeat = 0;
        status->mouse_repeat_state = 0;
        status->mouse_drag_state = 0;
        allow_focus_changes = true;
        is_dragging = false;
        retval = true;
      }
      else
        retval |= nx_update_touch(status, &touch);
    }

//    retval |= nx_keyboard_update(status);
  }

  return retval;
}
