/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Kevin Vance <kvance@kvance.com>
 * Copyright (C) 2019 Alice Rowan <petrifiedrowan@gmail.com>
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
#include "compat_sdl.h"
#include "render_sdl.h"
#include "util.h"

#include "SDL.h"

extern struct input_status input;

static boolean numlock_status_initialized;
static int joystick_instance_ids[MAX_JOYSTICKS];
static SDL_Joystick *joysticks[MAX_JOYSTICKS];

static enum keycode convert_SDL_internal(SDL_Keycode key)
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
    case SDLK_EQUALS: return IKEY_EQUALS;
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
    case SDLK_KP_0: return IKEY_KP0;
    case SDLK_KP_1: return IKEY_KP1;
    case SDLK_KP_2: return IKEY_KP2;
    case SDLK_KP_3: return IKEY_KP3;
    case SDLK_KP_4: return IKEY_KP4;
    case SDLK_KP_5: return IKEY_KP5;
    case SDLK_KP_6: return IKEY_KP6;
    case SDLK_KP_7: return IKEY_KP7;
    case SDLK_KP_8: return IKEY_KP8;
    case SDLK_KP_9: return IKEY_KP9;
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
    case SDLK_NUMLOCKCLEAR: return IKEY_NUMLOCK;
    case SDLK_CAPSLOCK: return IKEY_CAPSLOCK;
    case SDLK_SCROLLLOCK: return IKEY_SCROLLOCK;
    case SDLK_RSHIFT: return IKEY_RSHIFT;
    case SDLK_LSHIFT: return IKEY_LSHIFT;
    case SDLK_RCTRL: return IKEY_RCTRL;
    case SDLK_LCTRL: return IKEY_LCTRL;
    case SDLK_RALT: return IKEY_RALT;
    case SDLK_LALT: return IKEY_LALT;
#if !SDL_VERSION_ATLEAST(2,0,0)
    // SDL 1.2 had two different versions of the same pair of keys.
    // Because of this, we can't just #define these to the new values.
    case SDLK_LMETA: return IKEY_LSUPER;
    case SDLK_RMETA: return IKEY_RSUPER;
    case SDLK_LSUPER: return IKEY_LSUPER;
    case SDLK_RSUPER: return IKEY_RSUPER;
#else
    case SDLK_LGUI: return IKEY_LSUPER;
    case SDLK_RGUI: return IKEY_RSUPER;
#endif
    case SDLK_SYSREQ: return IKEY_SYSREQ;
    case SDLK_PAUSE: return IKEY_BREAK;
    case SDLK_MENU: return IKEY_MENU;
#ifdef __WIN32__
#if SDL_VERSION_ATLEAST(2,0,6) && !SDL_VERSION_ATLEAST(2,0,10)
    // Dumb hack for a Windows virtual keycode bug. TODO remove.
    case SDLK_CLEAR: return IKEY_KP5;
#endif
#endif
    default: return IKEY_UNKNOWN;
  }
}

#ifdef CONFIG_PANDORA
static int get_pandora_joystick_button(SDL_Keycode key)
{
  // Pandora hack. The home, end, page up, page down, right shift,
  // and right control keys are actually joystick buttons.
  switch(key)
  {
    case SDLK_HOME:     return 0;
    case SDLK_END:      return 1;
    case SDLK_PAGEUP:   return 2;
    case SDLK_PAGEDOWN: return 3;
    case SDLK_RSHIFT:   return 4;
    case SDLK_RCTRL:    return 5;
  }
  return -1;
}
#endif

#if SDL_VERSION_ATLEAST(2,0,0)
/**
 * The SDL GameController API puts this in an interesting place. The best way
 * to populate MZX action mappings is to parse the SDL mapping string directly.
 * The API offers no other way to get detailed mapping information on half axes
 * and axis inversions (these were added after the API was designed; no new
 * functions were added to read this extended info), and using gamecontroller
 * events to simulate presses could result in situations where the joystick
 * events and the gamecontroller events create different simultaneous presses.
 *
 * However, the axis mappings can be so convoluted it's better to open the
 * controller with the API anyway just to populate the analog axis values. This
 * appears to not affect the joystick events received whatsoever.
 *
 * No equivalent of this API exists for SDL 1.x.
 */

static SDL_GameController *gamecontrollers[MAX_JOYSTICKS];
boolean allow_gamecontroller_mapping = true;

static enum joystick_special_axis sdl_axis_map[SDL_CONTROLLER_AXIS_MAX] =
{
  [SDL_CONTROLLER_AXIS_LEFTX]         = JOY_AXIS_LEFT_X,
  [SDL_CONTROLLER_AXIS_LEFTY]         = JOY_AXIS_LEFT_Y,
  [SDL_CONTROLLER_AXIS_RIGHTX]        = JOY_AXIS_RIGHT_X,
  [SDL_CONTROLLER_AXIS_RIGHTY]        = JOY_AXIS_RIGHT_Y,
  [SDL_CONTROLLER_AXIS_TRIGGERLEFT]   = JOY_AXIS_LEFT_TRIGGER,
  [SDL_CONTROLLER_AXIS_TRIGGERRIGHT]  = JOY_AXIS_RIGHT_TRIGGER
};

static Sint16 sdl_axis_action_map[SDL_CONTROLLER_AXIS_MAX][2] =
{
  [SDL_CONTROLLER_AXIS_LEFTX]         = { -JOY_L_LEFT,  -JOY_L_RIGHT },
  [SDL_CONTROLLER_AXIS_LEFTY]         = { -JOY_L_UP,    -JOY_L_DOWN },
  [SDL_CONTROLLER_AXIS_RIGHTX]        = { -JOY_R_LEFT,  -JOY_R_RIGHT },
  [SDL_CONTROLLER_AXIS_RIGHTY]        = { -JOY_R_UP,    -JOY_R_DOWN },
  [SDL_CONTROLLER_AXIS_TRIGGERLEFT]   = { 0,            -JOY_LTRIGGER },
  [SDL_CONTROLLER_AXIS_TRIGGERRIGHT]  = { 0,            -JOY_RTRIGGER },
};

static Sint16 sdl_action_map[SDL_CONTROLLER_BUTTON_MAX] =
{
  [SDL_CONTROLLER_BUTTON_A]             = -JOY_A,
  [SDL_CONTROLLER_BUTTON_B]             = -JOY_B,
  [SDL_CONTROLLER_BUTTON_X]             = -JOY_X,
  [SDL_CONTROLLER_BUTTON_Y]             = -JOY_Y,
  [SDL_CONTROLLER_BUTTON_BACK]          = -JOY_SELECT,
//[SDL_CONTROLLER_BUTTON_GUIDE]         = -JOY_GUIDE,
  [SDL_CONTROLLER_BUTTON_START]         = -JOY_START,
  [SDL_CONTROLLER_BUTTON_LEFTSTICK]     = -JOY_LSTICK,
  [SDL_CONTROLLER_BUTTON_RIGHTSTICK]    = -JOY_RSTICK,
  [SDL_CONTROLLER_BUTTON_LEFTSHOULDER]  = -JOY_LSHOULDER,
  [SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = -JOY_RSHOULDER,
  [SDL_CONTROLLER_BUTTON_DPAD_UP]       = -JOY_UP,
  [SDL_CONTROLLER_BUTTON_DPAD_DOWN]     = -JOY_DOWN,
  [SDL_CONTROLLER_BUTTON_DPAD_LEFT]     = -JOY_LEFT,
  [SDL_CONTROLLER_BUTTON_DPAD_RIGHT]    = -JOY_RIGHT
};

enum
{
  GC_NONE,
  GC_BUTTON,
  GC_AXIS,
  GC_HAT,
};

struct gc_map
{
  char *dbg;
  Uint8 feature;
  Uint8 which;
  Uint8 pos;
};

struct gc_axis_map
{
  struct gc_map neg;
  struct gc_map pos;
};

static int sdl_hat_to_dir(int hat_mask)
{
  switch(hat_mask)
  {
    case SDL_HAT_UP:
      return JOYHAT_UP;

    case SDL_HAT_DOWN:
      return JOYHAT_DOWN;

    case SDL_HAT_LEFT:
      return JOYHAT_LEFT;

    case SDL_HAT_RIGHT:
      return JOYHAT_RIGHT;

    default:
      return -1;
  }
}

static void parse_gamecontroller_read_value(char *key, char *value,
 struct gc_map *single, struct gc_map *neg, struct gc_map *pos)
{
  // Joystick axes may have half-axis prefixes + or -.
  // Joystick axes may also have an inversion suffix ~.
  char half_axis = 0;

  if(*value == '+' || *value == '-')
    half_axis = *(value++);

  switch(*value)
  {
    case 'a':
    {
      // Axis- a# or a#~
      unsigned int axis;

      if(!isdigit((Uint8)value[1]))
        break;

      axis = strtoul(value + 1, &value, 10);
      if(axis >= MAX_JOYSTICK_AXES)
        break;

      // Only one provided output and no half axis specified? Map + to it.
      if(half_axis == 0 && single)
      {
        neg = NULL;
        pos = single;
      }
      else

      if(half_axis == '+')
      {
        pos = single ? single : pos ? pos : neg;
      }
      else

      if(half_axis == '-')
      {
        neg = single ? single : neg ? neg : pos;
      }

      if(value && *value == '~')
      {
        // Invert
        single = neg;
        neg = pos;
        pos = single;
      }

      if(neg)
      {
        neg->feature = GC_AXIS;
        neg->which = axis;
        neg->pos = 0;
        neg->dbg = key;
      }
      if(pos)
      {
        pos->feature = GC_AXIS;
        pos->which = axis;
        pos->pos = 1;
        pos->dbg = key;
      }
      return;
    }

    case 'b':
    {
      // Button- b#
      unsigned int button;

      if(!isdigit((Uint8)value[1]))
        break;

      button = strtoul(value + 1, NULL, 10);
      if(button >= MAX_JOYSTICK_BUTTONS)
        break;

      if(!single)
        single = pos ? pos : neg;

      if(single)
      {
        single->feature = GC_BUTTON;
        single->which = button;
        single->dbg = key;
      }
      return;
    }

    case 'h':
    {
      // Hat- h#.#
      unsigned int hat;
      unsigned int hat_mask;
      int dir;

      if(!isdigit((Uint8)value[1]))
        break;

      hat = strtoul(value + 1, &value, 10);
      if(hat != 0 || !value[0] || !isdigit((Uint8)value[1]))
        break;

      hat_mask = strtoul(value + 1, NULL, 10);
      dir = sdl_hat_to_dir(hat_mask);
      if(dir < 0)
        break;

      if(!single)
        single = pos ? pos : neg;

      if(single)
      {
        single->feature = GC_HAT;
        single->which = hat;
        single->pos = dir;
        single->dbg = key;
      }
      return;
    }
  }
  debug("[JOYSTICK] ignoring '%s' -> '%s'\n", value, key);
  return;
}

static void parse_gamecontroller_read_entry(char *key, char *value,
 struct gc_axis_map *axes, struct gc_map *buttons)
{
  SDL_GameControllerAxis a;
  SDL_GameControllerButton b;
  struct gc_map *single = NULL;
  struct gc_map *neg = NULL;
  struct gc_map *pos = NULL;
  char half_axis = 0;

  // Gamecontroller axes may have half-axis prefixes + or -.
  if(*key == '+' || *key == '-')
    half_axis = *(key++);

  a = SDL_GameControllerGetAxisFromString(key);
  b = SDL_GameControllerGetButtonFromString(key);
  if(a != SDL_CONTROLLER_AXIS_INVALID)
  {
    if(half_axis == '+')
      single = &(axes[a].pos);

    if(half_axis == '-')
      single = &(axes[a].neg);

    if(half_axis == 0)
    {
      neg = &(axes[a].neg);
      pos = &(axes[a].pos);
    }
  }
  else

  if(b != SDL_CONTROLLER_BUTTON_INVALID)
  {
    // This button isn't really useful to MZX.
    if(b == SDL_CONTROLLER_BUTTON_GUIDE)
      return;

    single = &(buttons[b]);
  }
  else

  if(!strcasecmp(key, "platform"))
  {
    // ignore- field used by SDL.
    return;
  }

  else
  {
    warn("[JOYSTICK] Invalid control '%s'! Report this!\n", key);
    return;
  }

  parse_gamecontroller_read_value(key, value, single, neg, pos);
}

static void parse_gamecontroller_read_string(char *map,
 struct gc_axis_map *axes, struct gc_map *buttons)
{
  // Format: entry,entry,...
  // Entry:  value or key:value
  char *end = map + strlen(map);
  char *key;
  char *value;

  while(map < end)
  {
    key = map;
    value = NULL;

    while(*map != ',')
    {
      if(!*map) break;

      if(*map == ':' && !value)
      {
        *map = 0;
        value = map + 1;
      }

      map++;
    }
    *(map++) = 0;

    if(value)
      parse_gamecontroller_read_entry(key, value, axes, buttons);
  }
}

static void parse_gamecontroller_apply(int joy, Sint16 mapping,
 struct gc_map *target, boolean *select_mapped, boolean *select_used)
{
  Uint8 which = target->which;
  Uint8 pos = target->pos;

  if(mapping == -JOY_SELECT || mapping == -JOY_START)
    *select_mapped = true;

  switch(target->feature)
  {
    case GC_NONE:
      return;

    case GC_BUTTON:
    {
      debug("[JOYSTICK]  b%u -> '%s' (%d)\n", which, target->dbg, mapping);
      if(!input.joystick_global_map.button_is_conf[joy][which])
        input.joystick_global_map.button[joy][which] = mapping;

      if(!input.joystick_game_map.button_is_conf[joy][which])
        input.joystick_game_map.button[joy][which] = mapping;
      break;
    }

    case GC_AXIS:
    {
      debug("[JOYSTICK]  a%u%s -> '%s' (%d)\n", which, pos?"+":"-",
       target->dbg, mapping);

      if(!input.joystick_global_map.axis_is_conf[joy][which])
        input.joystick_global_map.axis[joy][which][pos] = mapping;

      if(!input.joystick_game_map.axis_is_conf[joy][which])
        input.joystick_game_map.axis[joy][which][pos] = mapping;
      break;
    }

    case GC_HAT:
    {
      debug("[JOYSTICK]  hd%u -> '%s' (%d)\n", pos, target->dbg, mapping);
      if(!input.joystick_global_map.hat_is_conf[joy])
        input.joystick_global_map.hat[joy][pos] = mapping;

      if(!input.joystick_game_map.hat_is_conf[joy])
        input.joystick_game_map.hat[joy][pos] = mapping;
      break;
    }
  }
  if(mapping == -JOY_SELECT || mapping == -JOY_START)
    *select_used = true;
  return;
}

static void parse_gamecontroller_map(int joystick_index, char *map)
{
  struct gc_axis_map axes[SDL_CONTROLLER_AXIS_MAX];
  struct gc_map buttons[SDL_CONTROLLER_BUTTON_MAX];
  boolean select_mapped = false;
  boolean select_used = false;
  size_t i;

  memset(axes, 0, sizeof(axes));
  memset(buttons, 0, sizeof(buttons));

  parse_gamecontroller_read_string(map, axes, buttons);

  // Apply axes.
  for(i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++)
  {
    parse_gamecontroller_apply(joystick_index,
     sdl_axis_action_map[i][0], &(axes[i].neg), &select_mapped, &select_used);

    parse_gamecontroller_apply(joystick_index,
     sdl_axis_action_map[i][1], &(axes[i].pos), &select_mapped, &select_used);
  }

  // Apply buttons.
  for(i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++)
  {
    parse_gamecontroller_apply(joystick_index,
     sdl_action_map[i], &(buttons[i]), &select_mapped, &select_used);
  }

  if(select_mapped && !select_used)
  {
    // TODO originally this was going to try to place JOY_SELECT on another
    // button. That was kind of a bad idea, so just print a warning for now.
    info("[JOYSTICK] %d doesn't have any gamecontroller button that binds "
     "'select' or 'start' (by default, these are the SDL gamecontoller buttons "
     "'back' and 'start', respectively). Since these buttons are used to open "
     "the joystick menu, you may want to override this controller mapping.\n",
      joystick_index + 1);
  }
}

static void init_gamecontroller(int sdl_index, int joystick_index)
{
  SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(sdl_index);
  char guid_string[33];

  SDL_JoystickGetGUIDString(guid, guid_string, 33);
  gamecontrollers[joystick_index] = NULL;

  if(SDL_IsGameController(sdl_index))
  {
    SDL_GameController *gamecontroller = SDL_GameControllerOpen(sdl_index);

    if(gamecontroller)
    {
      char *mapping = NULL;
      gamecontrollers[joystick_index] = gamecontroller;

#if SDL_VERSION_ATLEAST(2,0,9)
      // NOTE: the other functions for this will not return the default mapping
      // string; this is the only one that can return everything. Right now,
      // this only matters for the Emscripten port.
      mapping = (char *)SDL_GameControllerMappingForDeviceIndex(sdl_index);
#else
      mapping = (char *)SDL_GameControllerMapping(gamecontroller);
#endif

#ifdef __EMSCRIPTEN__
      if(!mapping)
      {
        // The only function that can return the default mapping was added
        // in SDL 2.0.9, but I'm not sure Emscripten actually has SDL 2.0.9.
        // This string is copied from SDL_gamecontrollerdb.h
        static const char default_mapping[] =
         "default,Standard Gamepad,a:b0,b:b1,back:b8,dpdown:b13,dpleft:b14,"
         "dpright:b15,dpup:b12,guide:b16,leftshoulder:b4,leftstick:b10,"
         "lefttrigger:b6,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b11,"
         "righttrigger:b7,rightx:a2,righty:a3,start:b9,x:b2,y:b3,";

        mapping = SDL_malloc(sizeof(default_mapping));
        if(mapping)
          strcpy(mapping, default_mapping);
      }
#endif

      if(mapping)
      {
        info("[JOYSTICK] joystick %d has an SDL mapping: %s\n",
         joystick_index + 1, mapping);

        if(strncmp(mapping, guid_string, strlen(guid_string)))
          info("[JOYSTICK] GUID: %s\n", guid_string);

        if(allow_gamecontroller_mapping)
          parse_gamecontroller_map(joystick_index, mapping);

        SDL_free(mapping);
        return;
      }
    }
  }
  info("[JOYSTICK] joystick %d does not have an SDL mapping or could not be "
   "opened as a gamecontroller (GUID: %s).\n", joystick_index + 1, guid_string);
}

// Clean up auto-generated bindings so they don't cause problems for other
// controllers that might end up using this position.
static void gamecontroller_clean_map(int joy)
{
  int i;

  for(i = 0; i < MAX_JOYSTICK_AXES; i++)
  {
    if(!input.joystick_global_map.axis_is_conf[joy][i])
    {
      input.joystick_global_map.axis[joy][i][0] = 0;
      input.joystick_global_map.axis[joy][i][1] = 0;
    }

    if(!input.joystick_game_map.axis_is_conf[joy][i])
    {
      input.joystick_game_map.axis[joy][i][0] = 0;
      input.joystick_game_map.axis[joy][i][1] = 0;
    }
  }

  for(i = 0; i < MAX_JOYSTICK_BUTTONS; i++)
  {
    if(!input.joystick_global_map.button_is_conf[joy][i])
      input.joystick_global_map.button[joy][i] = 0;

    if(!input.joystick_game_map.button_is_conf[joy][i])
      input.joystick_game_map.button[joy][i] = 0;
  }

  if(!input.joystick_global_map.hat_is_conf[joy])
  {
    for(i = 0; i < NUM_JOYSTICK_HAT_DIRS; i++)
      input.joystick_global_map.hat[joy][i] = 0;
  }

  if(!input.joystick_game_map.hat_is_conf[joy])
  {
    for(i = 0; i < NUM_JOYSTICK_HAT_DIRS; i++)
      input.joystick_game_map.hat[joy][i] = 0;
  }
}

/**
 * Load gamecontrollerdb.txt if configured and if it isn't already loaded.
 * This adds more gamecontroller mappings so MZX can support more controllers.
 * The function this uses wasn't added until 2.0.2.
 */
static void load_gamecontrollerdb(void)
{
#if defined(CONFIG_GAMECONTROLLERDB) && SDL_VERSION_ATLEAST(2,0,2)
  static boolean gamecontrollerdb_loaded = false;

  if(!gamecontrollerdb_loaded)
  {
    const char *path = mzx_res_get_by_id(GAMECONTROLLERDB_TXT);

    if(path)
    {
      int result = SDL_GameControllerAddMappingsFromFile(path);
      if(result >= 0)
        debug("[JOYSTICK] Added %d mappings from '%s'.\n", result, path);
    }

    gamecontrollerdb_loaded = true;
  }
#endif
}

/**
 * Change one of the default SDL to MZX mapping values.
 */
void gamecontroller_map_sym(const char *sym, const char *value)
{
  SDL_GameControllerAxis a;
  SDL_GameControllerButton b;
  Sint16 binding = 0;

  if(joystick_parse_map_value(value, &binding))
  {
    char dir = 0;
    if(*sym == '+' || *sym == '-')
      dir = *(sym++);

    // Digital axis (default to + if no dir specified).
    a = SDL_GameControllerGetAxisFromString(sym);
    if(a != SDL_CONTROLLER_AXIS_INVALID)
    {
      int pos = (dir != '-') ? 1 : 0;
      sdl_axis_action_map[a][pos] = binding;
    }

    // Button
    b = SDL_GameControllerGetButtonFromString(sym);
    if(b != SDL_CONTROLLER_BUTTON_INVALID)
      sdl_action_map[b] = binding;
  }

  // TODO analog axes
}

/**
 * Enable or disable the SDL to MZX mapping system.
 */
void gamecontroller_set_enabled(boolean enable)
{
  allow_gamecontroller_mapping = enable;
}

/**
 * Add a mapping string to SDL.
 */
void gamecontroller_add_mapping(const char *mapping)
{
  // Make sure this is loaded first so it doesn't override the user mapping.
  load_gamecontrollerdb();

  if(SDL_GameControllerAddMapping(mapping) < 0)
    warn("Failed to add gamecontroller mapping: %s\n", SDL_GetError());
}

#endif /* SDL_VERSION_ATLEAST(2,0,0) */

/**
 * SDL 2 uses joystick instance IDs instead of the joystick index for all
 * purposes aside from SDL_JoystickOpen(). We prefer the joystick index (which
 * SDL 1.2 used exclusively) as the instance IDs increment every time
 * SDL_JoystickOpen() is used, so keep a map between the two. Additionally,
 * store the joystick pointer to make things easier when closing joysticks.
 */

static int get_joystick_index(int sdl_instance_id)
{
  int i;
  for(i = 0; i < MAX_JOYSTICKS; i++)
    if((joystick_instance_ids[i] == sdl_instance_id) && joysticks[i])
      return i;

  return -1;
}

static int get_next_unused_joystick_index(void)
{
  int i;
  for(i = 0; i < MAX_JOYSTICKS; i++)
    if(!joysticks[i])
      return i;

  return -1;
}

static void init_joystick(int sdl_index)
{
  struct buffered_status *status = store_status();
  int joystick_index = get_next_unused_joystick_index();

  if(joystick_index >= 0)
  {
    SDL_Joystick *joystick = SDL_JoystickOpen(sdl_index);

    if(joystick)
    {
      joystick_instance_ids[joystick_index] = SDL_JoystickInstanceID(joystick);
      joysticks[joystick_index] = joystick;
      joystick_set_active(status, joystick_index, true);

      debug("[JOYSTICK] Opened %d (SDL instance ID: %d)\n",
       joystick_index + 1, joystick_instance_ids[joystick_index]);

#if SDL_VERSION_ATLEAST(2,0,0)
      init_gamecontroller(sdl_index, joystick_index);
#endif
    }
  }
}

#if SDL_VERSION_ATLEAST(2,0,0)
// TODO: swappable joysticks in SDL <2
static void close_joystick(int joystick_index)
{
  if(joystick_index >= 0)
  {
    debug("[JOYSTICK] Closing %d (SDL instance ID: %d)\n",
     joystick_index + 1, joystick_instance_ids[joystick_index]);

    // SDL_GameControllerClose also closes the joystick.
    if(gamecontrollers[joystick_index])
    {
      SDL_GameControllerClose(gamecontrollers[joystick_index]);
      gamecontroller_clean_map(joystick_index);
      gamecontrollers[joystick_index] = NULL;
    }
    else
      SDL_JoystickClose(joysticks[joystick_index]);

    joystick_instance_ids[joystick_index] = -1;
    joysticks[joystick_index] = NULL;
  }
}
#endif

static boolean process_event(SDL_Event *event)
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
      // Set the exit status
      status->exit_status = true;
      break;
    }

#if SDL_VERSION_ATLEAST(2,0,0)
    case SDL_WINDOWEVENT:
    {
      switch(event->window.event)
      {
        case SDL_WINDOWEVENT_RESIZED:
        {
          resize_screen(event->window.data1, event->window.data2);
          break;
        }

        case SDL_WINDOWEVENT_FOCUS_LOST:
        {
          // Pause while minimized
          if(input.unfocus_pause)
          {
            while(1)
            {
              SDL_WaitEvent(event);

              if(event->type == SDL_WINDOWEVENT &&
                 event->window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
                break;
            }
          }
          break;
        }
      }

      break;
    }
#else // !SDL_VERSION_ATLEAST(2,0,0)
    case SDL_VIDEORESIZE:
    {
      resize_screen(event->resize.w, event->resize.h);
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
#endif // !SDL_VERSION_ATLEAST(2,0,0)

    case SDL_MOUSEMOTION:
    {
      SDL_Window *window = SDL_GetWindowFromID(sdl_window_id);
      int mx_real = event->motion.x;
      int my_real = event->motion.y;
      int mx, my, min_x, min_y, max_x, max_y;
      get_screen_coords(mx_real, my_real, &mx, &my, &min_x,
       &min_y, &max_x, &max_y);

      if(mx >= SCREEN_PIX_W)
      {
        SDL_WarpMouseInWindow(window, max_x, my_real);
        mx = SCREEN_PIX_W - 1;
      }

      if(mx < 0)
      {
        SDL_WarpMouseInWindow(window, min_x, my_real);
        mx = 0;
      }

      if(my >= SCREEN_PIX_H)
      {
        SDL_WarpMouseInWindow(window, mx_real, max_y);
        my = SCREEN_PIX_H - 1;
      }

      if(my < 0)
      {
        SDL_WarpMouseInWindow(window, mx_real, min_y);
        my = 0;
      }

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

#if SDL_VERSION_ATLEAST(2,0,0)
    // emulate the X11-style "wheel is a button" that SDL 1.2 used
    case SDL_MOUSEWHEEL:
    {
      SDL_Event fake_event;

      fake_event.type = SDL_MOUSEBUTTONDOWN;
      fake_event.button.windowID = event->wheel.windowID;
      fake_event.button.which = event->wheel.which;
      fake_event.button.state = SDL_PRESSED;
      fake_event.button.x = 0;
      fake_event.button.y = 0;

      if(abs(event->wheel.x) > abs(event->wheel.y))
      {
        if(event->wheel.x < 0)
          fake_event.button.button = MOUSE_BUTTON_WHEELLEFT;
        else
          fake_event.button.button = MOUSE_BUTTON_WHEELRIGHT;
      }

      else
      {
        if(event->wheel.y < 0)
          fake_event.button.button = MOUSE_BUTTON_WHEELDOWN;
        else
          fake_event.button.button = MOUSE_BUTTON_WHEELUP;
      }

      SDL_PushEvent(&fake_event);

      fake_event.type = SDL_MOUSEBUTTONUP;
      fake_event.button.state = SDL_RELEASED;

      SDL_PushEvent(&fake_event);
      break;
    }
#endif // SDL_VERSION_ATLEAST(2,0,0)

    case SDL_KEYDOWN:
    {
      Uint16 unicode = 0;

#if SDL_VERSION_ATLEAST(2,0,0)
      // SDL 2.0 uses proper key repeat, but derives its timing from the OS.
      // Our hand-rolled key repeat is more friendly to use than SDL 2.0's
      // (with Windows, at least) and is consistent across all platforms.

      // To enable SDL 2.0 key repeat, remove this check/break and see the
      // update_autorepeat() function in event.c

      if(event->key.repeat)
        break;
#endif

#ifdef CONFIG_PANDORA
      {
        // Pandora hack. Certain keys are actually joystick buttons.
        int button = get_pandora_joystick_button(event->key.keysym.sym);
        if(button >= 0)
        {
          joystick_button_press(status, 0, button);
          break;
        }
      }
#endif

      ckey = convert_SDL_internal(event->key.keysym.sym);
      if(!ckey)
      {
#if !SDL_VERSION_ATLEAST(2,0,0)
        if(!event->key.keysym.unicode)
          break;
#endif
        ckey = IKEY_UNICODE;
      }

#if SDL_VERSION_ATLEAST(2,0,0)
      // SDL 2.0 sends the raw key and translated 'text' as separate events.
      // There is no longer a UNICODE mode that sends both at once.
      // Because of the way the SDL 1.2 assumption is embedded deeply in
      // the MZX event queue processor, emulate the 1.2 behaviour by waiting
      // for a TEXTINPUT event after a KEYDOWN.
      SDL_PumpEvents();

      if(SDL_PeepEvents(event, 1, SDL_GETEVENT, SDL_TEXTINPUT, SDL_TEXTINPUT))
        unicode = event->text.text[0] | event->text.text[1] << 8;
#else
      unicode = event->key.keysym.unicode;
#endif

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
#if !SDL_VERSION_ATLEAST(2,0,0) && defined(__WIN32__)
        status->numlock_status = true;
#endif
        break;
      }

#ifdef CONFIG_ENABLE_SCREENSHOTS
      if(ckey == IKEY_F12 && enable_f12_hack)
      {
        dump_screen();
        break;
      }
#endif

      // Ignore alt + tab
      if((ckey == IKEY_TAB) && get_alt_status(keycode_internal))
      {
        break;
      }

      if(status->key_repeat &&
       (status->key_repeat != IKEY_LSHIFT) &&
       (status->key_repeat != IKEY_RSHIFT) &&
       (status->key_repeat != IKEY_LSUPER) &&
       (status->key_repeat != IKEY_RSUPER) &&
       (status->key_repeat != IKEY_LALT) &&
       (status->key_repeat != IKEY_RALT) &&
       (status->key_repeat != IKEY_LCTRL) &&
       (status->key_repeat != IKEY_RCTRL))
      {
        // Stack current repeat key if it isn't shift, super, alt, or ctrl
        if(input.repeat_stack_pointer != KEY_REPEAT_STACK_SIZE)
        {
          input.key_repeat_stack[input.repeat_stack_pointer] =
           status->key_repeat;
          input.unicode_repeat_stack[input.repeat_stack_pointer] =
           status->unicode_repeat;
          input.repeat_stack_pointer++;
        }
      }

      key_press(status, ckey, unicode);
      break;
    }

    case SDL_KEYUP:
    {
#ifdef CONFIG_PANDORA
      {
        // Pandora hack. Certain keys are actually joystick buttons.
        int button = get_pandora_joystick_button(event->key.keysym.sym);
        if(button >= 0)
        {
          joystick_button_release(status, 0, button);
          break;
        }
      }
#endif

      ckey = convert_SDL_internal(event->key.keysym.sym);
      if(!ckey)
      {
#if !SDL_VERSION_ATLEAST(2,0,0)
        if(!status->keymap[IKEY_UNICODE])
          break;
#endif
        ckey = IKEY_UNICODE;
      }

      if(ckey == IKEY_NUMLOCK)
      {
#if !SDL_VERSION_ATLEAST(2,0,0) && defined(__WIN32__)
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
      key_release(status, ckey);
      break;
    }

#if SDL_VERSION_ATLEAST(2,0,0)
    case SDL_JOYDEVICEADDED:
    {
      // Add a new joystick.
      // "which" for this event (but not for any other joystick event) is not
      // a joystick instance ID, but instead an index for SDL_JoystickOpen().
      init_joystick(event->jdevice.which);
      break;
    }

    case SDL_JOYDEVICEREMOVED:
    {
      // Close a disconnected joystick.
      int which = event->jdevice.which;
      int joystick_index = get_joystick_index(which);

      close_joystick(joystick_index);

      // Joysticks can be trivially disconnected while holding a button and
      // the corresponding release event will never be sent for it. Release
      // all of this joystick's inputs.
      joystick_clear(status, joystick_index);
      break;
    }

    case SDL_CONTROLLERAXISMOTION:
    {
      // Since gamecontroller axis mappings can be complicated, use
      // the gamecontroller events to update the named axis values.
      int value = event->caxis.value;
      int which = event->caxis.which;
      int axis = event->caxis.axis;
      enum joystick_special_axis special_axis = sdl_axis_map[axis];

      int joystick_index = get_joystick_index(which);
      if(joystick_index < 0 || !special_axis || !allow_gamecontroller_mapping)
        break;

      joystick_special_axis_update(status, joystick_index, special_axis, value);
      break;
    }
#endif

    case SDL_JOYAXISMOTION:
    {
      int axis_value = event->jaxis.value;
      int which = event->jaxis.which;
      int axis = event->jaxis.axis;

      // Get the real joystick index from the SDL instance ID
      int joystick_index = get_joystick_index(which);
      if(joystick_index < 0)
        break;

      joystick_axis_update(status, joystick_index, axis, axis_value);
      break;
    }

    case SDL_JOYBUTTONDOWN:
    {
      int which = event->jbutton.which;
      int button = event->jbutton.button;

      // Get the real joystick index from the SDL instance ID
      int joystick_index = get_joystick_index(which);
      if(joystick_index < 0)
        break;

#ifdef CONFIG_SWITCH
      // Ignore fake axis "buttons".
      if((button >= 16) && (button <= 23))
        break;
#endif

      joystick_button_press(status, joystick_index, button);
      break;
    }

    case SDL_JOYBUTTONUP:
    {
      int which = event->jbutton.which;
      int button = event->jbutton.button;

      // Get the real joystick index from the SDL instance ID
      int joystick_index = get_joystick_index(which);
      if(joystick_index < 0)
        break;

#ifdef CONFIG_SWITCH
      // Ignore fake axis "buttons".
      if((button >= 16) && (button <= 23))
        break;
#endif

      joystick_button_release(status, joystick_index, button);
      break;
    }

    case SDL_JOYHATMOTION:
    {
      int which = event->jhat.which;
      int dir = event->jhat.value;
      boolean hat_u = (dir & SDL_HAT_UP) ? true : false;
      boolean hat_d = (dir & SDL_HAT_DOWN) ? true : false;
      boolean hat_l = (dir & SDL_HAT_LEFT) ? true : false;
      boolean hat_r = (dir & SDL_HAT_RIGHT) ? true : false;

      // Get the real joystick index from the SDL instance ID
      int joystick_index = get_joystick_index(which);
      if(joystick_index < 0)
        break;

      joystick_hat_update(status, joystick_index, JOYHAT_UP, hat_u);
      joystick_hat_update(status, joystick_index, JOYHAT_DOWN, hat_d);
      joystick_hat_update(status, joystick_index, JOYHAT_LEFT, hat_l);
      joystick_hat_update(status, joystick_index, JOYHAT_RIGHT, hat_r);
      break;
    }

    default:
      return false;
  }

  return true;
}

boolean __update_event_status(void)
{
  Uint32 rval = false;
  SDL_Event event;

  while(SDL_PollEvent(&event))
    rval |= process_event(&event);

#if !SDL_VERSION_ATLEAST(2,0,0)
  {
    // ALT+F4 - will not trigger an exit event, so set the variable manually.
    struct buffered_status *status = store_status();

    if(status->key_repeat == IKEY_F4 && get_alt_status(keycode_internal))
    {
      status->key = IKEY_UNKNOWN;
      status->key_repeat = IKEY_UNKNOWN;
      status->unicode = 0;
      status->exit_status = true;
      return true;
    }
  }
#endif /*SDL_VERSION_ATLEAST*/

  return rval;
}

// This returns whether the input buffer _may_ contain a request to quit.
// Proper polling should be performed if the answer is yes.
boolean __peek_exit_input(void)
{
#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_Event events[256];
  int num_events, i;

  SDL_PumpEvents();
  num_events =
   SDL_PeepEvents(events, 256, SDL_PEEKEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);

  for(i = 0; i < num_events; i++)
  {
    if(events[i].type == SDL_QUIT)
      return true;

    if(events[i].type == SDL_KEYDOWN)
    {
      SDL_KeyboardEvent *ev = (SDL_KeyboardEvent *) &events[i];

      if(ev->keysym.sym == SDLK_ESCAPE)
        return true;

      if(ev->keysym.sym == SDLK_c && (ev->keysym.mod & KMOD_CTRL))
        return true;

      if(ev->keysym.sym == SDLK_F4 && (ev->keysym.mod & KMOD_ALT))
        return true;
    }
  }

#else /* !SDL_VERSION_ATLEAST(2,0,0) */

  // FIXME: SDL supports SDL_PeepEvents but the implementation is
  // different

#endif /* SDL_VERSION_ATLEAST(2,0,0) */

  return false;
}

void __wait_event(void)
{
  /**
   * This function used to take a timeout param, but we really don't want to
   * implement that on a per-platform basis. SDL_WaitEventTimeout also does a
   * very cool SDL_Delay(10) which we can probably live without.
   */
  SDL_Event event;
  boolean any_event;

  // FIXME: WaitEvent with MSVC hangs the render cycle, so this is, hopefully,
  //        a short-term fix.
#ifdef MSVC_H
  any_event = SDL_PollEvent(&event);
#else
  any_event = SDL_WaitEvent(&event);
#endif

  if(any_event)
    process_event(&event);
}

void real_warp_mouse(int x, int y)
{
  SDL_Window *window = SDL_GetWindowFromID(sdl_window_id);

  if((x < 0) || (y < 0))
  {
    int current_x, current_y;
    SDL_GetMouseState(&current_x, &current_y);

    if(x < 0)
      x = current_x;

    if(y < 0)
      y = current_y;
  }

  SDL_WarpMouseInWindow(window, x, y);
}

void initialize_joysticks(void)
{
#if !SDL_VERSION_ATLEAST(2,0,0) || defined(CONFIG_SWITCH)
  // SDL 1.2 doesn't have joystick added/removed events.
  // Switch SDL doesn't seem to generate these events at all on startup.
  int i, count;

  count = SDL_NumJoysticks();

  if(count > MAX_JOYSTICKS)
    count = MAX_JOYSTICKS;

  for(i = 0; i < count; i++)
    init_joystick(i);
#endif

#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_GameControllerEventState(SDL_ENABLE);
  load_gamecontrollerdb();
#endif

  SDL_JoystickEventState(SDL_ENABLE);
}
