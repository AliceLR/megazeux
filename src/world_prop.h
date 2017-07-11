/* MegaZeux
 *
 * Copyright (C) 2017 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __WORLD_PROP_H
#define __WORLD_PROP_H

#include "compat.h"

__M_BEGIN_DECLS

#include "memfile.h"

// Data and functions for world/board/robot IO.

enum file_prop
{
  FPROP_NONE                  = 0x0000,
  FPROP_WORLD_INFO            = 0x0001, // properties file
  FPROP_WORLD_GLOBAL_ROBOT    = 0x0004, // properties file
  FPROP_WORLD_SFX             = 0x0008, // data, NUM_SFX * SFX_SIZE
  FPROP_WORLD_CHARS           = 0x000A, // data, 3584*15
  FPROP_WORLD_PAL             = 0x000C, // data, SMZX_PAL_SIZE * 3
  FPROP_WORLD_PAL_INDEX       = 0x000D, // data, 1024
  FPROP_WORLD_PAL_INTENSITY   = 0x000E, // data, SMZX_PAL_SIZE * 1

  FPROP_WORLD_VCO             = 0x0080, // data
  FPROP_WORLD_VCH             = 0x0081, // data
  FPROP_WORLD_SPRITES         = 0x0084, // properties file
  FPROP_WORLD_COUNTERS        = 0x0085, // counter format, use stream
  FPROP_WORLD_STRINGS         = 0x0086, // string format, use stream

  FPROP_BOARD_INFO            = 0x0100, // properties file (board_id)
  FPROP_BOARD_BID             = 0x0101, // data
  FPROP_BOARD_BPR             = 0x0102, // data
  FPROP_BOARD_BCO             = 0x0103, // data
  FPROP_BOARD_UID             = 0x0104, // data
  FPROP_BOARD_UPR             = 0x0105, // data
  FPROP_BOARD_UCO             = 0x0106, // data
  FPROP_BOARD_OCH             = 0x0107, // data
  FPROP_BOARD_OCO             = 0x0108, // data

  FPROP_ROBOT                 = 0x1000, // properties file (board_id + robot_id)
  FPROP_SCROLL                = 0x2000, // properties file (board_id + robot_id)
  FPROP_SENSOR                = 0x3000  // properties file (board_id + robot_id)
};


#define PROP_HEADER_SIZE 6


// IF YOU ADD ANYTHING, MAKE SURE THIS GETS UPDATED!
#define COUNT_WORLD_PROPS (              1 + 3 +   4 + 16 + 1)
#define BOUND_WORLD_PROPS (BOARD_NAME_SIZE + 5 + 455 + 24 + \
 (NUM_STATUS_COUNTERS * COUNTER_NAME_SIZE))

#define COUNT_SAVE_PROPS  ( 2 +   4 +  30 +          3 +        1)
#define BOUND_SAVE_PROPS  ( 2 +   9 + 100 + 3*MAX_PATH + NUM_KEYS)

// 600
#define WORLD_PROP_SIZE \
 (BOUND_WORLD_PROPS + PROP_HEADER_SIZE * COUNT_WORLD_PROPS + 2)

// 1783
#define SAVE_PROP_SIZE \
 (BOUND_SAVE_PROPS + PROP_HEADER_SIZE * COUNT_SAVE_PROPS)

// 2383
#define WORLD_PROP_TOTAL_SIZE (WORLD_PROP_SIZE + SAVE_PROP_SIZE)

enum world_prop
{
  WPROP_EOF                       = 0x0000, // Size

  // Header redundant properties      4 (6)     5 + BOARD_NAME_SIZE (+2)
  WPROP_WORLD_NAME                = 0x0001, //  BOARD_NAME_SIZE
  WPROP_WORLD_VERSION             = 0x0002, //   2
  WPROP_FILE_VERSION              = 0x0003, //   2
  WPROP_SAVE_START_BOARD          = 0x0004, //  (1)
  WPROP_SAVE_TEMPORARY_BOARD      = 0x0005, //  (1)
  WPROP_NUM_BOARDS                = 0x0008, //   1

  // ID Chars                            4     455
  WPROP_ID_CHARS                  = 0x0010, // 323
  WPROP_ID_MISSILE_COLOR          = 0x0011, //   1
  WPROP_ID_BULLET_COLOR           = 0x0012, //   3
  WPROP_ID_DMG                    = 0x0013, // 128

  // Status counters           NUM_STATUS_COUNTERS * COUNTER_NAME_SIZE
  WPROP_STATUS_COUNTERS           = 0x0018,

  // Global properties                  16      24
  WPROP_EDGE_COLOR                = 0x0020, //   1
  WPROP_FIRST_BOARD               = 0x0021, //   1
  WPROP_ENDGAME_BOARD             = 0x0022, //   1
  WPROP_DEATH_BOARD               = 0x0023, //   1
  WPROP_ENDGAME_X                 = 0x0024, //   2
  WPROP_ENDGAME_Y                 = 0x0025, //   2
  WPROP_GAME_OVER_SFX             = 0x0026, //   1
  WPROP_DEATH_X                   = 0x0027, //   2
  WPROP_DEATH_Y                   = 0x0028, //   2
  WPROP_STARTING_LIVES            = 0x0029, //   2
  WPROP_LIVES_LIMIT               = 0x002A, //   2
  WPROP_STARTING_HEALTH           = 0x002B, //   2
  WPROP_HEALTH_LIMIT              = 0x002C, //   2
  WPROP_ENEMY_HURT_ENEMY          = 0x002D, //   1
  WPROP_CLEAR_ON_EXIT             = 0x002E, //   1
  WPROP_ONLY_FROM_SWAP            = 0x002F, //   1

  // Temporarily save-only               4       9
  WPROP_SMZX_MODE                 = 0x8030, //   1
  WPROP_VLAYER_WIDTH              = 0x8031, //   2
  WPROP_VLAYER_HEIGHT             = 0x8032, //   2
  WPROP_VLAYER_SIZE               = 0x8033, //   4

  // Save properties                  30+4    100 + 3 MAX_PATH + NUM_KEYS
  WPROP_REAL_MOD_PLAYING          = 0x8040, // MAX_PATH
  WPROP_MZX_SPEED                 = 0x8041, //   1
  WPROP_LOCK_SPEED                = 0x8042, //   1
  WPROP_COMMANDS                  = 0x8043, //   4
  WPROP_COMMANDS_STOP             = 0x8044, //   4
  WPROP_SAVED_POSITIONS           = 0x8048, //  40 (2+2+1)*8
  WPROP_UNDER_PLAYER              = 0x8049, //   3 (1+1+1)
  WPROP_PLAYER_RESTART_X          = 0x804A, //   2
  WPROP_PLAYER_RESTART_Y          = 0x804B, //   2
  WPROP_SAVED_PL_COLOR            = 0x804C, //   1
  WPROP_KEYS                      = 0x804D, // NUM_KEYS
  WPROP_BLIND_DUR                 = 0x8050, //   1
  WPROP_FIREWALKER_DUR            = 0x8051, //   1
  WPROP_FREEZE_TIME_DUR           = 0x8052, //   1
  WPROP_SLOW_TIME_DUR             = 0x8053, //   1
  WPROP_WIND_DUR                  = 0x8054, //   1
  WPROP_SCROLL_BASE_COLOR         = 0x8058, //   1
  WPROP_SCROLL_CORNER_COLOR       = 0x8059, //   1
  WPROP_SCROLL_POINTER_COLOR      = 0x805A, //   1
  WPROP_SCROLL_TITLE_COLOR        = 0x805B, //   1
  WPROP_SCROLL_ARROW_COLOR        = 0x805C, //   1
  WPROP_MESG_EDGES                = 0x8060, //   1
  WPROP_BI_SHOOT_STATUS           = 0x8061, //   1
  WPROP_BI_MESG_STATUS            = 0x8062, //   1
  WPROP_FADED                     = 0x8063, //   1
  WPROP_INPUT_FILE_NAME           = 0x8070, // MAX_PATH
  WPROP_INPUT_POS                 = 0x8074, //   4
  WPROP_FREAD_DELIMITER           = 0x8075, //   4
  WPROP_OUTPUT_FILE_NAME          = 0x8078, // MAX_PATH
  WPROP_OUTPUT_POS                = 0x807C, //   4
  WPROP_FWRITE_DELIMITER          = 0x807D, //   4
  WPROP_MULTIPLIER                = 0x8080, //   4
  WPROP_DIVIDER                   = 0x8081, //   4
  WPROP_C_DIVISIONS               = 0x8082, //   4
};


#define COUNT_SPRITE_PROPS 15
#define BOUND_SPRITE_PROPS 60
#define COUNT_SPRITE_ONCE_PROPS 3
#define BOUND_SPRITE_ONCE_PROPS 12

// 30746
#define SPRITE_PROPS_SIZE \
 ((BOUND_SPRITE_PROPS + PROP_HEADER_SIZE * COUNT_SPRITE_PROPS) * MAX_SPRITES + \
 (BOUND_SPRITE_ONCE_PROPS + PROP_HEADER_SIZE * COUNT_SPRITE_ONCE_PROPS) + 2)

enum sprite_prop
{
  SPROP_EOF               = 0x00,

  // For each sprite
  SPROP_SET_ID            = 0x01, // 4, used to select a sprite #
  SPROP_X                 = 0x02, // 2 (actually saving everything as 4)
  SPROP_Y                 = 0x03, // 2
  SPROP_REF_X             = 0x04, // 2
  SPROP_REF_Y             = 0x05, // 2
  SPROP_COLOR             = 0x06, // 1
  SPROP_FLAGS             = 0x07, // 1
  SPROP_WIDTH             = 0x08, // 1
  SPROP_HEIGHT            = 0x09, // 1
  SPROP_COL_X             = 0x0A, // 1
  SPROP_COL_Y             = 0x0B, // 1
  SPROP_COL_WIDTH         = 0x0C, // 1
  SPROP_COL_HEIGHT        = 0x0D, // 1
  SPROP_TRANSPARENT_COLOR = 0x0E, // 4
  SPROP_CHARSET_OFFSET    = 0x0F, // 4

  // Only once
  SPROP_ACTIVE_SPRITES    = 0x8000, // 1
  SPROP_SPRITE_Y_ORDER    = 0x8001, // 1
  SPROP_COLLISION_COUNT   = 0x8002, // 2
};


#define COUNT_BOARD_PROPS (              1 +  7 +          3 + 25)
#define BOUND_BOARD_PROPS (BOARD_NAME_SIZE + 10 + 3*MAX_PATH + 25)
#define BOARD_PROPS_SIZE \
 (COUNT_BOARD_PROPS * PROP_HEADER_SIZE + BOUND_BOARD_PROPS + 2)

#define COUNT_BOARD_SAVE_PROPS (             2 + 15)
#define BOUND_BOARD_SAVE_PROPS (2*ROBOT_MAX_TR + 25)
#define BOARD_SAVE_PROPS_SIZE \
 (COUNT_BOARD_SAVE_PROPS * PROP_HEADER_SIZE + BOUND_BOARD_SAVE_PROPS)

enum board_prop {
  BPROP_EOF                   = 0x0000,

  // Essential                       8     10 + BOARD_NAME_SIZE
  BPROP_BOARD_NAME            = 0x0001, // BOARD_NAME_SIZE
  BPROP_BOARD_WIDTH           = 0x0002, // 2
  BPROP_BOARD_HEIGHT          = 0x0003, // 2
  BPROP_OVERLAY_MODE          = 0x0004, // 1
  BPROP_NUM_ROBOTS            = 0x0005, // 1
  BPROP_NUM_SCROLLS           = 0x0006, // 1
  BPROP_NUM_SENSORS           = 0x0007, // 1
  BPROP_FILE_VERSION          = 0x0008, // 2

  // Non-essential                  25     25 + 3 MAX_PATH
  BPROP_MOD_PLAYING           = 0x0010, // MAX_PATH
  BPROP_VIEWPORT_X            = 0x0011, // 1
  BPROP_VIEWPORT_Y            = 0x0012, // 1
  BPROP_VIEWPORT_WIDTH        = 0x0013, // 1
  BPROP_VIEWPORT_HEIGHT       = 0x0014, // 1
  BPROP_CAN_SHOOT             = 0x0015, // 1
  BPROP_CAN_BOMB              = 0x0016, // 1
  BPROP_FIRE_BURN_BROWN       = 0x0017, // 1
  BPROP_FIRE_BURN_SPACE       = 0x0018, // 1
  BPROP_FIRE_BURN_FAKES       = 0x0019, // 1
  BPROP_FIRE_BURN_TREES       = 0x001A, // 1
  BPROP_EXPLOSIONS_LEAVE      = 0x001B, // 1
  BPROP_SAVE_MODE             = 0x001C, // 1
  BPROP_FOREST_BECOMES        = 0x001D, // 1
  BPROP_COLLECT_BOMBS         = 0x001E, // 1
  BPROP_FIRE_BURNS            = 0x001F, // 1
  BPROP_BOARD_N               = 0x0020, // 1
  BPROP_BOARD_S               = 0x0021, // 1
  BPROP_BOARD_E               = 0x0022, // 1
  BPROP_BOARD_W               = 0x0023, // 1
  BPROP_RESTART_IF_ZAPPED     = 0x0024, // 1
  BPROP_TIME_LIMIT            = 0x0025, // 2
  BPROP_PLAYER_NS_LOCKED      = 0x0026, // 1
  BPROP_PLAYER_EW_LOCKED      = 0x0027, // 1
  BPROP_PLAYER_ATTACK_LOCKED  = 0x0028, // 1
  BPROP_RESET_ON_ENTRY        = 0x0029, // 1
  BPROP_CHARSET_PATH          = 0x002A, // MAX_PATH
  BPROP_PALETTE_PATH          = 0x002B, // MAX_PATH

  // Save                           17     25 + 2 ROBOT_MAX_TR
  BPROP_SCROLL_X              = 0x0100, // 2
  BPROP_SCROLL_Y              = 0x0101, // 2
  BPROP_LOCKED_X              = 0x0102, // 2
  BPROP_LOCKED_Y              = 0x0103, // 2
  BPROP_PLAYER_LAST_DIR       = 0x0104, // 1
  BPROP_LAZWALL_START         = 0x010A, // 1
  BPROP_LAST_KEY              = 0x010B, // 1
  BPROP_NUM_INPUT             = 0x010C, // 4
  BPROP_INPUT_SIZE            = 0x010D, // 4 (IS SEPARATE FROM INPUT_STRING)
  BPROP_INPUT_STRING          = 0x010E, // ROBOT_MAX_TR
  BRPOP_BOTTOM_MESG           = 0x0110, // ROBOT_MAX_TR
  BPROP_BOTTOM_MESG_TIMER     = 0x0111, // 1
  BPROP_BOTTOM_MESG_ROW       = 0x0112, // 1
  BPROP_BOTTOM_MESG_COL       = 0x0113, // 1
  BPROP_VOLUME                = 0x0114, // 1
  BPROP_VOLUME_INC            = 0x0115, // 1
  BPROP_VOLUME_TARGET         = 0x0116, // 1
};


#define COUNT_ROBOT_PROPS ( 4 + 1)
#define BOUND_ROBOT_PROPS (20 + 0) // +prog

#define ROBOT_PROPS_SIZE \
 (PROP_HEADER_SIZE * COUNT_ROBOT_PROPS + BOUND_ROBOT_PROPS + 2)

#define COUNT_ROBOT_SAVE_PROPS (11 + 2 +    1 + 1 + 1)
#define BOUND_ROBOT_SAVE_PROPS (17 + 8 + 4*32 + 0 + 1) // +stack

#define ROBOT_SAVE_PROPS_SIZE \
 (PROP_HEADER_SIZE * COUNT_ROBOT_SAVE_PROPS + BOUND_ROBOT_SAVE_PROPS)

#ifdef CONFIG_DEBYTECODE
#ifdef CONFIG_EDITOR

#define COUNT_ROBOT_DBC_EDIT_PROPS (2)
#define BOUND_ROBOT_DBC_EDIT_PROPS (0) // +prog +map

#define ROBOT_DBC_EDIT_PROPS_SIZE \
 (PROP_HEADER_SIZE * COUNT_ROBOT_DBC_EDIT_PROPS + BOUND_ROBOT_DBC_EDIT_PROPS)

#endif
#endif

enum robot_prop {
  RPROP_EOF               = 0x0000,
  RPROP_ROBOT_NAME        = 0x0001, // 15
  RPROP_ROBOT_CHAR        = 0x0002, // 1
  RPROP_XPOS              = 0x0003, // 2
  RPROP_YPOS              = 0x0004, // 2
//RPROP_PROGRAM           = 0x0005, // 4

  // Slated for separation
#ifdef CONFIG_DEBYTECODE
  RPROP_PROGRAM_MAP       = 0x00FD, // variable
  RPROP_PROGRAM_SOURCE    = 0x00FE, // variable
#endif
  RPROP_PROGRAM_BYTECODE  = 0x00FF, // variable

  // Legacy world, now save
  RPROP_CUR_PROG_LINE     = 0x0100, // 4
  RPROP_POS_WITHIN_LINE   = 0x0101, // 4
  RPROP_ROBOT_CYCLE       = 0x0102, // 1
  RPROP_CYCLE_COUNT       = 0x0103, // 1
  RPROP_BULLET_TYPE       = 0x0104, // 1
  RPROP_IS_LOCKED         = 0x0105, // 1
  RPROP_CAN_LAVAWALK      = 0x0106, // 1
  RPROP_WALK_DIR          = 0x0107, // 1
  RPROP_LAST_TOUCH_DIR    = 0x0108, // 1
  RPROP_LAST_SHOT_DIR     = 0x0109, // 1
  RPROP_STATUS            = 0x010A, // 1

  // Legacy save
  RPROP_LOOP_COUNT        = 0x010B, // 4
  RPROP_LOCALS            = 0x010C, // 4*32
  RPROP_STACK_POINTER     = 0x0110, // 4
  RPROP_STACK             = 0x0111, // variable

  // New
  RPROP_CAN_GOOPWALK      = 0x0120, // 1
};

#define SCROLL_PROPS_SIZE (PROP_HEADER_SIZE * 2 + 2)
enum scroll_prop {
  SCRPROP_EOF             = 0x00,
  SCRPROP_NUM_LINES       = 0x01, // 2
  SCRPROP_MESG            = 0x02, // variable
};

#define SENSOR_PROPS_SIZE (PROP_HEADER_SIZE * 3 + 31)
enum sensor_prop {
  SENPROP_EOF             = 0x00,
  SENPROP_SENSOR_NAME     = 0x01, // 15
  SENPROP_SENSOR_CHAR     = 0x02, //  1
  SENPROP_ROBOT_TO_MESG   = 0x03, // 15
};


// This function is used to save properties files in world saving.
// There are no safety checks here. USE THE BOUNDING MACROS WHEN ALLOCATING.
static inline void save_prop_eof(struct memfile *mf)
{
  mfputw(0, mf);
}

static inline void save_prop_c(int ident, int value, struct memfile *mf)
{
  mfputw(ident, mf);
  mfputd(1, mf);
  mfputc(value, mf);
}

static inline void save_prop_w(int ident, int value, struct memfile *mf)
{
  mfputw(ident, mf);
  mfputd(2, mf);
  mfputw(value, mf);
}

static inline void save_prop_d(int ident, int value, struct memfile *mf)
{
  mfputw(ident, mf);
  mfputd(4, mf);
  mfputd(value, mf);
}

static inline void save_prop_s(int ident, const void *src, size_t len,
 size_t count, struct memfile *mf)
{
  mfputw(ident, mf);
  mfputd(len * count, mf);
  mfwrite(src, len, count, mf);
}

static inline void save_prop_v(int ident, size_t len, struct memfile *prop,
 struct memfile *mf)
{
  mfputw(ident, mf);
  mfputd(len, mf);
  mfopen_static(mf->current, len, prop);
  mf->current += len;
}

static inline int load_prop_int(int length, struct memfile *prop)
{
  switch(length)
  {
    case 1:
      return mfgetc(prop);

    case 2:
      return mfgetw(prop);

    case 4:
      return mfgetd(prop);

    default:
      return 0;
  }
}

// This function is used to read properties files in world loading.
static inline int next_prop(struct memfile *prop, int *ident, int *length,
 struct memfile *mf)
{
  char *end = mf->end;
  char *cur;
  int len;

  if((end - mf->current)<PROP_HEADER_SIZE)
  {
    return 0;
  }

  *ident = mfgetw(mf);
  len = mfgetd(mf);
  cur = mf->current;

  if((end - cur)<len)
  {
    return 0;
  }

  *length = len;
  prop->current = cur;
  prop->start = cur;
  prop->end = cur + len;

  mf->current += len;
  return 1;
}

__M_END_DECLS

#endif // __WORLD_PROP_H
