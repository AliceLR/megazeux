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
#include "../../src/graphics.h"
#include "../../src/platform.h"
#include "../../src/util.h"

#include <3ds.h>

#include "event.h"
#include "keyboard.h"

extern struct input_status input;
static enum bottom_screen_mode b_mode;
static enum focus_mode allow_focus_changes = FOCUS_ALLOW;
static boolean is_dragging = false;

boolean update_hid(void);

enum focus_mode get_allow_focus_changes(void)
{
  return allow_focus_changes;
}

enum bottom_screen_mode get_bottom_screen_mode(void)
{
  return b_mode;
}

boolean __update_event_status(void)
{
  boolean retval = false;
  retval |= aptMainLoop();
  retval |= update_hid();
  return retval;
}

void __wait_event(void)
{
  while(!__update_event_status())
    gspWaitForVBlank();
}

// Convert the 3DS axis ranges to the MZX axis ranges. The 3DS uses values in
// (roughly) the -150 to 150 range, but MZX uses the full range of a Sint16.
// The 3DS Y axis is also inverted from the typical axis direction, but that
// is fixed in check_circle below.
static inline Sint16 axis_convert(s16 value)
{
  int new_value = ((int)value) * 32768 / 150;
  return (Sint16)CLAMP(new_value, -32768, 32767);
}

static inline boolean check_circle(struct buffered_status *status,
 circlePosition *current, circlePosition *prev, Uint32 axis_x, Uint32 axis_y)
{
  boolean rval = false;

  if(current->dx != prev->dx)
  {
    joystick_axis_update(status, 0, axis_x, axis_convert(current->dx));
    rval = true;
  }

  if(current->dy != prev->dy)
  {
    // 3DS has an inverted Y axis on the circle pad vs. most controllers.
    joystick_axis_update(status, 0, axis_y, axis_convert(-current->dy));
    rval = true;
  }

  return rval;
}

static inline boolean check_hat(struct buffered_status *status,
 Uint32 down, Uint32 up, Uint32 key, enum joystick_hat dir)
{
  if(down & key)
  {
    joystick_hat_update(status, 0, dir, true);
    return true;
  }
  else

  if(up & key)
  {
    joystick_hat_update(status, 0, dir, false);
    return true;
  }

  return false;
}

static inline boolean check_joy(struct buffered_status *status,
  Uint32 down, Uint32 up, Uint32 key, Uint32 button)
{
  if(down & key)
  {
    joystick_button_press(status, 0, button);
    return true;
  }
  else

  if(up & key)
  {
    joystick_button_release(status, 0, button);
    return true;
  }

  return false;
}

static inline boolean ctr_is_mouse_area(touchPosition *touch)
{
  int mx, my;

  if(b_mode == BOTTOM_SCREEN_MODE_KEYBOARD)
  {
    mx = touch->px * 4 - 320;
    my = touch->py * 4 - (13 * 4);

    if(mx < 0 || mx >= 640 || my < 0 || my >= 350)
      return false;

    else
      return true;
  }
  else
  {
    return true;
  }
}

static inline boolean ctr_update_touch(struct buffered_status *status,
 touchPosition *touch)
{
  int mx, my;

  if(b_mode == BOTTOM_SCREEN_MODE_KEYBOARD)
  {
    mx = touch->px * 4 - 320;
    my = touch->py * 4 - (13 * 4);

    if(mx < 0 || mx >= 640 || my < 0 || my >= 350)
      return false;
  }
  else
  {
    int s_height = ctr_get_subscreen_height();
    mx = touch->px * 2;
    my = ((int)(touch->py - ((240 - s_height) / 2)) * 350 / s_height);
    if(mx < 0) mx = 0;
    if(mx >= 640) mx = 639;
    if(my < 0) my = 0;
    if(my >= 350) my = 349;
  }

  if((Uint32) mx != status->real_mouse_x || (Uint32) my != status->real_mouse_y)
  {
    status->real_mouse_x = mx;
    status->real_mouse_y = my;
    status->mouse_x = mx / 8;
    status->mouse_y = my / 14;
    status->mouse_moved = true;

    allow_focus_changes = FOCUS_PASS;
    focus_pixel(mx, my);
    allow_focus_changes = FOCUS_FORBID;

    return true;
  }
  else
  {
    return false;
  }
}

static inline boolean ctr_update_cstick(struct buffered_status *status)
{
  circlePosition pos;
  int dmx, dmy, nmx, nmy;

  hidCstickRead(&pos);
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

    if((Uint32) nmx != status->real_mouse_x ||
     (Uint32) nmy != status->real_mouse_y)
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

boolean update_hid(void)
{
  struct buffered_status *status = store_status();
  Uint32 down, held, up;
  boolean retval = false;
  touchPosition touch;
  circlePosition cpad;
  static circlePosition last_cpad;

  hidScanInput();
  hidCircleRead(&cpad);
  down = hidKeysDown();
  held = hidKeysHeld();
  up = hidKeysUp();

  retval |= check_circle(status, &cpad, &last_cpad, 0, 1);
  retval |= check_hat(status, down, up, KEY_DUP, JOYHAT_UP);
  retval |= check_hat(status, down, up, KEY_DDOWN, JOYHAT_DOWN);
  retval |= check_hat(status, down, up, KEY_DLEFT, JOYHAT_LEFT);
  retval |= check_hat(status, down, up, KEY_DRIGHT, JOYHAT_RIGHT);
  retval |= check_joy(status, down, up, KEY_A, 0);
  retval |= check_joy(status, down, up, KEY_B, 1);
  retval |= check_joy(status, down, up, KEY_X, 2);
  retval |= check_joy(status, down, up, KEY_Y, 3);
  retval |= check_joy(status, down, up, KEY_L, 4);
  retval |= check_joy(status, down, up, KEY_SELECT, 6);
  retval |= check_joy(status, down, up, KEY_START, 7);
  retval |= check_joy(status, down, up, KEY_ZL, 8);
  retval |= check_joy(status, down, up, KEY_ZR, 9);

  last_cpad = cpad;

  if(down & KEY_R)
  {
    b_mode = (b_mode + 1) % BOTTOM_SCREEN_MODE_MAX;
    retval = true;
  }

  if((down | held | up) & KEY_TOUCH)
  {
    hidTouchRead(&touch);

    if((down & KEY_TOUCH) && ctr_is_mouse_area(&touch))
    {
      status->mouse_button = MOUSE_BUTTON_LEFT;
      status->mouse_repeat = MOUSE_BUTTON_LEFT;
      status->mouse_button_state |= MOUSE_BUTTON(MOUSE_BUTTON_LEFT);
      status->mouse_repeat_state = 1;
      status->mouse_drag_state = -1;
      status->mouse_time = get_ticks();
      is_dragging = true;
      allow_focus_changes = FOCUS_FORBID;
      retval = true;
    }

    if(is_dragging)
    {
      if(up & KEY_TOUCH)
      {
        status->mouse_button_state &= ~MOUSE_BUTTON(MOUSE_BUTTON_LEFT);
        status->mouse_repeat = 0;
        status->mouse_repeat_state = 0;
        status->mouse_drag_state = -0;
        allow_focus_changes = FOCUS_ALLOW;
        is_dragging = false;
        retval = true;
      }
      else
      {
        retval |= ctr_update_touch(status, &touch);
      }
    }

    retval |= ctr_keyboard_update(status);
  }

//  retval |= ctr_update_cstick(status);

  return retval;
}

int ctr_get_subscreen_height(void)
{
  switch(get_config()->video_ratio)
  {
    case RATIO_CLASSIC_4_3:
    case RATIO_STRETCH:
      return 240;
    default:
      return 175;
  }
}

void initialize_joysticks(void)
{
  struct buffered_status *status = store_status();
  joystick_set_active(status, 0, true);
}
