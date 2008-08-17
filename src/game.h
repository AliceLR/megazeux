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

#ifndef GAME_H
#define GAME_H

#include "world.h"

void place_player(World *mzx_world, int x, int y, int dir);
void load_world_selection(World *mzx_world);
void load_world_file(World *mzx_world, char *name);
void title_screen(World *mzx_world);
void draw_viewport(World *src_board);
void update_variables(World *mzx_world, int slowed);
void calculate_xytop(World *mzx_world, int *x, int *y);
void update_player(World *mzx_world);
void game_settings(World *mzx_world);
void play_game(World *mzx_world, int fadein);
int move_player(World *mzx_world, int dir);
// Dir is for transporter
void give_potion(World *mzx_world, int type);
int grab_item(World *mzx_world, int offset, int dir);
void show_status(World *mzx_world);
void show_counter(World *mzx_world, char *str, int x, int y,
 int skip_if_zero);
int update(World *mzx_world, int game, int *fadein);
void set_mesg(World *mzx_world, char *str);
void set_3_mesg(Board *src_board, char *str1, int num, char *str2);
void set_mesg_direct(Board *src_board, char *str);
void rotate(World *mzx_world, int x, int y, int dir);
void check_find_player(World *mzx_world);
void find_player(World *mzx_world);
int take_key(World *mzx_world, int color);
int give_key(World *mzx_world, int color);
void draw_debug_box(World *mzx_world, int x, int y, int d_x, int d_y);

extern int pal_update;
extern char *world_ext[2];

#endif
