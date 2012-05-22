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

CORE_LIBSPEC void id_put(struct board *src_board, unsigned char x_pos,
 unsigned char y_pos, int array_x, int array_y, int ovr_x, int ovr_y);
CORE_LIBSPEC void draw_game_window(struct board *src_board,
 int array_x, int array_y);

CORE_LIBSPEC unsigned char get_id_char(struct board *src_board, int id_offset);
CORE_LIBSPEC unsigned char get_id_color(struct board *src_board, int id_offset);
CORE_LIBSPEC unsigned char get_id_board_color(struct board *src_board, int id_offset, int ignore_under);
CORE_LIBSPEC unsigned char get_id_under_char(struct board *src_board, int id_offset);
CORE_LIBSPEC unsigned char get_id_under_color(struct board *src_board, int id_offset);


#define bullet_char  306
#define player_char  318
#define player_color 322

CORE_LIBSPEC extern unsigned char id_chars[455];
CORE_LIBSPEC extern unsigned char id_dmg[128];
CORE_LIBSPEC extern unsigned char bullet_color[3];
CORE_LIBSPEC extern unsigned char missile_color;

__M_END_DECLS

#endif // __IDPUT_H
