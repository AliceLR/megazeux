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

#ifndef __EDITOR_BOARD_H
#define __EDITOR_BOARD_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "../board_struct.h"
#include "../world_struct.h"

void replace_current_board(World *mzx_world, char *name);
Board *create_blank_board(void);
void save_board_file(Board *cur_board, char *name);
void change_board_size(Board *src_board, int new_width, int new_height);

__M_END_DECLS

#endif // __EDITOR_BOARD_H
