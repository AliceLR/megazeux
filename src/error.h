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

/* Declaration for ERROR.CPP */

#ifndef __ERROR_H
#define __ERROR_H

#include "compat.h"

#define ERROR_OPT_FAIL  1
#define ERROR_OPT_RETRY 2
#define ERROR_OPT_EXIT  4
#define ERROR_OPT_OK    8
#define ERROR_OPT_HELP  16

__M_BEGIN_DECLS

enum error_code
{
  E_DEFAULT,
  E_FILE_DOES_NOT_EXIST,
  E_SAVE_FILE_INVALID,
  E_SAVE_VERSION_OLD,
  E_SAVE_VERSION_TOO_RECENT,
  E_WORLD_FILE_INVALID,
  E_WORLD_FILE_VERSION_OLD,
  E_WORLD_FILE_VERSION_TOO_RECENT,
  E_WORLD_PASSWORD_PROTECTED,
  E_WORLD_DECRYPT_WRITE_PROTECTED,
  E_WORLD_LOCKED,
  E_WORLD_IO_POST_VALIDATION,
  E_WORLD_IO_SAVING,
  E_WORLD_BOARD_MISSING,
  E_WORLD_BOARD_CORRUPT,
  E_WORLD_BOARD_TRUNCATED_SAFE,
  E_WORLD_ROBOT_MISSING,
  E_BOARD_FILE_INVALID,
  E_BOARD_ROBOT_CORRUPT,
  E_BOARD_SCROLL_CORRUPT,
  E_BOARD_SENSOR_CORRUPT,
  E_BOARD_SUMMARY,
  E_MZM_DOES_NOT_EXIST,
  E_MZM_FILE_INVALID,
  E_MZM_FILE_FROM_SAVEGAME,
  E_MZM_FILE_VERSION_TOO_RECENT,
  E_MZM_ROBOT_CORRUPT,
  E_LOAD_BC_CORRUPT,
  E_ZIP,
#ifdef CONFIG_DEBYTECODE
  E_DBC_WORLD_OVERWRITE_OLD,
  E_DBC_SAVE_ROBOT_UNSUPPORTED,
#endif
  NUM_ERROR_CODES
};

CORE_LIBSPEC int error(const char *string, unsigned int type,
 unsigned int options, unsigned int code);

CORE_LIBSPEC int error_message(enum error_code id, int parameter,
 const char *string);

int get_and_reset_error_count(void);
void set_error_suppression(enum error_code id, int value);
void reset_error_suppression(void);

__M_END_DECLS

#endif // __ERROR_H
