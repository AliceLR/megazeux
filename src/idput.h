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

/* Declarations for IDPUT.ASM */

#ifndef __IDPUT_H
#define __IDPUT_H

#include "compat.h"

__M_BEGIN_DECLS

#include "world_struct.h"
#include "board_struct.h"
#include "data.h"

void id_put(Board *src_board, unsigned char x_pos, unsigned char y_pos,
  int array_x, int array_y, int ovr_x, int ovr_y);
void draw_game_window(Board *src_board, int array_x, int array_y);
void draw_edit_window(Board *src_board, int array_x, int array_y,
 int window_height);

unsigned char get_id_char(Board *src_board, int id_offset);
unsigned char get_id_color(Board *src_board, int id_offset);

__M_END_DECLS

#endif // __IDPUT_H
