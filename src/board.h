/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef __BOARD_H
#define __BOARD_H

#include "compat.h"

__M_BEGIN_DECLS

#include "const.h"
#include "board_struct.h"
#include "world_struct.h"
#include "zip.h"

CORE_LIBSPEC int save_board(struct world *mzx_world, struct board *cur_board,
 struct zip_archive *zp, int savegame, int file_version, int board_id);

CORE_LIBSPEC struct board *load_board_allocate(struct world *mzx_world,
 struct zip_archive *zp, int savegame, int file_version, unsigned int board_id);

CORE_LIBSPEC void clear_board(struct board *cur_board);

struct board *duplicate_board(struct world *mzx_world,
 struct board *src_board);

void dummy_board(struct board *cur_board);

int find_board(struct world *mzx_world, char *name);

#ifdef CONFIG_EDITOR
CORE_LIBSPEC int load_board_direct(struct world *mzx_world,
 struct board *cur_board,  struct zip_archive *zp, int savegame,
 int file_version, unsigned int board_id);
#endif // CONFIG_EDITOR

__M_END_DECLS

#endif // __BOARD_H
