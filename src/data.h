/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
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

#ifndef __DATA_H
#define __DATA_H

#include "compat.h"

__M_BEGIN_DECLS

#include "const.h"
#include <stdio.h>
#include <stdlib.h>

CORE_LIBSPEC extern char curr_file[MAX_PATH];
CORE_LIBSPEC extern char curr_sav[MAX_PATH];
CORE_LIBSPEC extern char current_dir[MAX_PATH];
CORE_LIBSPEC extern char config_dir[MAX_PATH];

CORE_LIBSPEC extern unsigned char scroll_color;
CORE_LIBSPEC extern const char *const thing_names[128];
CORE_LIBSPEC extern const unsigned int flags[128];

enum thing
{
  SPACE           = 0,
  NORMAL          = 1,
  SOLID           = 2,
  TREE            = 3,
  LINE            = 4,
  CUSTOM_BLOCK    = 5,
  BREAKAWAY       = 6,
  CUSTOM_BREAK    = 7,
  BOULDER         = 8,
  CRATE           = 9,
  CUSTOM_PUSH     = 10,
  BOX             = 11,
  CUSTOM_BOX      = 12,
  FAKE            = 13,
  CARPET          = 14,
  FLOOR           = 15,
  TILES           = 16,
  CUSTOM_FLOOR    = 17,
  WEB             = 18,
  THICK_WEB       = 19,
  STILL_WATER     = 20,
  N_WATER         = 21,
  S_WATER         = 22,
  E_WATER         = 23,
  W_WATER         = 24,
  ICE             = 25,
  LAVA            = 26,
  CHEST           = 27,
  GEM             = 28,
  MAGIC_GEM       = 29,
  HEALTH          = 30,
  RING            = 31,
  POTION          = 32,
  ENERGIZER       = 33,
  GOOP            = 34,
  AMMO            = 35,
  BOMB            = 36,
  LIT_BOMB        = 37,
  EXPLOSION       = 38,
  KEY             = 39,
  LOCK            = 40,
  DOOR            = 41,
  OPEN_DOOR       = 42,
  STAIRS          = 43,
  CAVE            = 44,
  CW_ROTATE       = 45,
  CCW_ROTATE      = 46,
  GATE            = 47,
  OPEN_GATE       = 48,
  TRANSPORT       = 49,
  COIN            = 50,
  N_MOVING_WALL   = 51,
  S_MOVING_WALL   = 52,
  E_MOVING_WALL   = 53,
  W_MOVING_WALL   = 54,
  POUCH           = 55,
  PUSHER          = 56,
  SLIDER_NS       = 57,
  SLIDER_EW       = 58,
  LAZER           = 59,
  LAZER_GUN       = 60,
  BULLET          = 61,
  MISSILE         = 62,
  FIRE            = 63,
  FOREST          = 65,
  LIFE            = 66,
  WHIRLPOOL_1     = 67,
  WHIRLPOOL_2     = 68,
  WHIRLPOOL_3     = 69,
  WHIRLPOOL_4     = 70,
  INVIS_WALL      = 71,
  RICOCHET_PANEL  = 72,
  RICOCHET        = 73,
  MINE            = 74,
  SPIKE           = 75,
  CUSTOM_HURT     = 76,
  __TEXT          = 77,
  SHOOTING_FIRE   = 78,
  SEEKER          = 79,
  SNAKE           = 80,
  EYE             = 81,
  THIEF           = 82,
  SLIMEBLOB       = 83,
  RUNNER          = 84,
  GHOST           = 85,
  DRAGON          = 86,
  FISH            = 87,
  SHARK           = 88,
  SPIDER          = 89,
  GOBLIN          = 90,
  SPITTING_TIGER  = 91,
  BULLET_GUN      = 92,
  SPINNING_GUN    = 93,
  BEAR            = 94,
  BEAR_CUB        = 95,
  MISSILE_GUN     = 97,
  SPRITE          = 98,
  SPR_COLLISION   = 99,
  IMAGE_FILE      = 100,
  SENSOR          = 122,
  ROBOT_PUSHABLE  = 123,
  ROBOT           = 124,
  SIGN            = 125,
  SCROLL          = 126,
  PLAYER          = 127,
  NO_ID           = 255
};

static inline bool is_fake(enum thing id)
{
  return (id == SPACE) || ((id >= FAKE) && (id <= THICK_WEB));
}

static inline bool is_robot(enum thing id)
{
  return (id == ROBOT) || (id == ROBOT_PUSHABLE);
}

static inline bool is_signscroll(enum thing id)
{
  return (id == SIGN) || (id == SCROLL);
}

static inline bool is_water(enum thing id)
{
  return (id >= STILL_WATER) && (id <= W_WATER);
}

static inline bool is_whirlpool(enum thing id)
{
  return (id >= WHIRLPOOL_1) && (id <= WHIRLPOOL_2);
}

static inline bool is_enemy(enum thing id)
{
  return (id >= SNAKE) && (id <= BEAR_CUB) &&
   (id != BULLET_GUN) && (id != SPINNING_GUN);
}

static inline bool is_storageless(enum thing id)
{
  return id < SENSOR;
}

enum dir
{
  IDLE    = 0,
  NORTH   = 1,
  SOUTH   = 2,
  EAST    = 3,
  WEST    = 4,
  RANDNS  = 5,
  RANDEW  = 6,
  RANDNE  = 7,
  RANDNB  = 8,
  SEEK    = 9,
  RANDANY = 10,
  BENEATH = 11,
  ANYDIR  = 12,
  FLOW    = 13,
  NODIR   = 14,
  RANDB   = 15,
  RANDP   = 16,
  CW      = 32,
  OPP     = 64,
  RANDNOT = 128
};

static inline bool is_cardinal_dir(enum dir d)
{
  return (d >= NORTH) && (d <= WEST);
}

static inline int dir_to_int(enum dir d)
{
  return (int)d - 1;
}

static inline enum dir int_to_dir(int d)
{
  return (enum dir)(d + 1);
}

#define CAN_PUSH      (1 << 0)
#define CAN_TRANSPORT (1 << 1)
#define CAN_LAVAWALK  (1 << 2)
#define CAN_FIREWALK  (1 << 3)
#define CAN_WATERWALK (1 << 4)
#define MUST_WEB      (1 << 5)
#define MUST_THICKWEB (1 << 6)
#define REACT_PLAYER  (1 << 7)
#define MUST_WATER    (1 << 8)
#define MUST_LAVAGOOP (1 << 9)
#define CAN_GOOPWALK  (1 << 10)
#define SPITFIRE      (1 << 11)

enum move_status
{
  NO_HIT      = 0,
  HIT         = 1,
  HIT_PLAYER  = 2,
  HIT_EDGE    = 3
};

enum equality
{
  EQUAL                 = 0,
  LESS_THAN             = 1,
  GREATER_THAN          = 2,
  GREATER_THAN_OR_EQUAL = 3,
  LESS_THAN_OR_EQUAL    = 4,
  NOT_EQUAL             = 5
};

enum condition
{
  WALKING               = 0,
  SWIMMING              = 1,
  FIRE_WALKING          = 2,
  TOUCHING              = 3,
  BLOCKED               = 4,
  ALIGNED               = 5,
  ALIGNED_NS            = 6,
  ALIGNED_EW            = 7,
  LASTSHOT              = 8,
  LASTTOUCH             = 9,
  RIGHTPRESSED          = 10,
  LEFTPRESSED           = 11,
  UPPRESSED             = 12,
  DOWNPRESSED           = 13,
  SPACEPRESSED          = 14,
  DELPRESSED            = 15,
  MUSICON               = 16,
  SOUNDON               = 17
};

enum chest_contents
{
  ITEM_KEY              = 1,
  ITEM_COINS            = 2,
  ITEM_LIFE             = 3,
  ITEM_AMMO             = 4,
  ITEM_GEMS             = 5,
  ITEM_HEALTH           = 6,
  ITEM_POTION           = 7,
  ITEM_RING             = 8,
  ITEM_LOBOMBS          = 9,
  ITEM_HIBOMBS          = 10
};

enum potion
{
  POTION_DUD            = 0,
  POTION_INVINCO        = 1,
  POTION_BLAST          = 2,
  POTION_SMALL_HEALTH   = 3,
  POTION_BIG_HEALTH     = 4,
  POTION_POISON         = 5,
  POTION_BLIND          = 6,
  POTION_KILL           = 7,
  POTION_FIREWALK       = 8,
  POTION_DETONATE       = 9,
  POTION_BANISH         = 10,
  POTION_SUMMON         = 11,
  POTION_AVALANCHE      = 12,
  POTION_FREEZE         = 13,
  POTION_WIND           = 14,
  POTION_SLOW           = 15
};

enum board_target
{
  TARGET_NONE           = 0,
  TARGET_POSITION       = 1,
  TARGET_ENTRANCE       = 2,
  TARGET_TELEPORT       = 3
};

__M_END_DECLS

#endif // __DATA_H
