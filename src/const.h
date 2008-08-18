/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1999 Charles Goetzman
 * Copyright (C) 2004 Gilead Kutnick
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

/* Constants */
#ifndef __CONST_H
#define __CONST_H

#include "compat.h"

__M_BEGIN_DECLS

#ifndef __WIN32__
#include <limits.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 512
#endif

#ifndef MAX_PATH
#define MAX_PATH PATH_MAX
#endif

#define OVERLAY_OFF           0
#define OVERLAY_ON            1
#define OVERLAY_STATIC        2
#define OVERLAY_TRANS         3

#define EXPL_LEAVE_SPACE      0
#define EXPL_LEAVE_ASH        1
#define EXPL_LEAVE_FIRE       2

#define CAN_SAVE              0
#define CANT_SAVE             1
#define CAN_SAVE_ON_SENSOR    2

#define FOREST_TO_EMPTY       0
#define FOREST_TO_FLOOR       1

#define FIRE_BURNS_LIMITED    0
#define FIRE_BURNS_FOREVER    1

#define NO_BOARD              255
#define NO_ENDGAME_BOARD      255
#define NO_DEATH_BOARD        255
#define DEATH_SAME_POS        254

#define PLAYER_BULLET         0
#define NEUTRAL_BULLET        1
#define ENEMY_BULLET          2

#define DIR_IDLE              0
#define DIR_NONE              0
#define DIR_N                 1
#define DIR_S                 2
#define DIR_E                 3
#define DIR_W                 4
#define DIR_RANDNS            5
#define DIR_RANDEW            6
#define DIR_RANDNE            7
#define DIR_RANDNB            8
#define DIR_SEEK              9
#define DIR_RANDANY           10
#define DIR_UNDER             11
#define DIR_ANYDIR            12
#define DIR_FLOW              13
#define DIR_NODIR             14
#define DIR_RANDB             15
// These are added to the above or checked using AND.
#define DIR_RANDP             16
#define DIR_CW                32
#define DIR_OPP               64
#define DIR_CCW               96
#define DIR_RANDNOT           128

#define HORIZONTAL            0
#define VERTICAL              1

#define NO_KEY                127

#define NO_PROTECTION         0
#define NO_SAVING             1
#define NO_EDITING            2
#define NO_PLAYING            3

// "SIZE" includes terminating \0
// This is legacy, for world format only
#define FILENAME_SIZE         13
#define NUM_BOARDS            250
#define BOARD_NAME_SIZE       25
// This is legacy, for status counters only
#define COUNTER_NAME_SIZE     15
#define NUM_KEYS              16

#define ARRAY_DIR_N           -100
#define ARRAY_DIR_S           100
#define ARRAY_DIR_E           1
#define ARRAY_DIR_W           -1

// Attribute flags
#define A_PUSHNS              1
#define A_PUSHEW              2
#define A_PUSHABLE            3
#define A_ITEM                4
#define A_UPDATE              8
#define A_HURTS               16
#define A_UNDER               32
#define A_ENTRANCE            64
#define A_EXPLODE             128
#define A_BLOW_UP             256
#define A_SHOOTABLE           512
#define A_ENEMY               1024
#define A_AFFECT_IF_STOOD     2048
#define A_SPEC_SHOT           4096
#define A_SPEC_PUSH           8192
#define A_SPEC_BOMB           16384
#define A_SPEC_STOOD          32768

// ID number for storage of the global robot
#define GLOBAL_ROBOT          0
#define NUM_STATUS_CNTRS      6

__M_END_DECLS

#endif // __CONST_H
