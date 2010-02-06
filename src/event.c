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

// Event handler for SDL events, primarily keypresses and mouse events.

#include "event.h"
#include "SDL.h"
#include "delay.h"

#include "graphics.h"

input_status input;

Uint32 update_event_status()
{
  SDL_Event event;
  Uint32 rval = 0;

  input.last_SDL = (SDLKey)0;
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

Uint32 update_event_status_delay()
{
  int rval = update_event_status();
  int delay_ticks;

  if(!input.last_update_time)
    input.last_update_time = get_ticks();

  delay_ticks = UPDATE_DELAY -
   (get_ticks() - input.last_update_time);

  input.last_update_time = get_ticks();

  if(delay_ticks < 0)
   delay_ticks = 0;

  delay(delay_ticks);
  return rval;
}

void wait_event()
{
  SDL_Event event;

  input.last_SDL = (SDLKey)0;
  input.last_unicode = 0;
  input.mouse_moved = 0;
  input.last_mouse_button = 0;

  SDL_WaitEvent(&event);
  process_event(&event);
  update_autorepeat();
}

Uint32 process_event(SDL_Event *event)
{
  Uint32 rval = 1;

  switch(event->type)
  {
    case SDL_QUIT:
    {
      // Stuff an escape
      input.last_SDL = SDLK_ESCAPE;
      input.SDL_keymap[SDLK_ESCAPE] = 1;
      input.last_keypress_time = SDL_GetTicks();
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
      input.mouse_button_state |= 1 << (event->button.button - 1);
      input.last_mouse_repeat_state = 1;
      input.mouse_drag_state = -1;
      input.last_mouse_time = SDL_GetTicks();
      break;
    }

    case SDL_MOUSEBUTTONUP:
    {
      input.mouse_button_state &= ~(1 << (event->button.button - 1));
      input.last_mouse_repeat = 0;
      input.mouse_drag_state = 0;
      input.last_mouse_repeat_state = 0;
      break;
    }

    case SDL_KEYDOWN:
    {
      if((event->key.keysym.sym == SDLK_RETURN) &&
       get_alt_status(keycode_SDL) && get_ctrl_status(keycode_SDL))
      {
        toggle_fullscreen();
        break;
      }

      if(event->key.keysym.sym == SDLK_CAPSLOCK)
      {
        input.caps_status = 1;
      }

      if(event->key.keysym.sym == SDLK_F12)
      {
        dump_screen();
        break;
      }

      // Ignore alt + tab
      if((event->key.keysym.sym == SDLK_TAB) &&
       get_alt_status(keycode_SDL))
      {
        break;
      }

      if(input.last_SDL_repeat &&
       (input.last_SDL_repeat != SDLK_LSHIFT) &&
       (input.last_SDL_repeat != SDLK_RSHIFT) &&
       (input.last_SDL_repeat != SDLK_LALT) &&
       (input.last_SDL_repeat != SDLK_RALT) &&
       (input.last_SDL_repeat != SDLK_LCTRL) &&
       (input.last_SDL_repeat != SDLK_RCTRL))
      {
        // Stack current repeat key if it isn't shift, alt, or ctrl
        if(input.repeat_stack_pointer != KEY_REPEAT_STACK_SIZE)
        {
          input.last_SDL_repeat_stack[input.repeat_stack_pointer] =
           input.last_SDL_repeat;
          input.last_unicode_repeat_stack[input.repeat_stack_pointer] =
           input.last_unicode_repeat;
          input.repeat_stack_pointer++;
        }
      }

      input.SDL_keymap[event->key.keysym.sym] = 1;
      input.last_SDL_pressed = event->key.keysym.sym;
      input.last_SDL = event->key.keysym.sym;
      input.last_unicode = event->key.keysym.unicode;
      input.last_SDL_repeat = event->key.keysym.sym;
      input.last_unicode_repeat = event->key.keysym.unicode;
      input.last_keypress_time = SDL_GetTicks();
      input.last_SDL_release = (SDLKey)0;
      break;
    }

    case SDL_KEYUP:
    {
      if(event->key.keysym.sym == SDLK_CAPSLOCK)
      {
        input.caps_status = 0;
      }

      input.SDL_keymap[event->key.keysym.sym] = 0;
      if(input.last_SDL_repeat == event->key.keysym.sym)
      {
        input.last_SDL_repeat = (SDLKey)0;
        input.last_unicode_repeat = 0;
      }
      input.last_SDL_release = event->key.keysym.sym;
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
      SDLKey stuffed_key;

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
          if(input.SDL_keymap[stuffed_key] == 0)
          {
            input.last_SDL_pressed = stuffed_key;
            input.last_SDL = stuffed_key;
            input.last_unicode = stuffed_key;
            input.last_SDL_repeat = stuffed_key;
            input.last_unicode_repeat = stuffed_key;
            input.SDL_keymap[stuffed_key] = 1;
            input.last_keypress_time = SDL_GetTicks();
            input.last_SDL_release = (SDLKey)0;
          }

          if(last_axis == (digital_value ^ 1))
          {
            stuffed_key =
              input.joystick_axis_map[which][axis][last_axis];

            input.SDL_keymap[stuffed_key] = 0;
            input.last_SDL_repeat = (SDLKey)0;
            input.last_unicode_repeat = 0;
            input.last_SDL_release = stuffed_key;
          }
        }
      }
      else
      {
        stuffed_key =
          input.joystick_axis_map[which][axis][last_axis];

        if(stuffed_key)
        {
          input.SDL_keymap[stuffed_key] = 0;
          input.last_SDL_repeat = (SDLKey)0;
          input.last_unicode_repeat = 0;
          input.last_SDL_release = stuffed_key;
        }
      }

      input.last_axis[which][axis] = digital_value;
      break;
    }

    case SDL_JOYBUTTONDOWN:
    {
      int which = event->jbutton.which;
      int button = event->jbutton.button;
      SDLKey stuffed_key =
        input.joystick_button_map[which][button];

      if(stuffed_key && (input.SDL_keymap[stuffed_key] == 0))
      {
        input.last_SDL_pressed = stuffed_key;
        input.last_SDL = stuffed_key;
        input.last_unicode = stuffed_key;
        input.last_SDL_repeat = stuffed_key;
        input.last_unicode_repeat = stuffed_key;
        input.SDL_keymap[stuffed_key] = 1;
        input.last_keypress_time = SDL_GetTicks();
        input.last_SDL_release = (SDLKey)0;
      }

      break;
    }

    case SDL_JOYBUTTONUP:
    {

      int which = event->jbutton.which;
      int button = event->jbutton.button;
      SDLKey stuffed_key =
        input.joystick_button_map[which][button];

      if(stuffed_key)
      {
        input.SDL_keymap[stuffed_key] = 0;
        input.last_SDL_repeat = (SDLKey)0;
        input.last_unicode_repeat = 0;
        input.last_SDL_release = stuffed_key;
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

Uint32 update_autorepeat()
{
  Uint32 rval = 0;

  // Repeat code
  Uint8 last_SDL_state = input.SDL_keymap[input.last_SDL_repeat];
  Uint8 last_mouse_state = input.last_mouse_repeat_state;

  if(last_SDL_state)
  {
    Uint32 new_time = SDL_GetTicks();
    Uint32 ms_difference = new_time -
     input.last_keypress_time;

    if(last_SDL_state == 1)
    {
      if(ms_difference > KEY_REPEAT_START)
      {
        input.last_keypress_time = new_time;
        input.SDL_keymap[input.last_SDL_repeat] = 2;
        input.last_SDL = input.last_SDL_repeat;
        input.last_unicode = input.last_unicode_repeat;
        rval = 1;
      }
    }
    else
    {
      if(ms_difference > KEY_REPEAT_RATE)
      {
        input.last_keypress_time = new_time;
        input.last_SDL = input.last_SDL_repeat;
        input.last_unicode = input.last_unicode_repeat;
        rval = 1;
      }
    }
  }
  else

  if(input.repeat_stack_pointer)
  {
    // Take repeat off the stack.
    input.repeat_stack_pointer--;
    input.last_SDL_repeat =
     input.last_SDL_repeat_stack[input.repeat_stack_pointer];
    input.last_unicode_repeat =
     input.last_unicode_repeat_stack[input.repeat_stack_pointer];

    input.last_keypress_time = SDL_GetTicks();
  }

  if(last_mouse_state)
  {
    Uint32 new_time = SDL_GetTicks();
    Uint32 ms_difference = new_time -
     input.last_mouse_time;

    if(input.mouse_drag_state == -1)
      input.mouse_drag_state = 0;
    else
      input.mouse_drag_state = 1;

    if(last_mouse_state == 1)
    {
      if(ms_difference > MOUSE_REPEAT_START)
      {
        input.last_mouse_time = new_time;
        input.last_mouse_repeat_state = 2;
        input.last_mouse_button = input.last_mouse_repeat;
        rval = 1;
      }
    }
    else
    {
      if(ms_difference > MOUSE_REPEAT_RATE)
      {
        input.last_mouse_time = new_time;
        input.last_mouse_button = input.last_mouse_repeat;
        rval = 1;
      }
    }
  }

  return rval;
}

Uint32 get_key(keycode_type type)
{
  switch(type)
  {
    case keycode_pc_xt:
    {
      return convert_SDL_xt(input.last_SDL);
    }

    case keycode_SDL:
    {
      return input.last_SDL;
    }

    case keycode_unicode:
    {
      return input.last_unicode;
    }
  }

  // Unnecessary return to shut -Wall up
  return 0;
}

Uint32 get_key_status(keycode_type type, Uint32 index)
{
  switch(type)
  {
    case keycode_pc_xt:
    {
      SDLKey first, second;
      first = convert_xt_SDL(index, &second);

      return (input.SDL_keymap[first] || input.SDL_keymap[second]);
    }

    case keycode_SDL:
    {
      return input.SDL_keymap[index];
    }

    // Not currently handled (probably will never be)
    case keycode_unicode:
    {
      return 0;
    }
  }

  // Unnecessary return to shut -Wall up
  return 0;
}

Uint32 get_last_key(keycode_type type)
{
  switch(type)
  {
    case keycode_pc_xt:
    {
      return convert_SDL_xt(input.last_SDL_pressed);
    }

    case keycode_SDL:
    {
      return input.last_SDL_pressed;
    }

    // Not currently handled (probably will never be)
    case keycode_unicode:
    {
      return 0;
    }
  }

  // Unnecessary return to shut -Wall up
  return 0;
}

Uint32 get_last_key_released(keycode_type type)
{
  switch(type)
  {
    case keycode_pc_xt:
    {
      return convert_SDL_xt(input.last_SDL_release);
    }

    case keycode_SDL:
    {
      return input.last_SDL_release;
    }

    // Not currently handled (probably will never be)
    case keycode_unicode:
    {
      return 0;
    }
  }

  // Unnecessary return to shut -Wall up
  return 0;
}

Uint32 get_mouse_movement(int *x, int *y)
{
  if(input.mouse_moved)
  {
    get_mouse_position(x, y);
    return 1;
  }

  return 0;
}

void get_mouse_position(int *x, int *y)
{
  *x = input.mouse_x;
  *y = input.mouse_y;
}

void get_real_mouse_position(int *x, int *y)
{
  *x = input.real_mouse_x;
  *y = input.real_mouse_y;
}

Uint32 get_mouse_x()
{
  return input.mouse_x;
}

Uint32 get_mouse_y()
{
  return input.mouse_y;
}

Uint32 get_mouse_drag()
{
  return input.mouse_drag_state;
}

Uint32 get_mouse_press()
{
  if(input.last_mouse_button <= SDL_BUTTON_RIGHT)
    return input.last_mouse_button;
  else
    return 0;
}

Uint32 get_mouse_press_ext()
{
  return input.last_mouse_button;
}

Uint32 get_mouse_status()
{
  return (input.mouse_button_state &
   (SDL_BUTTON(1) | SDL_BUTTON(2) | SDL_BUTTON(3)));
}

Uint32 get_mouse_status_ext()
{
  return input.mouse_button_state;
}

void warp_mouse(Uint32 x, Uint32 y)
{
  int mx = (x * 8) + 4;
  int my = (y * 14) + 7;
  int mx_real, my_real;
  input.mouse_x = x;
  input.mouse_y = y;
  input.real_mouse_x = mx;
  input.real_mouse_y = my;
  set_screen_coords(mx, my, &mx_real, &my_real);
  SDL_WarpMouse(mx_real, my_real);
}

void warp_mouse_x(Uint32 x)
{
  int mx = (x * 8) + 4;
  int mx_real, my_real;
  input.mouse_x = x;
  input.real_mouse_x = mx;
  set_screen_coords(mx, input.real_mouse_y, &mx_real, &my_real);
  SDL_WarpMouse(mx_real, my_real);
}

void warp_mouse_y(Uint32 y)
{
  int my = (y * 14) + 7;
  int mx_real, my_real;
  input.mouse_y = y;
  input.real_mouse_y = my;
  set_screen_coords(input.real_mouse_x, my, &mx_real, &my_real);
  SDL_WarpMouse(mx_real, my_real);
}

void force_last_key(keycode_type type, int val)
{
  switch(type)
  {
    case keycode_pc_xt:
    {
      SDLKey second;
      input.last_SDL_pressed = convert_xt_SDL(val, &second);
      break;
    }

    case keycode_SDL:
    {
      input.last_SDL_pressed = (SDLKey)val;
      break;
    }

    // Not currently handled (probably will never be)
    case keycode_unicode:
    {
      break;
    }
  }
}

Uint32 convert_SDL_xt(SDLKey key)
{
  switch(key)
  {
    case SDLK_ESCAPE: return 0x01;
    case SDLK_F1: return 0x3B;
    case SDLK_F2: return 0x3C;
    case SDLK_F3: return 0x3D;
    case SDLK_F4: return 0x3E;
    case SDLK_F5: return 0x3F;
    case SDLK_F6: return 0x40;
    case SDLK_F7: return 0x41;
    case SDLK_F8: return 0x42;
    case SDLK_F9: return 0x43;
    case SDLK_F10: return 0x44;
    case SDLK_F11: return 0x57;
    case SDLK_F12: return 0x58;
    case SDLK_BACKQUOTE: return 0x29;
    case SDLK_1: return 0x02;
    case SDLK_2: return 0x03;
    case SDLK_3: return 0x04;
    case SDLK_4: return 0x05;
    case SDLK_5: return 0x06;
    case SDLK_6: return 0x07;
    case SDLK_7: return 0x08;
    case SDLK_8: return 0x09;
    case SDLK_9: return 0x0A;
    case SDLK_0: return 0x0B;
    case SDLK_MINUS: return 0x0C;
    case SDLK_EQUALS: return 0x0D;
    case SDLK_BACKSLASH: return 0x2B;
    case SDLK_BACKSPACE: return 0x0E;
    case SDLK_TAB: return 0x0F;
    case SDLK_q: return 0x10;
    case SDLK_w: return 0x11;
    case SDLK_e: return 0x12;
    case SDLK_r: return 0x13;
    case SDLK_t: return 0x14;
    case SDLK_y: return 0x15;
    case SDLK_u: return 0x16;
    case SDLK_i: return 0x17;
    case SDLK_o: return 0x18;
    case SDLK_p: return 0x19;
    case SDLK_LEFTBRACKET: return 0x1A;
    case SDLK_RIGHTBRACKET: return 0x1B;
    case SDLK_CAPSLOCK: return 0x3A;
    case SDLK_a: return 0x1E;
    case SDLK_s: return 0x1F;
    case SDLK_d: return 0x20;
    case SDLK_f: return 0x21;
    case SDLK_g: return 0x22;
    case SDLK_h: return 0x23;
    case SDLK_j: return 0x24;
    case SDLK_k: return 0x25;
    case SDLK_l: return 0x26;
    case SDLK_SEMICOLON: return 0x27;
    case SDLK_QUOTE: return 0x28;
    case SDLK_RETURN: return 0x1C;
    case SDLK_LSHIFT: return 0x2A;
    case SDLK_z: return 0x2C;
    case SDLK_x: return 0x2D;
    case SDLK_c: return 0x2E;
    case SDLK_v: return 0x2F;
    case SDLK_b: return 0x30;
    case SDLK_n: return 0x31;
    case SDLK_m: return 0x32;
    case SDLK_COMMA: return 0x33;
    case SDLK_PERIOD: return 0x34;
    case SDLK_SLASH: return 0x35;
    case SDLK_RSHIFT: return 0x36;
    case SDLK_LCTRL: return 0x1D;
    case SDLK_LSUPER: return 0x5B;
    case SDLK_LALT: return 0x38;
    case SDLK_SPACE: return 0x39;
    case SDLK_RALT: return 0x38;
    case SDLK_RSUPER: return 0x5C;
    case SDLK_MENU: return 0x5D;
    case SDLK_RCTRL: return 0x1D;
    case SDLK_SYSREQ: return 0x37;
    case SDLK_SCROLLOCK: return 0x46;
    case SDLK_BREAK: return 0xC5;
    case SDLK_INSERT: return 0x52;
    case SDLK_HOME: return 0x47;
    case SDLK_PAGEUP: return 0x49;
    case SDLK_DELETE: return 0x53;
    case SDLK_END: return 0x4F;
    case SDLK_PAGEDOWN: return 0x51;
    case SDLK_NUMLOCK: return 0x45;
    case SDLK_KP_DIVIDE: return 0x35;
    case SDLK_KP_MULTIPLY: return 0x37;
    case SDLK_KP_MINUS: return 0x4A;
    case SDLK_KP7: return 0x47;
    case SDLK_KP8: return 0x48;
    case SDLK_KP9: return 0x49;
    case SDLK_KP4: return 0x4B;
    case SDLK_KP5: return 0x4C;
    case SDLK_KP6: return 0x4D;
    case SDLK_KP_PLUS: return 0x4E;
    case SDLK_KP1: return 0x4F;
    case SDLK_KP2: return 0x50;
    case SDLK_KP3: return 0x51;
    case SDLK_KP_ENTER: return 0x1C;
    case SDLK_KP0: return 0x52;
    case SDLK_KP_PERIOD: return 0x53;
    case SDLK_UP: return 0x48;
    case SDLK_LEFT: return 0x4B;
    case SDLK_DOWN: return 0x50;
    case SDLK_RIGHT: return 0x4D;

    // All others are currently undefined; returns 0
    default: return 0;
  }
}

SDLKey convert_xt_SDL(Uint32 key, SDLKey *second)
{
  *second = (SDLKey)0;
  switch(key)
  {
    case 0x01: return SDLK_ESCAPE;
    case 0x3B: return SDLK_F1;
    case 0x3C: return SDLK_F2;
    case 0x3D: return SDLK_F3;
    case 0x3E: return SDLK_F4;
    case 0x3F: return SDLK_F5;
    case 0x40: return SDLK_F6;
    case 0x41: return SDLK_F7;
    case 0x42: return SDLK_F8;
    case 0x43: return SDLK_F9;
    case 0x44: return SDLK_F10;
    case 0x57: return SDLK_F11;
    case 0x58: return SDLK_F12;
    case 0x29: return SDLK_BACKQUOTE;
    case 0x02: return SDLK_1;
    case 0x03: return SDLK_2;
    case 0x04: return SDLK_3;
    case 0x05: return SDLK_4;
    case 0x06: return SDLK_5;
    case 0x07: return SDLK_6;
    case 0x08: return SDLK_7;
    case 0x09: return SDLK_8;
    case 0x0A: return SDLK_9;
    case 0x0B: return SDLK_0;
    case 0x0C: return SDLK_MINUS;
    case 0x0D: return SDLK_EQUALS;
    case 0x2B: return SDLK_BACKSLASH;
    case 0x0E: return SDLK_BACKSPACE;
    case 0x0F: return SDLK_TAB;
    case 0x10: return SDLK_q;
    case 0x11: return SDLK_w;
    case 0x12: return SDLK_e;
    case 0x13: return SDLK_r;
    case 0x14: return SDLK_t;
    case 0x15: return SDLK_y;
    case 0x16: return SDLK_u;
    case 0x17: return SDLK_i;
    case 0x18: return SDLK_o;
    case 0x19: return SDLK_p;
    case 0x1A: return SDLK_LEFTBRACKET;
    case 0x1B: return SDLK_RIGHTBRACKET;
    case 0x3A: return SDLK_CAPSLOCK;
    case 0x1E: return SDLK_a;
    case 0x1F: return SDLK_s;
    case 0x20: return SDLK_d;
    case 0x21: return SDLK_f;
    case 0x22: return SDLK_g;
    case 0x23: return SDLK_h;
    case 0x24: return SDLK_j;
    case 0x25: return SDLK_k;
    case 0x26: return SDLK_l;
    case 0x27: return SDLK_SEMICOLON;
    case 0x28: return SDLK_QUOTE;
    case 0x1C:
      *second = SDLK_KP_ENTER;
      return SDLK_RETURN;
    case 0x2A: return SDLK_LSHIFT;
    case 0x2C: return SDLK_z;
    case 0x2D: return SDLK_x;
    case 0x2E: return SDLK_c;
    case 0x2F: return SDLK_v;
    case 0x30: return SDLK_b;
    case 0x31: return SDLK_n;
    case 0x32: return SDLK_m;
    case 0x33: return SDLK_COMMA;
    case 0x34: return SDLK_PERIOD;
    case 0x35:
      *second = SDLK_KP_DIVIDE;
      return SDLK_SLASH;
    case 0x36: return SDLK_RSHIFT;
    case 0x1D:
      *second = SDLK_RCTRL;
      return SDLK_LCTRL;
    case 0x5B: return SDLK_LSUPER;
    case 0x38:
      *second = SDLK_RALT;
      return SDLK_LALT;
    case 0x39: return SDLK_SPACE;
    case 0x5C: return SDLK_RSUPER;
    case 0x5D: return SDLK_MENU;
    case 0x37:
      *second = SDLK_KP_MULTIPLY;
      return SDLK_SYSREQ;
    case 0x46: return SDLK_SCROLLOCK;
    case 0xC5: return SDLK_BREAK;
    case 0x52:
      *second = SDLK_KP0;
      return SDLK_INSERT;
    case 0x47:
      *second = SDLK_KP7;
      return SDLK_HOME;
    case 0x49:
      *second = SDLK_KP9;
      return SDLK_PAGEUP;
    case 0x53:
      *second = SDLK_KP_PERIOD;
      return SDLK_DELETE;
    case 0x4F:
      *second = SDLK_KP1;
      return SDLK_END;
    case 0x51:
      *second = SDLK_KP3;
      return SDLK_PAGEDOWN;
    case 0x45: return SDLK_NUMLOCK;
    case 0x4A: return SDLK_KP_MINUS;
    case 0x48:
      *second = SDLK_UP;
      return SDLK_KP8;
    case 0x4B:
      *second = SDLK_LEFT;
       return SDLK_KP4;
    case 0x4C: return SDLK_KP5;
    case 0x4D:
      *second = SDLK_RIGHT;
      return SDLK_KP6;
    case 0x4E: return SDLK_KP_PLUS;
    case 0x50:
      *second = SDLK_DOWN;
      return SDLK_KP2;
    default: return (SDLKey)0;
  }
}

int get_alt_status(keycode_type type)
{
  if(get_key_status(type, SDLK_LALT) ||
   get_key_status(type, SDLK_RALT))
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

int get_shift_status(keycode_type type)
{
  return(get_key_status(type, SDLK_LSHIFT) ||
   get_key_status(type, SDLK_RSHIFT));
}

int get_ctrl_status(keycode_type type)
{
  if(get_key_status(type, SDLK_LCTRL) ||
   get_key_status(type, SDLK_RCTRL))
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

void initialize_joysticks()
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

void map_joystick_axis(int joystick, int axis, SDLKey min_key,
 SDLKey max_key)
{
  input.joystick_axis_map[joystick][axis][0] = min_key;
  input.joystick_axis_map[joystick][axis][1] = max_key;
}

void map_joystick_button(int joystick, int button, SDLKey key)
{
  input.joystick_button_map[joystick][button] = key;
}

void set_refocus_pause(int val)
{
  input.unfocus_pause = val;
}

