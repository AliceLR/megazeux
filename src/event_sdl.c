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

#include "event.h"
#include "platform.h"
#include "graphics.h"

#include "SDL.h"

#ifdef CONFIG_NDS
#include "keyboard.h"
#include "render_nds.h"
#endif

static bool numlock_status_initialized;

static enum keycode convert_SDL_internal(SDLKey key)
{
  switch(key)
  {
    case SDLK_BACKSPACE: return IKEY_BACKSPACE;
    case SDLK_TAB: return IKEY_TAB;
    case SDLK_RETURN: return IKEY_RETURN;
    case SDLK_ESCAPE: return IKEY_ESCAPE;
    case SDLK_SPACE: return IKEY_SPACE;
    case SDLK_QUOTE: return IKEY_QUOTE;
    case SDLK_PLUS: return IKEY_EQUALS;
    case SDLK_COMMA: return IKEY_COMMA;
    case SDLK_MINUS: return IKEY_MINUS;
    case SDLK_PERIOD: return IKEY_PERIOD;
    case SDLK_SLASH: return IKEY_SLASH;
    case SDLK_0: return IKEY_0;
    case SDLK_1: return IKEY_1;
    case SDLK_2: return IKEY_2;
    case SDLK_3: return IKEY_3;
    case SDLK_4: return IKEY_4;
    case SDLK_5: return IKEY_5;
    case SDLK_6: return IKEY_6;
    case SDLK_7: return IKEY_7;
    case SDLK_8: return IKEY_8;
    case SDLK_9: return IKEY_9;
    case SDLK_SEMICOLON: return IKEY_SEMICOLON;
#if SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION <= 2
    case SDLK_EQUALS: return IKEY_EQUALS;
#endif
    case SDLK_LEFTBRACKET: return IKEY_LEFTBRACKET;
    case SDLK_BACKSLASH: return IKEY_BACKSLASH;
    case SDLK_RIGHTBRACKET: return IKEY_RIGHTBRACKET;
    case SDLK_BACKQUOTE: return IKEY_BACKQUOTE;
    case SDLK_a: return IKEY_a;
    case SDLK_b: return IKEY_b;
    case SDLK_c: return IKEY_c;
    case SDLK_d: return IKEY_d;
    case SDLK_e: return IKEY_e;
    case SDLK_f: return IKEY_f;
    case SDLK_g: return IKEY_g;
    case SDLK_h: return IKEY_h;
    case SDLK_i: return IKEY_i;
    case SDLK_j: return IKEY_j;
    case SDLK_k: return IKEY_k;
    case SDLK_l: return IKEY_l;
    case SDLK_m: return IKEY_m;
    case SDLK_n: return IKEY_n;
    case SDLK_o: return IKEY_o;
    case SDLK_p: return IKEY_p;
    case SDLK_q: return IKEY_q;
    case SDLK_r: return IKEY_r;
    case SDLK_s: return IKEY_s;
    case SDLK_t: return IKEY_t;
    case SDLK_u: return IKEY_u;
    case SDLK_v: return IKEY_v;
    case SDLK_w: return IKEY_w;
    case SDLK_x: return IKEY_x;
    case SDLK_y: return IKEY_y;
    case SDLK_z: return IKEY_z;
    case SDLK_DELETE: return IKEY_DELETE;
    case SDLK_KP0: return IKEY_KP0;
    case SDLK_KP1: return IKEY_KP1;
    case SDLK_KP2: return IKEY_KP2;
    case SDLK_KP3: return IKEY_KP3;
    case SDLK_KP4: return IKEY_KP4;
    case SDLK_KP5: return IKEY_KP5;
    case SDLK_KP6: return IKEY_KP6;
    case SDLK_KP7: return IKEY_KP7;
    case SDLK_KP8: return IKEY_KP8;
    case SDLK_KP9: return IKEY_KP9;
    case SDLK_KP_PERIOD: return IKEY_KP_PERIOD;
    case SDLK_KP_DIVIDE: return IKEY_KP_DIVIDE;
    case SDLK_KP_MULTIPLY: return IKEY_KP_MULTIPLY;
    case SDLK_KP_MINUS: return IKEY_KP_MINUS;
    case SDLK_KP_PLUS: return IKEY_KP_PLUS;
    case SDLK_KP_ENTER: return IKEY_KP_ENTER;
    case SDLK_UP: return IKEY_UP;
    case SDLK_DOWN: return IKEY_DOWN;
    case SDLK_RIGHT: return IKEY_RIGHT;
    case SDLK_LEFT: return IKEY_LEFT;
    case SDLK_INSERT: return IKEY_INSERT;
    case SDLK_HOME: return IKEY_HOME;
    case SDLK_END: return IKEY_END;
    case SDLK_PAGEUP: return IKEY_PAGEUP;
    case SDLK_PAGEDOWN: return IKEY_PAGEDOWN;
    case SDLK_F1: return IKEY_F1;
    case SDLK_F2: return IKEY_F2;
    case SDLK_F3: return IKEY_F3;
    case SDLK_F4: return IKEY_F4;
    case SDLK_F5: return IKEY_F5;
    case SDLK_F6: return IKEY_F6;
    case SDLK_F7: return IKEY_F7;
    case SDLK_F8: return IKEY_F8;
    case SDLK_F9: return IKEY_F9;
    case SDLK_F10: return IKEY_F10;
    case SDLK_F11: return IKEY_F11;
    case SDLK_F12: return IKEY_F12;
    case SDLK_NUMLOCK: return IKEY_NUMLOCK;
    case SDLK_CAPSLOCK: return IKEY_CAPSLOCK;
    case SDLK_SCROLLOCK: return IKEY_SCROLLOCK;
    case SDLK_RSHIFT: return IKEY_RSHIFT;
    case SDLK_LSHIFT: return IKEY_LSHIFT;
    case SDLK_RCTRL: return IKEY_RCTRL;
    case SDLK_LCTRL: return IKEY_LCTRL;
    case SDLK_RALT: return IKEY_RALT;
    case SDLK_LALT: return IKEY_LALT;
    case SDLK_LSUPER: return IKEY_LSUPER;
    case SDLK_RSUPER: return IKEY_RSUPER;
    case SDLK_SYSREQ: return IKEY_SYSREQ;
    case SDLK_BREAK: return IKEY_BREAK;
    case SDLK_MENU: return IKEY_MENU;
    default: return IKEY_UNKNOWN;
  }
}

static Uint32 process_event(SDL_Event *event)
{
  Uint32 rval = 1;
  enum keycode ckey;

  /* SDL's numlock keyboard modifier handling seems to be broken on X11,
   * and it will only get numlock's status right on application init. We
   * can trust this value once, and then toggle based on user presses of
   * the numlock key.
   *
   * On Windows, KEYDOWN/KEYUP seem to be sent separately, to indicate
   * enabling or disabling of numlock. But on X11, both KEYDOWN/KEYUP are
   * sent for each toggle, so this must be handled differently.
   *
   * What a mess!
   */
  if(!numlock_status_initialized)
  {
    input.numlock_status = !!(SDL_GetModState() & KMOD_NUM);
    numlock_status_initialized = true;
  }

  switch(event->type)
  {
    case SDL_QUIT:
    {
      // Stuff an escape
      input.last_key = IKEY_ESCAPE;
      input.keymap[IKEY_ESCAPE] = 1;
      input.last_keypress_time = get_ticks();
      break;
    }

    case SDL_VIDEORESIZE:
    {
      resize_screen(event->resize.w, event->resize.h);
      break;
    }

    case SDL_MOUSEMOTION:
    {
      int mx_real = event->motion.x;
      int my_real = event->motion.y;
      int mx, my, min_x, min_y, max_x, max_y;
      get_screen_coords(mx_real, my_real, &mx, &my, &min_x,
       &min_y, &max_x, &max_y);

      if(mx > 639)
        SDL_WarpMouse(max_x, my_real);

      if(mx < 0)
        SDL_WarpMouse(min_x, my_real);

      if(my > 349)
        SDL_WarpMouse(mx_real, max_y);

      if(my < 0)
        SDL_WarpMouse(mx_real, min_y);

#ifdef CONFIG_NDS
      nds_mouselook(false);
      focus_pixel(mx_real, my_real);
      nds_mouselook(true);
#endif

      input.real_mouse_x = mx;
      input.real_mouse_y = my;
      input.mouse_x = mx / 8;
      input.mouse_y = my / 14;
      input.mouse_moved = 1;
      break;
    }

    case SDL_MOUSEBUTTONDOWN:
    {
      input.last_mouse_button = event->button.button;
      input.last_mouse_repeat = event->button.button;
      input.mouse_button_state |= SDL_BUTTON(event->button.button);
      input.last_mouse_repeat_state = 1;
      input.mouse_drag_state = -1;
      input.last_mouse_time = SDL_GetTicks();
      break;
    }

    case SDL_MOUSEBUTTONUP:
    {
      input.mouse_button_state &= ~SDL_BUTTON(event->button.button);
      input.last_mouse_repeat = 0;
      input.mouse_drag_state = 0;
      input.last_mouse_repeat_state = 0;
      break;
    }

    case SDL_KEYDOWN:
    {
      ckey = convert_SDL_internal(event->key.keysym.sym);
      if(!ckey)
      {
        if(event->key.keysym.unicode)
          ckey = IKEY_UNICODE;
        else
          break;
      }
      if((ckey == IKEY_RETURN) &&
       get_alt_status(keycode_internal) && get_ctrl_status(keycode_internal))
      {
        toggle_fullscreen();
        break;
      }

      if(ckey == IKEY_CAPSLOCK)
      {
        input.caps_status = 1;
      }

      if(ckey == IKEY_NUMLOCK)
      {
#ifdef __WIN32__
        input.numlock_status = 1;
#endif
        break;
      }

      if(ckey == IKEY_F12)
      {
        dump_screen();
        break;
      }

      // Ignore alt + tab
      if((ckey == IKEY_TAB) &&
       get_alt_status(keycode_internal))
      {
        break;
      }

      if(input.last_key_repeat &&
       (input.last_key_repeat != IKEY_LSHIFT) &&
       (input.last_key_repeat != IKEY_RSHIFT) &&
       (input.last_key_repeat != IKEY_LALT) &&
       (input.last_key_repeat != IKEY_RALT) &&
       (input.last_key_repeat != IKEY_LCTRL) &&
       (input.last_key_repeat != IKEY_RCTRL))
      {
        // Stack current repeat key if it isn't shift, alt, or ctrl
        if(input.repeat_stack_pointer != KEY_REPEAT_STACK_SIZE)
        {
          input.last_key_repeat_stack[input.repeat_stack_pointer] =
           input.last_key_repeat;
          input.last_unicode_repeat_stack[input.repeat_stack_pointer] =
           input.last_unicode_repeat;
          input.repeat_stack_pointer++;
        }
      }

      input.keymap[ckey] = 1;
      input.last_key_pressed = ckey;
      input.last_key = ckey;
      input.last_unicode = event->key.keysym.unicode;
      input.last_key_repeat = ckey;
      input.last_unicode_repeat = event->key.keysym.unicode;
      input.last_keypress_time = get_ticks();
      input.last_key_release = IKEY_UNKNOWN;
      break;
    }

    case SDL_KEYUP:
    {
      ckey = convert_SDL_internal(event->key.keysym.sym);
      if(!ckey)
      {
        if(input.keymap[IKEY_UNICODE])
          ckey = IKEY_UNICODE;
        else
          break;
      }

      if(ckey == IKEY_NUMLOCK)
      {
#ifdef __WIN32__
        input.numlock_status = 0;
#else
        input.numlock_status ^= 1;
#endif
        break;
      }

      if(ckey == IKEY_CAPSLOCK)
      {
        input.caps_status = 0;
      }

      input.keymap[ckey] = 0;
      if(input.last_key_repeat == ckey)
      {
        input.last_key_repeat = IKEY_UNKNOWN;
        input.last_unicode_repeat = 0;
      }
      input.last_key_release = ckey;
      break;
    }

    case SDL_ACTIVEEVENT:
    {
      if(input.unfocus_pause)
      {
        // Pause while minimized
        if(event->active.state & (SDL_APPACTIVE | SDL_APPINPUTFOCUS))
        {
          // Wait for SDL_APPACTIVE with gain of 1
          do
          {
            SDL_WaitEvent(event);
          } while((event->type != SDL_ACTIVEEVENT) ||
           (event->active.state & ~(SDL_APPACTIVE | SDL_APPINPUTFOCUS)));
        }
      }
      break;
    }

    case SDL_JOYAXISMOTION:
    {
      int axis_value = event->jaxis.value;
      int digital_value = -1;
      int which = event->jaxis.which;
      int axis = event->jaxis.axis;
      int last_axis = input.last_axis[which][axis];
      enum keycode stuffed_key;

      if(axis_value > 10000)
        digital_value = 1;
      else

      if(axis_value < -10000)
        digital_value = 0;

      if(digital_value != -1)
      {
        stuffed_key =
          input.joystick_axis_map[which][axis][digital_value];

        if(stuffed_key)
        {
          if(input.keymap[stuffed_key] == 0)
          {
            input.last_key_pressed = stuffed_key;
            input.last_key = stuffed_key;
            input.last_unicode = stuffed_key;
            input.last_key_repeat = stuffed_key;
            input.last_unicode_repeat = stuffed_key;
            input.keymap[stuffed_key] = 1;
            input.last_keypress_time = get_ticks();
            input.last_key_release = IKEY_UNKNOWN;
          }

          if(last_axis == (digital_value ^ 1))
          {
            stuffed_key =
              input.joystick_axis_map[which][axis][last_axis];

            input.keymap[stuffed_key] = 0;
            input.last_key_repeat = IKEY_UNKNOWN;
            input.last_unicode_repeat = 0;
            input.last_key_release = stuffed_key;
          }
        }
      }
      else
      {
        stuffed_key =
          input.joystick_axis_map[which][axis][last_axis];

        if(stuffed_key)
        {
          input.keymap[stuffed_key] = 0;
          input.last_key_repeat = IKEY_UNKNOWN;
          input.last_unicode_repeat = 0;
          input.last_key_release = stuffed_key;
        }
      }

      input.last_axis[which][axis] = digital_value;
      break;
    }

    case SDL_JOYBUTTONDOWN:
    {
      int which = event->jbutton.which;
      int button = event->jbutton.button;
      enum keycode stuffed_key =
        input.joystick_button_map[which][button];

#ifdef CONFIG_NDS
      // For the NDS, button 5 is hardcoded to subscreen mode switch.
      if(button == 5)
      {
        nds_subscreen_switch();
        break;
      }
#endif

      if(stuffed_key && (input.keymap[stuffed_key] == 0))
      {
        input.last_key_pressed = stuffed_key;
        input.last_key = stuffed_key;
        input.last_unicode = stuffed_key;
        input.last_key_repeat = stuffed_key;
        input.last_unicode_repeat = stuffed_key;
        input.keymap[stuffed_key] = 1;
        input.last_keypress_time = SDL_GetTicks();
        input.last_key_release = (enum keycode)0;
      }

      break;
    }

    case SDL_JOYBUTTONUP:
    {
      int which = event->jbutton.which;
      int button = event->jbutton.button;
      enum keycode stuffed_key =
        input.joystick_button_map[which][button];

#ifdef CONFIG_NDS
      // Probably unnecessary, but filter out button 5.
      if(button == 5)
        break;
#endif

      if(stuffed_key)
      {
        input.keymap[stuffed_key] = 0;
        input.last_key_repeat = (enum keycode)0;
        input.last_unicode_repeat = 0;
        input.last_key_release = stuffed_key;
      }

      break;
    }

    default:
    {
      rval = 0;
    }
  }

  return rval;
}

Uint32 update_event_status(void)
{
  SDL_Event event;
  Uint32 rval = 0;

  input.last_key = IKEY_UNKNOWN;
  input.last_unicode = 0;
  input.mouse_moved = 0;
  input.last_mouse_button = 0;

  while(SDL_PollEvent(&event))
  {
    rval |= process_event(&event);
  }

  rval |= update_autorepeat();

  return rval;
}

void wait_event(void)
{
  SDL_Event event;

  input.last_key = (enum keycode)0;
  input.last_unicode = 0;
  input.mouse_moved = 0;
  input.last_mouse_button = 0;

  SDL_WaitEvent(&event);
  process_event(&event);
  update_autorepeat();
}

void real_warp_mouse(Uint32 x, Uint32 y)
{
  SDL_WarpMouse(x, y);
}

void initialize_joysticks(void)
{
  int i, count;

  count = SDL_NumJoysticks();

  if(count > 16)
    count = 16;

  for(i = 0; i < count; i++)
  {
    SDL_JoystickOpen(i);
  }

  SDL_JoystickEventState(SDL_ENABLE);
}

#ifdef CONFIG_NDS

static void convert_nds_SDL(int c, SDL_keysym *sym)
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
    sym->sym = SDLK_F1 - c - 59;
  else if(c <= -133 && c >= -134)
    sym->sym = SDLK_F11 - c - 133;

  // Home, PgUp, etc.
  else if(c == -71)
    sym->sym = SDLK_HOME;
  else if(c == -73)
    sym->sym = SDLK_PAGEUP;
  else if(c == -81)
    sym->sym = SDLK_PAGEDOWN;
  else if(c == -79)
    sym->sym = SDLK_END;

  // Arrow keys
  else if(c == -72)
    sym->sym = SDLK_UP;
  else if(c == -80)
    sym->sym = SDLK_DOWN;
  else if(c == -75)
    sym->sym = SDLK_LEFT;
  else if(c == -77)
    sym->sym = SDLK_RIGHT;

  // Other normal-ish keys
  else if(c > 0 && c < 512)
    sym->sym = c;

  // Unknown key
  else
    sym->sym = SDLK_UNKNOWN;
}

static void nds_inject_keyboard(void)
{
  // Hold down a key for this many vblanks.
  const int NUM_HOLD_VBLANKS = 5;
  static SDL_Event key_event;
  static int hold_count = 0;

  if(hold_count == 1)
  {
    // Inject the key release.
    key_event.type = SDL_KEYUP;
    SDL_PushEvent(&key_event);
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
      key_event.type = SDL_KEYDOWN;
      convert_nds_SDL(c, &key_event.key.keysym);
      SDL_PushEvent(&key_event);

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
  SDL_Event motion_event;
  SDL_Event button_event;

  if(is_scaled_mode(subscreen_mode))
  {
    // Inject mouse motion events.
    if(keysHeld() & KEY_TOUCH)
    {
      touchPosition touch = touchReadXY();

      if(touch.px != last_x || touch.py != last_y)
      {
        // Send the event to SDL.
        motion_event.type = SDL_MOUSEMOTION;
        motion_event.motion.x = touch.px * 640 / 256;
        motion_event.motion.y = touch.py * 350 / 192;
        SDL_PushEvent(&motion_event);

        last_x = touch.px;
        last_y = touch.py;
      }

      if(!touch_held)
      {
       // Inject the mouse button press.
        button_event.type = SDL_MOUSEBUTTONDOWN;
        button_event.button.button = 1;
        SDL_PushEvent(&button_event);

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

      button_event.type = SDL_MOUSEBUTTONUP;
      button_event.button.button = 1;
      SDL_PushEvent(&button_event);
    }
  }
}

void nds_inject_input(void)
{
  nds_inject_keyboard();
  nds_inject_mouse();
}

#endif // CONFIG_NDS
