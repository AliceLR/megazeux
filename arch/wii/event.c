/* MegaZeux
 *
 * Copyright (C) 2008 Alan Williams <mralert@gmail.com>
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

#include <stdlib.h>
#include <stdio.h>

#define BOOL _BOOL
#include <gcutil.h>
#include <ogc/lwp.h>
#include <ogc/message.h>
#include <ogc/pad.h>
#include <ogc/ipc.h>
#include <ogc/video.h>
#include <ogc/usbmouse.h>
#include <wiikeyboard/keyboard.h>
// Use of anonymous union in wiiuse header
#pragma GCC diagnostic ignored "-pedantic"
#include <wiiuse/wpad.h>
#undef BOOL

#define PTR_BORDER_W 128
#define PTR_BORDER_H 70

#define USB_MOUSE_BTN_LEFT   0x01
#define USB_MOUSE_BTN_RIGHT  0x02
#define USB_MOUSE_BTN_MIDDLE 0x04

#define USB_MOUSE_BTN_MASK (USB_MOUSE_BTN_LEFT | USB_MOUSE_BTN_RIGHT | \
 USB_MOUSE_BTN_MIDDLE)

extern struct input_status input;

enum event_type
{
  EVENT_BUTTON_DOWN,
  EVENT_BUTTON_UP,
  EVENT_AXIS_MOVE,
  EVENT_CHANGE_EXT,
  EVENT_POINTER_MOVE,
  EVENT_POINTER_OUT,
  EVENT_KEY_DOWN,
  EVENT_KEY_UP,
  EVENT_KEY_LOCKS,
  EVENT_MOUSE_MOVE,
  EVENT_MOUSE_BUTTON_DOWN,
  EVENT_MOUSE_BUTTON_UP
};

struct button_event
{
  enum event_type type;
  Uint32 pad;
  Uint32 button;
};

struct axis_event
{
  enum event_type type;
  Uint32 pad;
  Uint32 axis;
  Sint16 pos;
};

struct ext_event
{
  enum event_type type;
  Uint32 pad;
  int ext;
};

struct pointer_event
{
  enum event_type type;
  Sint32 x;
  Sint32 y;
};

struct key_event
{
  enum event_type type;
  Uint32 key;
  Uint16 unicode;
};

struct locks_event
{
  enum event_type type;
  Uint16 locks;
};

struct mouse_move_event
{
  enum event_type type;
  Sint32 dx;
  Sint32 dy;
};

struct mouse_button_event
{
  enum event_type type;
  Uint32 button;
};

union event
{
  enum event_type type;
  struct button_event button;
  struct axis_event axis;
  struct ext_event ext;
  struct pointer_event pointer;
  struct key_event key;
  struct locks_event locks;
  struct mouse_move_event mmove;
  struct mouse_button_event mbutton;
};

#define STACKSIZE 8192
static u8 poll_stack[STACKSIZE];
static lwp_t poll_thread;

#define EVENT_QUEUE_SIZE 64
static mqbox_t eq;
static int eq_inited = 0;

static int pointing = 0;
static int ext_type[4] =
{
  WPAD_EXP_NONE, WPAD_EXP_NONE,
  WPAD_EXP_NONE, WPAD_EXP_NONE
};

static int write_eq(union event *ev)
{
  union event *new_ev;
  new_ev = malloc(sizeof(union event));
  if(!new_ev)
    return false;
  *new_ev = *ev;
  return MQ_Send(eq, (mqmsg_t)(&new_ev), MQ_MSG_NOBLOCK);
}

static int read_eq(union event *ev)
{
  mqmsg_t new_ev = NULL;
  if(MQ_Receive(eq, &new_ev, MQ_MSG_NOBLOCK))
  {
    *ev = *(union event *)new_ev;
    free(new_ev);
    return true;
  }
  else
    return false;
}

static void scan_buttons(Uint32 pad, Uint32 old_btns, Uint32 new_btns)
{
  Uint32 chg_btns;
  union event ev;
  int i;

  if(old_btns != new_btns)
  {
    chg_btns = old_btns ^ new_btns;
    ev.button.pad = pad;
    for(i = 1; i != 0; i <<= 1)
    {
      if(chg_btns & i)
      {
        ev.button.button = i;
        if(new_btns & i)
          ev.type = EVENT_BUTTON_DOWN;
        else
          ev.type = EVENT_BUTTON_UP;
        write_eq(&ev);
      }
    }
  }
}

static Sint16 adjust_axis(Uint8 pos, Uint8 min, Uint8 cen, Uint8 max)
{
  Sint32 temp;
  temp = (pos - cen) * 32767 / ((cen - min) + (max - cen) / 2);
  if(temp < -32767) temp = -32767;
  if(temp > 32767) temp = 32767;
  return temp;
}

static void scan_joystick(Uint32 pad, Uint32 xaxis, joystick_t js, Sint16 axes[])
{
  union event ev;
  Sint16 temp;

  ev.type = EVENT_AXIS_MOVE;
  ev.axis.pad = pad;
  temp = adjust_axis(js.pos.x, js.min.x, js.center.x, js.max.x);
  if(temp != axes[0])
  {
    ev.axis.axis = xaxis;
    ev.axis.pos = temp;
    write_eq(&ev);
    axes[0] = temp;
  }
  temp = -adjust_axis(js.pos.y, js.min.y, js.center.y, js.max.y);
  if(temp != axes[1])
  {
    ev.axis.axis = xaxis + 1;
    ev.axis.pos = temp;
    write_eq(&ev);
    axes[1] = temp;
  }
}

static void poll_input(void)
{
  static Sint32 old_x = 1000, old_y = 1000;
  static Uint32 old_point = 0;
  static Uint32 old_btns[4] = {0};
  static Sint16 old_axes[4][4] = {{0}};
  static int old_type[4] =
  {
    WPAD_EXP_NONE, WPAD_EXP_NONE,
    WPAD_EXP_NONE, WPAD_EXP_NONE
  };
  static Uint32 old_gcbtns[4] = {0};
  static Sint8 old_gcaxes[4][4] = {{0}};
  static Uint16 old_modifiers = 0;
  static Uint8 old_mousebtns = 0;

  WPADData *wd;
  PADStatus pad[4];
  keyboard_event ke;
  mouse_event me;
  u32 type;
  union event ev;
  Uint32 i, j;

  WPAD_ScanPads();
  for(i = 0; i < 4; i++)
  {
    if(WPAD_Probe(i, &type) == WPAD_ERR_NONE)
    {
      wd = WPAD_Data(i);
      if((int)type != old_type[i])
      {
        scan_buttons(i, old_btns[i], 0);
        old_btns[i] = 0;
        ev.type = EVENT_AXIS_MOVE;
        ev.axis.pad = i;
        ev.axis.pos = 0;
        for(j = 0; j < 4; j++)
        {
          if(old_axes[i][j])
          {
            ev.axis.axis = j;
            write_eq(&ev);
            old_axes[i][j] = 0;
          }
        }
        ev.type = EVENT_CHANGE_EXT;
        ev.ext.pad = i;
        old_type[i] = ev.ext.ext = type;
        write_eq(&ev);
      }
      if(i == 0)
      {
        if(wd->ir.valid)
        {
          ev.pointer.x = wd->ir.x - PTR_BORDER_W;
          ev.pointer.y = wd->ir.y - PTR_BORDER_H;
          if (ev.pointer.x < 0)
            ev.pointer.x = 0;
          if (ev.pointer.y < 0)
            ev.pointer.y = 0;
          if (ev.pointer.x >= 640)
            ev.pointer.x = 639;
          if (ev.pointer.y >= 350)
            ev.pointer.y = 349;
          if ((ev.pointer.x != old_x) || (ev.pointer.y != old_y))
          {
            ev.type = EVENT_POINTER_MOVE;
            write_eq(&ev);
            old_point = 1;
            old_x = ev.pointer.x;
            old_y = ev.pointer.y;
          }
        }
        else
        {
          if(old_point)
          {
            ev.type = EVENT_POINTER_OUT;
            write_eq(&ev);
            old_point = 0;
            old_x = old_y = 1000;
          }
        }
      }
      scan_buttons(i, old_btns[i], wd->btns_h);
      old_btns[i] = wd->btns_h;
      switch(type)
      {
        case WPAD_EXP_NUNCHUK:
        {
          scan_joystick(i, 0, wd->exp.nunchuk.js, old_axes[i]);
          break;
        }
        case WPAD_EXP_CLASSIC:
        {
          scan_joystick(i, 0, wd->exp.classic.ljs, old_axes[i]);
          scan_joystick(i, 2, wd->exp.classic.rjs, old_axes[i] + 2);
          break;
        }
        case WPAD_EXP_GUITARHERO3:
        {
          scan_joystick(i, 0, wd->exp.gh3.js, old_axes[i]);
          j = (int)(wd->exp.gh3.whammy_bar * 32767.0);
          if((int)j != old_axes[i][2])
          {
            ev.type = EVENT_AXIS_MOVE;
            ev.axis.pad = i;
            ev.axis.pos = j;
            write_eq(&ev);
          }
          break;
        }
        default: break;
      }
    }
    else
    {
      scan_buttons(i, old_btns[i], 0);
      old_btns[i] = 0;
      ev.type = EVENT_AXIS_MOVE;
      ev.axis.pad = i;
      ev.axis.pos = 0;
      for(j = 0; j < 4; j++)
      {
        if(old_axes[i][j])
        {
          ev.axis.axis = j;
          write_eq(&ev);
          old_axes[i][j] = 0;
        }
      }
    }
  }

  PAD_Read(pad);
  for(i = 0; i < 4; i++)
  {
    if(pad[i].err == PAD_ERR_NONE)
    {
      scan_buttons(i + 4, old_gcbtns[i], pad[i].button);
      old_gcbtns[i] = pad[i].button;
      ev.type = EVENT_AXIS_MOVE;
      ev.axis.pad = i + 4;
      if(pad[i].stickX != old_gcaxes[i][0])
      {
        ev.axis.axis = 0;
        ev.axis.pos = pad[i].stickX << 8;
        write_eq(&ev);
        old_gcaxes[i][0] = pad[i].stickX;
      }
      if(pad[i].stickY != old_gcaxes[i][1])
      {
        ev.axis.axis = 1;
        ev.axis.pos = -(pad[i].stickY << 8);
        write_eq(&ev);
        old_gcaxes[i][1] = pad[i].stickY;
      }
      if(pad[i].substickX != old_gcaxes[i][2])
      {
        ev.axis.axis = 2;
        ev.axis.pos = pad[i].substickX << 8;
        write_eq(&ev);
        old_gcaxes[i][0] = pad[i].substickX;
      }
      if(pad[i].substickY != old_gcaxes[i][3])
      {
        ev.axis.axis = 3;
        ev.axis.pos = -(pad[i].substickY << 8);
        write_eq(&ev);
        old_gcaxes[i][1] = pad[i].substickY;
      }
    }
    else
    {
      scan_buttons(i + 4, old_gcbtns[i], 0);
      old_gcbtns[i] = 0;
      ev.type = EVENT_AXIS_MOVE;
      ev.axis.pad = i + 4;
      ev.axis.pos = 0;
      for(j = 0; j < 4; j++)
      {
        if(old_gcaxes[i][j])
        {
          ev.axis.axis = j;
          write_eq(&ev);
          old_gcaxes[i][j] = 0;
        }
      }
    }
  }

  while(KEYBOARD_GetEvent(&ke))
  {
    ke.modifiers &= MOD_CAPSLOCK | MOD_NUMLOCK;
    if(ke.modifiers != old_modifiers)
    {
      ev.type = EVENT_KEY_LOCKS;
      ev.locks.locks = ke.modifiers;
      write_eq(&ev);
      old_modifiers = ke.modifiers;
    }
    ev.key.key = ke.keycode;
    // Non-character keys mapped to the private use area
    if((ke.symbol >= 0xE000) && (ke.symbol < 0xF900))
      ev.key.unicode = 0;
    else
      ev.key.unicode = ke.symbol;
    switch(ke.type)
    {
      default:
      case KEYBOARD_CONNECTED:
      case KEYBOARD_DISCONNECTED:
        break;
      case KEYBOARD_PRESSED:
        ev.type = EVENT_KEY_DOWN;
        write_eq(&ev);
        break;
      case KEYBOARD_RELEASED:
        ev.type = EVENT_KEY_UP;
        write_eq(&ev);
        break;
    }
  }

  ev.type = EVENT_MOUSE_MOVE;
  ev.mmove.dx = ev.mmove.dy = 0;
  while(MOUSE_GetEvent(&me))
  {
    ev.mmove.dx += me.rx;
    ev.mmove.dy += me.ry;
    me.button &= USB_MOUSE_BTN_MASK;
    if(me.button != old_mousebtns)
    {
      if(ev.mmove.dx || ev.mmove.dy)
        write_eq(&ev);
      for(i = 1; i <= (me.button | old_mousebtns); i <<= 1)
      {
        if(i & (me.button ^ old_mousebtns))
        {
          if(i & me.button)
            ev.type = EVENT_MOUSE_BUTTON_DOWN;
          else
            ev.type = EVENT_MOUSE_BUTTON_UP;
          ev.mbutton.button = i;
          write_eq(&ev);
        }
      }
      old_mousebtns = me.button;
      ev.type = EVENT_MOUSE_MOVE;
      ev.mmove.dx = ev.mmove.dy = 0;
    }
  }
  if(ev.mmove.dx || ev.mmove.dy)
    write_eq(&ev);
}

static void *wii_poll_thread(void *dud)
{
  WPAD_Init();
  WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS);
  WPAD_SetDataFormat(0, WPAD_FMT_BTNS_ACC_IR);
  WPAD_SetVRes(0, 640 + PTR_BORDER_W * 2, 350 + PTR_BORDER_H * 2);

  PAD_Init();

  KEYBOARD_Init(NULL);
  MOUSE_Init();

  while(1)
  {
    VIDEO_WaitVSync();
    poll_input();
  }

  return 0;
}

static Uint32 map_button(Uint32 pad, Uint32 button)
{
  Uint32 rval = 256;
  if(pad < 4)
  {
    switch(button)
    {
      case WPAD_BUTTON_A: rval = 0; break;
      case WPAD_BUTTON_B: rval = 1; break;
      case WPAD_BUTTON_1: rval = 2; break;
      case WPAD_BUTTON_2: rval = 3; break;
      case WPAD_BUTTON_LEFT: rval = 4; break;
      case WPAD_BUTTON_RIGHT: rval = 5; break;
      case WPAD_BUTTON_UP: rval = 6; break;
      case WPAD_BUTTON_DOWN: rval = 7; break;
      case WPAD_BUTTON_MINUS: rval = 8; break;
      case WPAD_BUTTON_PLUS: rval = 9; break;
      default: break;
    }

    switch(ext_type[pad])
    {
      case WPAD_EXP_NONE: break;
      case WPAD_EXP_NUNCHUK:
      {
        switch(button)
        {
          case WPAD_NUNCHUK_BUTTON_C: rval = 10; break;
          case WPAD_NUNCHUK_BUTTON_Z: rval = 11; break;
          default: break;
        }
        rval += 10;
        break;
      }
      case WPAD_EXP_CLASSIC:
      {
        switch(button)
        {
          case WPAD_CLASSIC_BUTTON_A: rval = 10; break;
          case WPAD_CLASSIC_BUTTON_B: rval = 11; break;
          case WPAD_CLASSIC_BUTTON_X: rval = 12; break;
          case WPAD_CLASSIC_BUTTON_Y: rval = 13; break;
          case WPAD_CLASSIC_BUTTON_FULL_L: rval = 14; break;
          case WPAD_CLASSIC_BUTTON_FULL_R: rval = 15; break;
          case WPAD_CLASSIC_BUTTON_ZL: rval = 16; break;
          case WPAD_CLASSIC_BUTTON_ZR: rval = 17; break;
          case WPAD_CLASSIC_BUTTON_LEFT: rval = 18; break;
          case WPAD_CLASSIC_BUTTON_RIGHT: rval = 19; break;
          case WPAD_CLASSIC_BUTTON_UP: rval = 20; break;
          case WPAD_CLASSIC_BUTTON_DOWN: rval = 21; break;
          case WPAD_CLASSIC_BUTTON_MINUS: rval = 22; break;
          case WPAD_CLASSIC_BUTTON_PLUS: rval = 23; break;
          default: break;
        }
        rval += 22;
        break;
      }
      case WPAD_EXP_GUITARHERO3:
      {
        switch(button)
        {
          case WPAD_GUITAR_HERO_3_BUTTON_GREEN: rval = 10; break;
          case WPAD_GUITAR_HERO_3_BUTTON_RED: rval = 11; break;
          case WPAD_GUITAR_HERO_3_BUTTON_YELLOW: rval = 12; break;
          case WPAD_GUITAR_HERO_3_BUTTON_BLUE: rval = 13; break;
          case WPAD_GUITAR_HERO_3_BUTTON_ORANGE: rval = 14; break;
          case WPAD_GUITAR_HERO_3_BUTTON_STRUM_UP: rval = 15; break;
          case WPAD_GUITAR_HERO_3_BUTTON_STRUM_DOWN: rval = 16; break;
          case WPAD_GUITAR_HERO_3_BUTTON_MINUS: rval = 17; break;
          case WPAD_GUITAR_HERO_3_BUTTON_PLUS: rval = 18; break;
          default: break;
        }
        rval += 46;
        break;
      }
      default:
        rval = 256; // Unsupported extension controller
        break;
    }
  }
  else
  {
    switch(button)
    {
      case PAD_BUTTON_A: rval = 0; break;
      case PAD_BUTTON_B: rval = 1; break;
      case PAD_BUTTON_X: rval = 2; break;
      case PAD_BUTTON_Y: rval = 3; break;
      case PAD_TRIGGER_L: rval = 4; break;
      case PAD_TRIGGER_R: rval = 5; break;
      case PAD_TRIGGER_Z: rval = 6; break;
      case PAD_BUTTON_LEFT: rval = 7; break;
      case PAD_BUTTON_RIGHT: rval = 8; break;
      case PAD_BUTTON_UP: rval = 9; break;
      case PAD_BUTTON_DOWN: rval = 10; break;
      case PAD_BUTTON_START: rval = 11; break;
      default: break;
    }
  }

  if (rval < 256)
    return rval;
  else
    return 256;
}

static enum keycode convert_USB_internal(Uint32 key)
{
  switch(key)
  {
    case 0x04: return IKEY_a;
    case 0x05: return IKEY_b;
    case 0x06: return IKEY_c;
    case 0x07: return IKEY_d;
    case 0x08: return IKEY_e;
    case 0x09: return IKEY_f;
    case 0x0A: return IKEY_g;
    case 0x0B: return IKEY_h;
    case 0x0C: return IKEY_i;
    case 0x0D: return IKEY_j;
    case 0x0E: return IKEY_k;
    case 0x0F: return IKEY_l;
    case 0x10: return IKEY_m;
    case 0x11: return IKEY_n;
    case 0x12: return IKEY_o;
    case 0x13: return IKEY_p;
    case 0x14: return IKEY_q;
    case 0x15: return IKEY_r;
    case 0x16: return IKEY_s;
    case 0x17: return IKEY_t;
    case 0x18: return IKEY_u;
    case 0x19: return IKEY_v;
    case 0x1A: return IKEY_w;
    case 0x1B: return IKEY_x;
    case 0x1C: return IKEY_y;
    case 0x1D: return IKEY_z;
    case 0x1E: return IKEY_1;
    case 0x1F: return IKEY_2;
    case 0x20: return IKEY_3;
    case 0x21: return IKEY_4;
    case 0x22: return IKEY_5;
    case 0x23: return IKEY_6;
    case 0x24: return IKEY_7;
    case 0x25: return IKEY_8;
    case 0x26: return IKEY_9;
    case 0x27: return IKEY_0;
    case 0x28: return IKEY_RETURN;
    case 0x29: return IKEY_ESCAPE;
    case 0x2A: return IKEY_BACKSPACE;
    case 0x2B: return IKEY_TAB;
    case 0x2C: return IKEY_SPACE;
    case 0x2D: return IKEY_MINUS;
    case 0x2E: return IKEY_EQUALS;
    case 0x2F: return IKEY_LEFTBRACKET;
    case 0x30: return IKEY_RIGHTBRACKET;
    case 0x31: return IKEY_BACKSLASH;
    case 0x33: return IKEY_SEMICOLON;
    case 0x34: return IKEY_QUOTE;
    case 0x35: return IKEY_BACKQUOTE;
    case 0x36: return IKEY_COMMA;
    case 0x37: return IKEY_PERIOD;
    case 0x38: return IKEY_SLASH;
    case 0x39: return IKEY_CAPSLOCK;
    case 0x3A: return IKEY_F1;
    case 0x3B: return IKEY_F2;
    case 0x3C: return IKEY_F3;
    case 0x3D: return IKEY_F4;
    case 0x3E: return IKEY_F5;
    case 0x3F: return IKEY_F6;
    case 0x40: return IKEY_F7;
    case 0x41: return IKEY_F8;
    case 0x42: return IKEY_F9;
    case 0x43: return IKEY_F10;
    case 0x44: return IKEY_F11;
    case 0x45: return IKEY_F12;
    case 0x46: return IKEY_SYSREQ;
    case 0x47: return IKEY_SCROLLOCK;
    case 0x48: return IKEY_BREAK;
    case 0x49: return IKEY_INSERT;
    case 0x4A: return IKEY_HOME;
    case 0x4B: return IKEY_PAGEUP;
    case 0x4C: return IKEY_DELETE;
    case 0x4D: return IKEY_END;
    case 0x4E: return IKEY_PAGEDOWN;
    case 0x4F: return IKEY_RIGHT;
    case 0x50: return IKEY_LEFT;
    case 0x51: return IKEY_DOWN;
    case 0x52: return IKEY_UP;
    case 0x53: return IKEY_NUMLOCK;
    case 0x54: return IKEY_KP_DIVIDE;
    case 0x55: return IKEY_KP_MULTIPLY;
    case 0x56: return IKEY_KP_MINUS;
    case 0x57: return IKEY_KP_PLUS;
    case 0x58: return IKEY_KP_ENTER;
    case 0x59: return IKEY_KP1;
    case 0x5A: return IKEY_KP2;
    case 0x5B: return IKEY_KP3;
    case 0x5C: return IKEY_KP4;
    case 0x5D: return IKEY_KP5;
    case 0x5E: return IKEY_KP6;
    case 0x5F: return IKEY_KP7;
    case 0x60: return IKEY_KP8;
    case 0x61: return IKEY_KP9;
    case 0x62: return IKEY_KP0;
    case 0x63: return IKEY_KP_PERIOD;
    case 0x65: return IKEY_MENU;
    case 0xE0: return IKEY_LCTRL;
    case 0xE1: return IKEY_LSHIFT;
    case 0xE2: return IKEY_LALT;
    case 0xE3: return IKEY_LSUPER;
    case 0xE4: return IKEY_RCTRL;
    case 0xE5: return IKEY_RSHIFT;
    case 0xE6: return IKEY_RALT;
    case 0xE7: return IKEY_RSUPER;
    default: return IKEY_UNKNOWN;
  }
}

static bool process_event(union event *ev)
{
  struct buffered_status *status = store_status();
  bool rval = true;

  switch(ev->type)
  {
    case EVENT_BUTTON_DOWN:
    {
      Uint32 button = map_button(ev->button.pad, ev->button.button);
      rval = false;
      if((ev->button.pad == 0) && pointing)
      {
        Uint32 mousebutton;
        switch(ev->button.button)
        {
          case WPAD_BUTTON_A: mousebutton = MOUSE_BUTTON_LEFT; break;
          case WPAD_BUTTON_B: mousebutton = MOUSE_BUTTON_RIGHT; break;
          default: mousebutton = 0; break;
        }
        if(mousebutton)
        {
          status->mouse_button = mousebutton;
          status->mouse_repeat = mousebutton;
          status->mouse_button_state |= MOUSE_BUTTON(mousebutton);
          status->mouse_repeat_state = 1;
          status->mouse_drag_state = -1;
          status->mouse_time = get_ticks();
          button = 256;
          rval = true;
        }
      }
      if((button < 256))
      {
        enum keycode skey = input.joystick_button_map[ev->button.pad][button];
        if(skey && (status->keymap[skey] == 0))
        {
          key_press(status, skey, skey);
          rval = true;
        }
      }
      else
      {
        if((ev->button.pad < 4) && ((ev->button.button == WPAD_BUTTON_HOME) ||
         ((ext_type[ev->button.pad] == WPAD_EXP_CLASSIC) &&
         (ev->button.button == WPAD_CLASSIC_BUTTON_HOME))))
        {
          status->keymap[IKEY_ESCAPE] = 1;
          status->key = IKEY_ESCAPE;
          status->keypress_time = get_ticks();
          rval = true;
          break;
        }
      }
      break;
    }
    case EVENT_BUTTON_UP:
    {
      Uint32 button = map_button(ev->button.pad, ev->button.button);
      rval = false;
      if((ev->button.pad == 0) && status->mouse_button_state)
      {
        Uint32 mousebutton;
        switch(ev->button.button)
        {
          case WPAD_BUTTON_A: mousebutton = MOUSE_BUTTON_LEFT; break;
          case WPAD_BUTTON_B: mousebutton = MOUSE_BUTTON_RIGHT; break;
          default: mousebutton = 0; break;
        }
        if(mousebutton &&
         (status->mouse_button_state & MOUSE_BUTTON(mousebutton)))
        {
          status->mouse_button_state &= ~MOUSE_BUTTON(mousebutton);
          status->mouse_repeat = 0;
          status->mouse_drag_state = 0;
          status->mouse_repeat_state = 0;
          button = 256;
          rval = true;
        }
      }
      if((button < 256))
      {
        enum keycode skey = input.joystick_button_map[ev->button.pad][button];
        if(skey)
        {
          key_release(status, skey);
          rval = true;
        }
      }
      break;
    }
    case EVENT_AXIS_MOVE:
    {
      int digital_value = -1;
      int axis = ev->axis.axis;
      int last_axis;
      enum keycode skey;
      if(ev->axis.pad < 4)
      {
        switch(ext_type[ev->axis.pad])
        {
          case WPAD_EXP_NUNCHUK: break;
          case WPAD_EXP_CLASSIC: axis += 2; break;
          case WPAD_EXP_GUITARHERO3: axis += 6; break;
          default: axis = 256; break; // Not supposed to happen
        }
      }
      if(axis == 256) break;
      last_axis = status->axis[ev->axis.pad][axis];

      if(ev->axis.pos > 10000)
        digital_value = 1;
      else if(ev->axis.pos < -10000)
        digital_value = 0;

      if(digital_value != -1)
      {
        skey = input.joystick_axis_map[ev->axis.pad][axis][digital_value];
        if(skey)
        {
          if(status->keymap[skey] == 0)
            key_press(status, skey, skey);

          if(last_axis == (digital_value ^ 1))
          {
            skey = input.joystick_axis_map[ev->axis.pad][axis][last_axis];
            key_release(status, skey);
          }
        }
      }
      else
      {
        if(last_axis != -1)
        {
          skey = input.joystick_axis_map[ev->axis.pad][axis][last_axis];
          if(skey)
            key_release(status, skey);
        }
      }
      status->axis[ev->axis.pad][axis] = digital_value;
      break;
    }
    case EVENT_CHANGE_EXT:
    {
      ext_type[ev->ext.pad] = ev->ext.ext;
      break;
    }
    case EVENT_POINTER_MOVE:
    {
      pointing = 1;
      status->mouse_moved = true;
      status->real_mouse_x = ev->pointer.x;
      status->real_mouse_y = ev->pointer.y;
      status->mouse_x = ev->pointer.x / 8;
      status->mouse_y = ev->pointer.y / 14;
      break;
    }
    case EVENT_POINTER_OUT:
    {
      pointing = 0;
      break;
    }
    case EVENT_KEY_DOWN:
    {
      enum keycode ckey = convert_USB_internal(ev->key.key);
      if(!ckey)
      {
        if(ev->key.unicode)
          ckey = IKEY_UNICODE;
        else
        {
          rval = false;
          break;
        }
      }

      if((ckey == IKEY_RETURN) &&
       get_alt_status(keycode_internal) &&
       get_ctrl_status(keycode_internal))
      {
        toggle_fullscreen();
        break;
      }

      if(ckey == IKEY_F12)
      {
        dump_screen();
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

      key_press(status, ckey, ev->key.unicode);
      break;
    }
    case EVENT_KEY_UP:
    {
      enum keycode ckey = convert_USB_internal(ev->key.key);
      if(!ckey)
      {
        if(status->keymap[IKEY_UNICODE])
          ckey = IKEY_UNICODE;
        else
        {
          rval = false;
          break;
        }
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
    case EVENT_KEY_LOCKS:
    {
      status->numlock_status = !!(ev->locks.locks & MOD_NUMLOCK);
      status->caps_status = !!(ev->locks.locks & MOD_CAPSLOCK);
      break;
    }
    case EVENT_MOUSE_MOVE:
    {
      int mx = status->real_mouse_x + ev->mmove.dx;
      int my = status->real_mouse_y + ev->mmove.dy;

      if(mx < 0)
        mx = 0;
      if(my < 0)
        my = 0;
      if(mx >= 640)
        mx = 639;
      if(my >= 350)
        my = 349;

      status->real_mouse_x = mx;
      status->real_mouse_y = my;
      status->mouse_x = mx / 8;
      status->mouse_y = my / 14;
      status->mouse_moved = true;
      break;
    }
    case EVENT_MOUSE_BUTTON_DOWN:
    {
      Uint32 button = 0;
      switch (ev->mbutton.button)
      {
        case USB_MOUSE_BTN_LEFT:
          button = MOUSE_BUTTON_LEFT;
          break;
        case USB_MOUSE_BTN_RIGHT:
          button = MOUSE_BUTTON_RIGHT;
          break;
        case USB_MOUSE_BTN_MIDDLE:
          button = MOUSE_BUTTON_MIDDLE;
          break;
        default:
          break;
      }

      if(!button)
        break;

      status->mouse_button = button;
      status->mouse_repeat = button;
      status->mouse_button_state |= MOUSE_BUTTON(button);
      status->mouse_repeat_state = 1;
      status->mouse_drag_state = -1;
      status->mouse_time = get_ticks();
      break;
    }
    case EVENT_MOUSE_BUTTON_UP:
    {
      Uint32 button = 0;
      switch (ev->mbutton.button)
      {
        case USB_MOUSE_BTN_LEFT:
          button = MOUSE_BUTTON_LEFT;
          break;
        case USB_MOUSE_BTN_RIGHT:
          button = MOUSE_BUTTON_RIGHT;
          break;
        case USB_MOUSE_BTN_MIDDLE:
          button = MOUSE_BUTTON_MIDDLE;
          break;
        default:
          break;
      }

      if(!button)
        break;

      status->mouse_button_state &= ~MOUSE_BUTTON(button);
      status->mouse_repeat = 0;
      status->mouse_drag_state = 0;
      status->mouse_repeat_state = 0;
      break;
    }
    default:
    {
      rval = false;
      break;
    }
  }

  return rval;
}

bool __update_event_status(void)
{
  bool rval = false;
  union event ev;

  if(!eq_inited)
    return false;

  while(read_eq(&ev))
    rval |= process_event(&ev);

  return rval;
}

void __wait_event(void)
{
  mqmsg_t ev;

  if(!eq_inited)
    return;

  if(MQ_Receive(eq, &ev, MQ_MSG_BLOCK))
  {
    process_event((union event *)ev);
    free(ev);
  }
}

void real_warp_mouse(Uint32 x, Uint32 y)
{
  // Mouse warping doesn't work too well with the Wiimote
}

void initialize_joysticks(void)
{
  MQ_Init(&eq, EVENT_QUEUE_SIZE);
  eq_inited = 1;
  LWP_CreateThread(&poll_thread, wii_poll_thread, NULL, poll_stack, STACKSIZE,
   40);
}
