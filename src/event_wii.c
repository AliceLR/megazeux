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

#include <stdlib.h>
#include <stdio.h>

#define BOOL _BOOL
#include <gcutil.h>
#include <ogc/lwp.h>
#include <ogc/message.h>
#include <ogc/pad.h>
#include <ogc/ipc.h>
#include <ogc/video.h>
#include <wiiuse/wpad.h>
#undef BOOL

enum event_type
{
  EVENT_BUTTON_DOWN,
  EVENT_BUTTON_UP,
  EVENT_AXIS_MOVE,
  EVENT_CHANGE_EXT,
  EVENT_POINTER_MOVE,
  EVENT_POINTER_OUT,
  EVENT_KEY_DOWN,
  EVENT_KEY_UP
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
  Uint32 x;
  Uint32 y;
};

struct key_event
{
  enum event_type type;
  Uint32 key;
};

union event
{
  enum event_type type;
  struct button_event button;
  struct axis_event axis;
  struct ext_event ext;
  struct pointer_event pointer;
  struct key_event key;
};

#define STACKSIZE 8192
static u8 poll_stack[STACKSIZE];
static lwp_t poll_thread;

static u8 kbd_stack[STACKSIZE];
static lwp_t kbd_thread;

#define EVENT_QUEUE_SIZE 64
static mqbox_t eq;

static int pointing = 0;
static int ext_type[4] =
{
  WPAD_EXP_NONE, WPAD_EXP_NONE,
  WPAD_EXP_NONE, WPAD_EXP_NONE
};

static s32 kbd_fd;
static Uint8 kbd_buf[16] ATTRIBUTE_ALIGN(32);
static char kbd_fs[] ATTRIBUTE_ALIGN(32) = "/dev/usb/kbd";

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
  static Uint32 old_x = 1000, old_y = 1000;
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

  WPADData *wd;
  PADStatus pad[4];
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
        if(wd->ir.valid && ((old_x != wd->ir.x) || (old_y != wd->ir.y)))
        {
          ev.type = EVENT_POINTER_MOVE;
          old_x = ev.pointer.x = (int)wd->ir.x;
          old_y = ev.pointer.y = (int)wd->ir.y;
          write_eq(&ev);
          old_point = 1;
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
}

static void *wii_kbd_thread(void *dud)
{
  static Uint8 old_mods = 0;
  static Uint8 old_keys[6] = {0};

  union event ev;
  int i, j, chg;

  delay(1000);

  kbd_fd = IOS_Open(kbd_fs, IPC_OPEN_RW);

  if(kbd_fd >= 0)
  {
    while(1)
    {
      if (IOS_Ioctl(kbd_fd, 0, NULL, 0, kbd_buf, 16) >= 0)
      {
        switch(kbd_buf[3])
        {
          case 0:
          case 1:
          {
            ev.type = EVENT_KEY_UP;
            for(i = 0; i < 6; i++)
            {
              if(old_keys[i] != 0)
              {
                ev.key.key = old_keys[i];
                write_eq(&ev);
                old_keys[i] = 0;
              }
            }
            for(i = 0; i < 8; i++)
            {
              if(old_mods & (1 << i))
              {
                ev.key.key = 0xE0 + i;
                write_eq(&ev);
              }
            }
            old_mods = 0;
            break;
          }
          case 2:
          {
            ev.type = EVENT_KEY_UP;
            for(i = 0; i < 6; i++)
            {
              if(old_keys[i])
              {
                for(j = 10; j < 16; j++)
                {
                  if(old_keys[i] == kbd_buf[j])
                    break;
                }
                if(j == 16)
                {
                  ev.key.key = old_keys[i];
                  write_eq(&ev);
                }
              }
            }
            ev.type = EVENT_KEY_DOWN;
            for(i = 10; i < 16; i++)
            {
              if(kbd_buf[i])
              {
                for(j = 0; j < 6; j++)
                {
                  if(kbd_buf[i] == old_keys[j])
                    break;
                }
                if(j == 6)
                {
                  ev.key.key = kbd_buf[i];
                  write_eq(&ev);
                }
              }
            }
            for(i = 0; i < 6; i++)
              old_keys[i] = kbd_buf[i + 10];
            chg = old_mods ^ kbd_buf[8];
            for(i = 0; i < 8; i++)
            {
              if(chg & (1 << i))
              {
                if(old_mods & (1 << i))
                  ev.type = EVENT_KEY_UP;
                else
                  ev.type = EVENT_KEY_DOWN;
                ev.key.key = 0xE0 + i;
                write_eq(&ev);
              }
            }
            old_mods = kbd_buf[8];
            break;
          }
          default: break;
        }
      }
    }
  }

  return 0;
}

static void *wii_poll_thread(void *dud)
{
  WPAD_Init();
  WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS);
  WPAD_SetDataFormat(0, WPAD_FMT_BTNS_ACC_IR);
  WPAD_SetVRes(0, 640, 350);

  PAD_Init();

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

#define UNICODE_SHIFT(A, B) \
  ((input.keymap[IKEY_LSHIFT] || input.keymap[IKEY_RSHIFT]) ? B : A)
#define UNICODE_CAPS(A, B) \
  (input.caps_status ? UNICODE_SHIFT(B, A) : UNICODE_SHIFT(A, B))
#define UNICODE_NUM(A, B) \
  (input.numlock_status ? B : A)

// TODO: Support non-US keyboard layouts
static Uint32 convert_internal_unicode(enum keycode key)
{
  switch(key)
  {
    case IKEY_SPACE: return ' ';
    case IKEY_QUOTE: return UNICODE_SHIFT('\'', '"');
    case IKEY_COMMA: return UNICODE_SHIFT(',', '<');
    case IKEY_MINUS: return UNICODE_SHIFT('-', '_');
    case IKEY_PERIOD: return UNICODE_SHIFT('.', '>');
    case IKEY_SLASH: return UNICODE_SHIFT('/', '?');
    case IKEY_0: return UNICODE_SHIFT('0', ')');
    case IKEY_1: return UNICODE_SHIFT('1', '!');
    case IKEY_2: return UNICODE_SHIFT('2', '@');
    case IKEY_3: return UNICODE_SHIFT('3', '#');
    case IKEY_4: return UNICODE_SHIFT('4', '$');
    case IKEY_5: return UNICODE_SHIFT('5', '%');
    case IKEY_6: return UNICODE_SHIFT('6', '^');
    case IKEY_7: return UNICODE_SHIFT('7', '&');
    case IKEY_8: return UNICODE_SHIFT('8', '*');
    case IKEY_9: return UNICODE_SHIFT('9', '(');
    case IKEY_SEMICOLON: return UNICODE_SHIFT(';', ':');
    case IKEY_EQUALS: return UNICODE_SHIFT('=', '+');
    case IKEY_LEFTBRACKET: return UNICODE_SHIFT('[', '{');
    case IKEY_BACKSLASH: return UNICODE_SHIFT('\\', '|');
    case IKEY_RIGHTBRACKET: return UNICODE_SHIFT(']', '}');
    case IKEY_BACKQUOTE: return UNICODE_SHIFT('`', '~');
    case IKEY_a: return UNICODE_CAPS('a', 'A');
    case IKEY_b: return UNICODE_CAPS('b', 'B');
    case IKEY_c: return UNICODE_CAPS('c', 'C');
    case IKEY_d: return UNICODE_CAPS('d', 'D');
    case IKEY_e: return UNICODE_CAPS('e', 'E');
    case IKEY_f: return UNICODE_CAPS('f', 'F');
    case IKEY_g: return UNICODE_CAPS('g', 'G');
    case IKEY_h: return UNICODE_CAPS('h', 'H');
    case IKEY_i: return UNICODE_CAPS('i', 'I');
    case IKEY_j: return UNICODE_CAPS('j', 'J');
    case IKEY_k: return UNICODE_CAPS('k', 'K');
    case IKEY_l: return UNICODE_CAPS('l', 'L');
    case IKEY_m: return UNICODE_CAPS('m', 'M');
    case IKEY_n: return UNICODE_CAPS('n', 'N');
    case IKEY_o: return UNICODE_CAPS('o', 'O');
    case IKEY_p: return UNICODE_CAPS('p', 'P');
    case IKEY_q: return UNICODE_CAPS('q', 'Q');
    case IKEY_r: return UNICODE_CAPS('r', 'R');
    case IKEY_s: return UNICODE_CAPS('s', 'S');
    case IKEY_t: return UNICODE_CAPS('t', 'T');
    case IKEY_u: return UNICODE_CAPS('u', 'U');
    case IKEY_v: return UNICODE_CAPS('v', 'V');
    case IKEY_w: return UNICODE_CAPS('w', 'W');
    case IKEY_x: return UNICODE_CAPS('x', 'X');
    case IKEY_y: return UNICODE_CAPS('y', 'Y');
    case IKEY_z: return UNICODE_CAPS('z', 'Z');
    case IKEY_KP0: return UNICODE_NUM(0, '0');
    case IKEY_KP1: return UNICODE_NUM(0, '1');
    case IKEY_KP2: return UNICODE_NUM(0, '2');
    case IKEY_KP3: return UNICODE_NUM(0, '3');
    case IKEY_KP4: return UNICODE_NUM(0, '4');
    case IKEY_KP5: return UNICODE_NUM(0, '5');
    case IKEY_KP6: return UNICODE_NUM(0, '6');
    case IKEY_KP7: return UNICODE_NUM(0, '7');
    case IKEY_KP8: return UNICODE_NUM(0, '8');
    case IKEY_KP9: return UNICODE_NUM(0, '9');
    case IKEY_KP_PERIOD: return UNICODE_NUM(0, '.');
    case IKEY_KP_DIVIDE: return '/';
    case IKEY_KP_MULTIPLY: return '*';
    case IKEY_KP_MINUS: return '-';
    case IKEY_KP_PLUS: return '+';
    default: return 0;
  }
}

static Uint32 process_event(union event *ev)
{
  Uint32 rval = 1;

  switch(ev->type)
  {
    case EVENT_BUTTON_DOWN:
    {
      Uint32 button = map_button(ev->button.pad, ev->button.button);
      rval = 0;
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
          input.last_mouse_button = mousebutton;
          input.last_mouse_repeat = mousebutton;
          input.mouse_button_state |= MOUSE_BUTTON(mousebutton);
          input.last_mouse_repeat_state = 1;
          input.mouse_drag_state = -1;
          input.last_mouse_time = get_ticks();
          button = 256;
          rval = 1;
        }
      }
      if((button < 256))
      {
        enum keycode skey = input.joystick_button_map[ev->button.pad][button];
        if(skey && (input.keymap[skey] == 0))
        {
          input.last_key_pressed = skey;
          input.last_key = skey;
          input.last_unicode = skey;
          input.last_key_repeat = skey;
          input.last_unicode_repeat = skey;
          input.keymap[skey] = 1;
          input.last_keypress_time = get_ticks();
          input.last_key_release = IKEY_UNKNOWN;
          rval = 1;
        }
      }
      else
      {
        if((ev->button.pad < 4) && ((ev->button.button == WPAD_BUTTON_HOME) ||
         ((ext_type[ev->button.pad] == WPAD_EXP_CLASSIC) &&
         (ev->button.button == WPAD_CLASSIC_BUTTON_HOME))))
        {
          input.keymap[IKEY_ESCAPE] = 1;
          input.last_key = IKEY_ESCAPE;
          input.last_keypress_time = get_ticks();
          rval = 1;
          break;
        }
      }
      break;
    }
    case EVENT_BUTTON_UP:
    {
      Uint32 button = map_button(ev->button.pad, ev->button.button);
      rval = 0;
      if((ev->button.pad == 0) && input.mouse_button_state)
      {
        Uint32 mousebutton;
        switch(ev->button.button)
        {
          case WPAD_BUTTON_A: mousebutton = MOUSE_BUTTON_LEFT; break;
          case WPAD_BUTTON_B: mousebutton = MOUSE_BUTTON_RIGHT; break;
          default: mousebutton = 0; break;
        }
        if(mousebutton &&
         (input.mouse_button_state & MOUSE_BUTTON(mousebutton)))
        {
          input.mouse_button_state &= ~MOUSE_BUTTON(mousebutton);
          input.last_mouse_repeat = 0;
          input.mouse_drag_state = 0;
          input.last_mouse_repeat_state = 0;
          button = 256;
          rval = 1;
        }
      }
      if((button < 256))
      {
        enum keycode skey = input.joystick_button_map[ev->button.pad][button];
        if(skey)
        {
          input.keymap[skey] = 0;
          input.last_key_repeat = IKEY_UNKNOWN;
          input.last_unicode_repeat = 0;
          input.last_key_release = skey;
          rval = 1;
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
      last_axis = input.last_axis[ev->axis.pad][axis];

      if(ev->axis.pos > 10000)
        digital_value = 1;
      else
        if(ev->axis.pos < -10000)
          digital_value = 0;

      if(digital_value != -1)
      {
        skey = input.joystick_axis_map[ev->axis.pad][axis][digital_value];
        if(skey)
        {
          if(input.keymap[skey] == 0)
          {
            input.last_key_pressed = skey;
            input.last_key = skey;
            input.last_unicode = skey;
            input.last_key_repeat = skey;
            input.last_unicode_repeat = skey;
            input.keymap[skey] = 1;
            input.last_keypress_time = get_ticks();
            input.last_key_release = IKEY_UNKNOWN;
          }
          if(last_axis == (digital_value ^ 1))
          {
            skey = input.joystick_axis_map[ev->axis.pad][axis][last_axis];
            input.keymap[skey] = 0;
            input.last_key_repeat = IKEY_UNKNOWN;
            input.last_unicode_repeat = 0;
            input.last_key_release = skey;
          }
        }
      }
      else
      {
        if(last_axis != -1)
        {
          skey = input.joystick_axis_map[ev->axis.pad][axis][last_axis];
          if(skey)
          {
            input.keymap[skey] = 0;
            input.last_key_repeat = IKEY_UNKNOWN;
            input.last_unicode_repeat = 0;
            input.last_key_release = skey;
          }
        }
      }
      input.last_axis[ev->axis.pad][axis] = digital_value;
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
      input.mouse_moved = 1;
      input.real_mouse_x = ev->pointer.x;
      input.real_mouse_y = ev->pointer.y;
      input.mouse_x = ev->pointer.x / 8;
      input.mouse_y = ev->pointer.y / 14;
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
        rval = 0;
        break;
      }
      if(ckey == IKEY_NUMLOCK)
        input.numlock_status ^= 1;
      if(ckey == IKEY_CAPSLOCK)
        input.caps_status ^= 1;

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
      input.last_unicode = convert_internal_unicode(ckey);
      input.last_key_repeat = ckey;
      input.last_unicode_repeat = input.last_unicode;
      input.last_keypress_time = get_ticks();
      input.last_key_release = IKEY_UNKNOWN;
      break;
    }
    case EVENT_KEY_UP:
    {
      enum keycode ckey = convert_USB_internal(ev->key.key);
      if(!ckey)
      {
        rval = 0;
        break;
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
    default:
    {
      rval = 0;
      break;
    }
  }

  return rval;
}

Uint32 update_event_status(void)
{
  Uint32 rval = 0;
  union event ev;

  input.last_key = IKEY_UNKNOWN;
  input.last_unicode = 0;
  input.mouse_moved = 0;
  input.last_mouse_button = 0;

  while(read_eq(&ev))
    rval |= process_event(&ev);

  rval |= update_autorepeat();

  return rval;
}

void wait_event(void)
{
  mqmsg_t ev;

  input.last_key = IKEY_UNKNOWN;
  input.last_unicode = 0;
  input.mouse_moved = 0;
  input.last_mouse_button = 0;

  if(MQ_Receive(eq, &ev, MQ_MSG_BLOCK))
  {
    process_event((union event *)ev);
    free(ev);
  }
  update_autorepeat();
}

void real_warp_mouse(Uint32 x, Uint32 y)
{
  // Mouse warping doesn't work too well with the Wiimote
}

void initialize_joysticks(void)
{
  MQ_Init(&eq, EVENT_QUEUE_SIZE);
  LWP_CreateThread(&poll_thread, wii_poll_thread, NULL, poll_stack, STACKSIZE,
   40);
  LWP_CreateThread(&kbd_thread, wii_kbd_thread, NULL, kbd_stack, STACKSIZE, 20);
}
