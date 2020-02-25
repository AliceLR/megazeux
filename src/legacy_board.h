/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef __LEGACY_BOARD_H
#define __LEGACY_BOARD_H

#include "compat.h"

__M_BEGIN_DECLS

#include "const.h"
#include "world_struct.h"

CORE_LIBSPEC int legacy_load_board_direct(struct world *mzx_world,
 struct board *cur_board, FILE *fp, int data_size, int savegame, int version);

CORE_LIBSPEC struct board *legacy_load_board_allocate(struct world *mzx_world,
 FILE *fp, int data_offset, int data_size, int savegame, int file_version);

__M_END_DECLS

#endif // __LEGACY_BOARD_H
