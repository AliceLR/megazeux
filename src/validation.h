/* MegaZeux
 *
 * Copyright (C) 2012 Alice Lauren Rowan <petrifiedrowan@gmail.com>
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

/* VALIDATION.H- Declarations for VALIDATION.C */

#ifndef __VALIDATION_H
#define __VALIDATION_H

#include "compat.h"

__M_BEGIN_DECLS

#include "world.h"
#include "const.h"

#include <stdlib.h>

enum val_result
{
  VAL_SUCCESS,
  VAL_VERSION,    // Version issue
  VAL_INVALID,    // Failed validation
  VAL_TRUNCATED,  // Passed validation until it hit EOF
  VAL_MISSING,    // file or ptr location in file does not exist
  VAL_ABORTED,    // Load aborted by user
  VAL_NEED_UNLOCK // Unlock world file.
};

#define NUM_VAL_ERRORS 23

enum val_error
{
  FILE_DOES_NOT_EXIST             = 0,
  SAVE_FILE_INVALID               = 1,
  SAVE_VERSION_OLD                = 2,
  SAVE_VERSION_TOO_RECENT         = 3,
  WORLD_FILE_INVALID              = 4,
  WORLD_FILE_VERSION_OLD          = 5,
  WORLD_FILE_VERSION_TOO_RECENT   = 6,
  WORLD_PASSWORD_PROTECTED        = 7,
  WORLD_LOCKED                    = 8,
  WORLD_BOARD_MISSING             = 9,
  WORLD_BOARD_CORRUPT             = 10,
  WORLD_BOARD_TRUNCATED_SAFE      = 11,
  WORLD_ROBOT_MISSING             = 12,
  BOARD_FILE_INVALID              = 13,
  BOARD_ROBOT_CORRUPT             = 14,
  BOARD_SCROLL_CORRUPT            = 15,
  BOARD_SENSOR_CORRUPT            = 16,
  MZM_DOES_NOT_EXIST              = 17,
  MZM_FILE_INVALID                = 18,
  MZM_FILE_FROM_SAVEGAME          = 19,
  MZM_FILE_VERSION_TOO_RECENT     = 20,
  MZM_ROBOT_CORRUPT               = 21,
  LOAD_BC_CORRUPT                 = 22,
};

CORE_LIBSPEC void val_error(enum val_error error_id, int value);
CORE_LIBSPEC void val_error_str(enum val_error error_id, int value, char *string);
CORE_LIBSPEC FILE * val_fopen(const char *filename);

int get_error_count(enum val_error error_id);
void set_validation_suppression(enum val_error error_id, int value);

CORE_LIBSPEC enum val_result validate_world_file(const char *filename, int savegame,
 int *end_of_global_offset, int decrypt_attempted);
//CORE_LIBSPEC enum val_result validate_legacy_bytecode(char *bc, int program_length);

__M_END_DECLS

#endif // __VALIDATION_H
