/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Kevin Vance <kvance@kvance.com>
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

#include "keyboard.h"
#include "render.h"

void nds_inject_input(void);

extern struct input_status input;

bool __update_event_status(void)
{
  // FIXME: stub
  return true;
}

void __wait_event(void)
{
  // FIXME: stub
}

void real_warp_mouse(Uint32 x, Uint32 y)
{
  // FIXME: stub
}

void initialize_joysticks(void)
{
  // stub
}

#if 0
static void convert_nds_normal(int c, SDL_keysym *sym)
{
  sym->scancode = c;
  sym->unicode = c;

  sym->mod = KMOD_NONE;

  // Uppercase letters
  if(c >= 65 && c <= 90)
  {
    sym->sym = c + 26;
    sym->mod = KMOD_SHIFT;
  }

  // Function keys
  else if(c <= -59 && c >= -68)
    sym->sym = IKEY_F1 - c - 59;
  else if(c <= -133 && c >= -134)
    sym->sym = IKEY_F11 - c - 133;

  // Home, PgUp, etc.
  else if(c == -71)
    sym->sym = IKEY_HOME;
  else if(c == -73)
    sym->sym = IKEY_PAGEUP;
  else if(c == -81)
    sym->sym = IKEY_PAGEDOWN;
  else if(c == -79)
    sym->sym = IKEY_END;

  // Arrow keys
  else if(c == -72)
    sym->sym = IKEY_UP;
  else if(c == -80)
    sym->sym = IKEY_DOWN;
  else if(c == -75)
    sym->sym = IKEY_LEFT;
  else if(c == -77)
    sym->sym = IKEY_RIGHT;

  // Other normal-ish keys
  else if(c > 0 && c < 512)
    sym->sym = c;

  // Unknown key
  else
    sym->sym = IKEY_UNKNOWN;
}
#endif

static void nds_inject_keyboard(void)
{
  // Hold down a key for this many vblanks.
  const int NUM_HOLD_VBLANKS = 5;
  //static SDL_Event key_event;
  static int hold_count = 0;

  if(hold_count == 1)
  {
    // Inject the key release.
    //key_event.type = SDL_KEYUP;
    //SDL_PushEvent(&key_event);
    hold_count = 0;
  }
  else if(hold_count > 1)
  {
    // Count down.
    hold_count--;
  }
  else if(hold_count == 0 && subscreen_mode == SUBSCREEN_KEYBOARD)
  {
    // Check for a new keypress.
    int c;
    if((c = processKeyboard()))
    {
      // Inject the keypress.
      //key_event.type = SDL_KEYDOWN;
      //convert_nds_normal(c, &key_event.key.keysym);
      //SDL_PushEvent(&key_event);

      // Remember to release it after a number of vblanks.
      hold_count = NUM_HOLD_VBLANKS + 1;
    }
  }
}

static void nds_inject_mouse(void)
{
  static bool touch_held = false;
  static int last_x = -1;
  static int last_y = -1;
  //SDL_Event motion_event;
  //SDL_Event button_event;

  if(is_scaled_mode(subscreen_mode))
  {
    // Inject mouse motion events.
    if(keysHeld() & KEY_TOUCH)
    {
      touchPosition touch;
      touchRead(&touch);

      if(touch.px != last_x || touch.py != last_y)
      {
        // Send the event to SDL.
        //motion_event.type = SDL_MOUSEMOTION;
        //motion_event.motion.x = touch.px * 640 / 256;
        //motion_event.motion.y = touch.py * 350 / 192;
        //SDL_PushEvent(&motion_event);

        last_x = touch.px;
        last_y = touch.py;
      }

      if(!touch_held)
      {
       // Inject the mouse button press.
        //button_event.type = SDL_MOUSEBUTTONDOWN;
        //button_event.button.button = 1;
        //SDL_PushEvent(&button_event);

        touch_held = true;
        last_x = -1;
        last_y = -1;
      }
    }
    else
    {
      // Inject the mouse button release.
      touch_held = false;
      nds_mouselook(false);

      //button_event.type = SDL_MOUSEBUTTONUP;
      //button_event.button.button = 1;
      //SDL_PushEvent(&button_event);
    }
  }
}

void nds_inject_input(void)
{
  nds_inject_keyboard();
  nds_inject_mouse();
}
