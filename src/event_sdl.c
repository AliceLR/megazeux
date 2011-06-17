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

extern struct input_status input;

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
    case SDLK_LSHIFT: return IKEY_LSHIFT;
    case SDLK_LCTRL: return IKEY_LCTRL;
    case SDLK_RALT: return IKEY_RALT;
    case SDLK_LALT: return IKEY_LALT;
    case SDLK_LSUPER: return IKEY_LSUPER;
    case SDLK_RSUPER: return IKEY_RSUPER;
    case SDLK_SYSREQ: return IKEY_SYSREQ;
    case SDLK_BREAK: return IKEY_BREAK;
    case SDLK_MENU: return IKEY_MENU;
#ifndef CONFIG_PANDORA
    case SDLK_HOME: return IKEY_HOME;
    case SDLK_END: return IKEY_END;
    case SDLK_PAGEUP: return IKEY_PAGEUP;
    case SDLK_PAGEDOWN: return IKEY_PAGEDOWN;
    case SDLK_RSHIFT: return IKEY_RSHIFT;
    case SDLK_RCTRL: return IKEY_RCTRL;
#else /* CONFIG_PANDORA */
    case SDLK_HOME: return input.joystick_button_map[0][0];
    case SDLK_END: return input.joystick_button_map[0][1];
    case SDLK_PAGEUP: return input.joystick_button_map[0][2];
    case SDLK_PAGEDOWN: return input.joystick_button_map[0][3];
    case SDLK_RSHIFT: return input.joystick_button_map[0][4];
    case SDLK_RCTRL: return input.joystick_button_map[0][5];
#endif /* CONFIG_PANDORA */
    default: return IKEY_UNKNOWN;
  }
}

static bool process_event(SDL_Event *event)
{
  struct buffered_status *status = store_status();
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
    status->numlock_status = !!(SDL_GetModState() & KMOD_NUM);
    numlock_status_initialized = true;
  }

  switch(event->type)
  {
    case SDL_QUIT:
    {
      // Stuff an escape
      status->key = IKEY_ESCAPE;
      status->keymap[IKEY_ESCAPE] = 1;
      status->keypress_time = get_ticks();
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

      status->real_mouse_x = mx;
      status->real_mouse_y = my;
      status->mouse_x = mx / 8;
      status->mouse_y = my / 14;
      status->mouse_moved = true;
      break;
    }

    case SDL_MOUSEBUTTONDOWN:
    {
      status->mouse_button = event->button.button;
      status->mouse_repeat = event->button.button;
      status->mouse_button_state |= SDL_BUTTON(event->button.button);
      status->mouse_repeat_state = 1;
      status->mouse_drag_state = -1;
      status->mouse_time = SDL_GetTicks();
      break;
    }

    case SDL_MOUSEBUTTONUP:
    {
      status->mouse_button_state &= ~SDL_BUTTON(event->button.button);
      status->mouse_repeat = 0;
      status->mouse_drag_state = 0;
      status->mouse_repeat_state = 0;
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
       get_alt_status(keycode_internal) &&
       get_ctrl_status(keycode_internal))
      {
        toggle_fullscreen();
        break;
      }

      if(ckey == IKEY_CAPSLOCK)
      {
        status->caps_status = true;
      }

      if(ckey == IKEY_NUMLOCK)
      {
#ifdef __WIN32__
        status->numlock_status = true;
#endif
        break;
      }

      if(ckey == IKEY_F12)
      {
        dump_screen();
        break;
      }

      // Ignore alt + tab
      if((ckey == IKEY_TAB) && get_alt_status(keycode_internal))
      {
        break;
      }

      if(status->key_repeat &&
       (status->key_repeat != IKEY_LSHIFT) &&
       (status->key_repeat != IKEY_RSHIFT) &&
       (status->key_repeat != IKEY_LALT) &&
       (status->key_repeat != IKEY_RALT) &&
       (status->key_repeat != IKEY_LCTRL) &&
       (status->key_repeat != IKEY_RCTRL))
      {
        // Stack current repeat key if it isn't shift, alt, or ctrl
        if(input.repeat_stack_pointer != KEY_REPEAT_STACK_SIZE)
        {
          input.key_repeat_stack[input.repeat_stack_pointer] =
           status->key_repeat;
          input.unicode_repeat_stack[input.repeat_stack_pointer] =
           status->unicode_repeat;
          input.repeat_stack_pointer++;
        }
      }

      key_press(status, ckey, event->key.keysym.unicode);
      break;
    }

    case SDL_KEYUP:
    {
      ckey = convert_SDL_internal(event->key.keysym.sym);
      if(!ckey)
      {
        if(status->keymap[IKEY_UNICODE])
          ckey = IKEY_UNICODE;
        else
          break;
      }

      if(ckey == IKEY_NUMLOCK)
      {
#ifdef __WIN32__
        status->numlock_status = false;
#else
        status->numlock_status = !status->numlock_status;
#endif
        break;
      }

      if(ckey == IKEY_CAPSLOCK)
      {
        status->caps_status = false;
      }

      status->keymap[ckey] = 0;
      if(status->key_repeat == ckey)
      {
        status->key_repeat = IKEY_UNKNOWN;
        status->unicode_repeat = 0;
      }
      status->key_release = ckey;
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
      Sint8 last_axis = status->axis[which][axis];
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
          if(status->keymap[stuffed_key] == 0)
            key_press(status, stuffed_key, stuffed_key);

          if(last_axis == (digital_value ^ 1))
          {
            key_release(status,
             input.joystick_axis_map[which][axis][last_axis]);
          }
        }
      }
      else if(last_axis != -1)
      {
        key_release(status,
          input.joystick_axis_map[which][axis][last_axis]);
      }

      status->axis[which][axis] = digital_value;
      break;
    }

    case SDL_JOYBUTTONDOWN:
    {
      int which = event->jbutton.which;
      int button = event->jbutton.button;
      enum keycode stuffed_key = input.joystick_button_map[which][button];

      if(stuffed_key && (status->keymap[stuffed_key] == 0))
        key_press(status, stuffed_key, stuffed_key);

      break;
    }

    case SDL_JOYBUTTONUP:
    {
      int which = event->jbutton.which;
      int button = event->jbutton.button;
      enum keycode stuffed_key = input.joystick_button_map[which][button];

      if(stuffed_key)
        key_release(status, stuffed_key);

      break;
    }

    default:
      return false;
  }

  return true;
}

bool __update_event_status(void)
{
  Uint32 rval = false;
  SDL_Event event;

  while(SDL_PollEvent(&event))
    rval |= process_event(&event);

  return rval;
}

void __wait_event(void)
{
  SDL_Event event;

  SDL_WaitEvent(&event);
  process_event(&event);
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
