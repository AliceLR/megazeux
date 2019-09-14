/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007-2009 Kevin Vance <kvance@kvance.com>
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

#include <nds/arm9/keyboard.h>
#include "render.h"
#include "evq.h"

void nds_update_input(void);
static boolean process_event(NDSEvent *event);

extern struct input_status input;
extern u16 subscreen_height;

static boolean keyboard_allow_release = true;

boolean __update_event_status(void)
{
  boolean retval = false;
  NDSEvent event;

  while(nds_event_poll(&event))
    retval |= process_event(&event);

  return retval;
}

void __wait_event(void)
{
  NDSEvent event;

  while(!nds_event_poll(&event))
    swiWaitForVBlank();
  process_event(&event);
}

void real_warp_mouse(int x, int y)
{
  const struct buffered_status *status = load_status();

  if(x < 0)
    x = status->real_mouse_x;

  if(y < 0)
    y = status->real_mouse_y;

  // Since we can't warp a touchscreen stylus, focus there instead.
  focus_pixel(x, y);
}

void initialize_joysticks(void)
{
  struct buffered_status *status = store_status();
  joystick_set_active(status, 0, true);
}

static int nds_map_joystick(int nds_button, boolean *is_hat)
{
  // Determine how to handle this particular joystick input.
  // Map the directional pad to the hat and everything else to buttons.
  *is_hat = false;

  switch(nds_button)
  {
    case KEY_UP:      *is_hat = true; return JOYHAT_UP;
    case KEY_DOWN:    *is_hat = true; return JOYHAT_DOWN;
    case KEY_LEFT:    *is_hat = true; return JOYHAT_LEFT;
    case KEY_RIGHT:   *is_hat = true; return JOYHAT_RIGHT;
    case KEY_A:       return 0;
    case KEY_B:       return 1;
    case KEY_X:       return 2;
    case KEY_Y:       return 3;
    case KEY_L:       return 4;
    case KEY_R:       return 5;
    case KEY_SELECT:  return 6;
    case KEY_START:   return 7;
  }
  return -1;
}

static void convert_nds_internal(int key, int *internal_code, int *unicode)
{
  // Actually ASCIIish, but close enough.
  if(key > 0)
    *unicode = key;
  else
    *unicode = 0;

  // Uppercase letters
  if(key >= 65 && key <= 90)
    *internal_code = key + 32;

  // Arrow keys
  else if(key == -17)
    *internal_code = IKEY_UP;
  else if(key == -19)
    *internal_code = IKEY_DOWN;
  else if(key == -20)
    *internal_code = IKEY_LEFT;
  else if(key == -18)
    *internal_code = IKEY_RIGHT;

  // Other keys
  else if(key == -23)
    *internal_code = IKEY_ESCAPE;
  else if(key == -15)
    *internal_code = IKEY_CAPSLOCK;
  else if(key == 10)
    *internal_code = IKEY_RETURN;
  else if(key == -14)
    *internal_code = IKEY_LSHIFT;
  else if(key == -16)
    *internal_code = IKEY_LCTRL;
  else if(key == -26)
    *internal_code = IKEY_LALT;
  else if(key == -5)
    *internal_code = IKEY_MENU;

  // ASCIIish keys
  else if(key > 0 && key < 512)
    *internal_code = key;

  // Unknown key
  else
    *internal_code = IKEY_UNKNOWN;
}

static boolean process_event(NDSEvent *event)
{
  struct buffered_status *status = store_status();
  boolean retval = true;

  switch(event->type)
  {
    // Hardware key pressed
    case NDS_EVENT_KEY_DOWN:
    {
      boolean is_hat;
      int button;

      // Special case- R is currently hardcoded to the keyboard.
      if(event->key == KEY_R)
      {
        nds_subscreen_switch();
        retval = false;
        break;
      }

      button = nds_map_joystick(event->key, &is_hat);
      if(button >= 0)
      {
        if(is_hat)
          joystick_hat_update(status, 0, button, true);
        else
          joystick_button_press(status, 0, button);
      }
      break;
    }

    case NDS_EVENT_KEY_UP:
    {
      boolean is_hat;
      int button;

      button = nds_map_joystick(event->key, &is_hat);
      if(button >= 0)
      {
        if(is_hat)
          joystick_hat_update(status, 0, button, false);
        else
          joystick_button_release(status, 0, button);
      }
      break;
    }

    // Software key down
    case NDS_EVENT_KEYBOARD_DOWN:
    {
      int internal_code, unicode;
      convert_nds_internal(event->key, &internal_code, &unicode);
      key_press(status, internal_code, unicode);

      keyboard_allow_release = true;
      break;
    }

    case NDS_EVENT_KEYBOARD_UP:
    {
      int internal_code, unicode;
      convert_nds_internal(event->key, &internal_code, &unicode);
      key_release(status, internal_code);
      break;
    }

    // Touchscreen stylus down
    case NDS_EVENT_TOUCH_DOWN:
    {
      int button = MOUSE_BUTTON_LEFT;
      status->mouse_button = button;
      status->mouse_repeat = button;
      status->mouse_button_state |= MOUSE_BUTTON(button);
      status->mouse_repeat_state = 1;
      status->mouse_drag_state = -1;
      status->mouse_time = get_ticks();
      break;
    }

    // Touchscreen stylus up
    case NDS_EVENT_TOUCH_UP:
    {
      int button = MOUSE_BUTTON_LEFT;
      status->mouse_button_state &= ~MOUSE_BUTTON(button);
      status->mouse_repeat = 0;
      status->mouse_drag_state = 0;
      status->mouse_repeat_state = 0;
      nds_mouselook(false);
      break;
    }

    // Touchscreen stylus drag
    case NDS_EVENT_TOUCH_MOVE:
    {
      int mx = event->x * 640 / 256;
      int my = (event->y - ((192 - subscreen_height)/2)) * 350 / subscreen_height;
      if(my < 0 || my >= 350)
        break;

      // Update the MZX mouse state.
      status->real_mouse_x = mx;
      status->real_mouse_y = my;
      status->mouse_x = mx / 8;
      status->mouse_y = my / 14;
      status->mouse_moved = true;

      // Update our internal mouselook state.
      nds_mouselook(false);
      focus_pixel(mx, my);
      nds_mouselook(true);
      break;
    }

    // Unknown event?
    default:
    {
      retval = false;
      break;
    }
  };

  return retval;
}

static void nds_update_hw_keys(void)
{
  static int last_keys_mask = 0;
  int keys = keysHeld();
  int check[] = { KEY_A, KEY_B, KEY_SELECT, KEY_START, KEY_RIGHT, KEY_LEFT,
                  KEY_UP, KEY_DOWN, KEY_R, KEY_L, KEY_X, KEY_Y };
  NDSEvent event;

  for(size_t i = 0; i < sizeof(check)/sizeof(*check); i++)
  {
    int keybit = check[i];
    if(keybit & keys)
    {
      if(!(keybit & last_keys_mask))
      {
        // Key pressed
        nds_event_fill_key_down(&event, keybit);
        nds_event_push(&event);
        last_keys_mask |= keybit;
      }
    }
    else if(keybit & last_keys_mask)
    {
      // Key released
      nds_event_fill_key_up(&event, keybit);
      nds_event_push(&event);
      last_keys_mask &= ~keybit;
    }
  }
}

static void nds_update_sw_keyboard(void)
{
  static int last_key = -1;
  NDSEvent event;

  // Release last key
  if(keyboard_allow_release && last_key != -1)
  {
    nds_event_fill_keyboard_up(&event, last_key);
    nds_event_push(&event);
    last_key = -1;
  }

  // Press new key (only allow if the keyboard is active)
  if(subscreen_mode == SUBSCREEN_KEYBOARD && last_key == -1)
  {
    int c = keyboardUpdate();

    if(c != -1)
    {
      nds_event_fill_keyboard_down(&event, c);
      nds_event_push(&event);
      keyboard_allow_release = false;
      last_key = c;
    }
  }
}

static void nds_update_mouse(void)
{
  static boolean last_touch_press = false;
  static int last_touch_x = -1;
  static int last_touch_y = -1;
  NDSEvent new_event;

  if(is_scaled_mode(subscreen_mode))
  {
    // Inject mouse motion events.
    if(keysHeld() & KEY_TOUCH)
    {
      touchPosition touch;
      touchRead(&touch);

      if(touch.px != last_touch_x || touch.py != last_touch_y)
      {
        // Add the event.
        nds_event_fill_touch_move(&new_event, touch.px, touch.py);
        nds_event_push(&new_event);

        last_touch_x = touch.px;
        last_touch_y = touch.py;
      }

      if(!last_touch_press)
      {
        nds_event_fill_touch_down(&new_event);
        nds_event_push(&new_event);

        last_touch_press = true;
      }
    }
    else if(last_touch_press)
    {
      nds_event_fill_touch_up(&new_event);
      nds_event_push(&new_event);

      last_touch_press = false;
    }
  }
}

void nds_update_input(void)
{
  scanKeys();
  nds_update_hw_keys();
  nds_update_sw_keyboard();
  nds_update_mouse();
}
