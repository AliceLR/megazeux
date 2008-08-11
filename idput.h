/* $Id: idput.h,v 1.2 1999/01/17 20:35:41 mental Exp $
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Declarations for IDPUT.ASM */

#include "world.h"
#include "board.h"

#ifndef IDPUT_H
#define IDPUT_H

void id_put(Board *src_board, unsigned char x_pos, unsigned char y_pos,
  int array_x, int array_y, int ovr_x, int ovr_y);
void draw_game_window(Board *src_board, int array_x, int array_y);
void draw_edit_window(Board *src_board, int array_x, int array_y,
 int window_height);

unsigned char get_id_char(Board *src_board, int id_offset);
unsigned char get_id_color(Board *src_board, int id_offset);

#endif
