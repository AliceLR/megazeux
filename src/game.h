/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1999 Charles Goetzman
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

/* Declarations for GAME.CPP */

#ifndef __GAME_H
#define __GAME_H

#include "compat.h"

__M_BEGIN_DECLS

#include "world_struct.h"

void title_screen(World *mzx_world);
void draw_viewport(World *src_board);
void calculate_xytop(World *mzx_world, int *x, int *y);
void play_game(World *mzx_world, int fadein);
int move_player(World *mzx_world, int dir);
int grab_item(World *mzx_world, int offset, int dir);
void set_mesg(World *mzx_world, char *str);
void set_mesg_direct(Board *src_board, char *str);
void rotate(World *mzx_world, int x, int y, int dir);
void check_find_player(World *mzx_world);
void find_player(World *mzx_world);
int take_key(World *mzx_world, int color);
int give_key(World *mzx_world, int color);
void draw_debug_box(World *mzx_world, int x, int y, int d_x, int d_y);

extern int pal_update;
extern char *world_ext[2];

__M_END_DECLS

#endif // __GAME_H
