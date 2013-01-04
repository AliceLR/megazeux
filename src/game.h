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

CORE_LIBSPEC void title_screen(struct world *mzx_world);
CORE_LIBSPEC void find_player(struct world *mzx_world);

void set_intro_mesg_timer(unsigned int time);
void calculate_xytop(struct world *mzx_world, int *x, int *y);
int move_player(struct world *mzx_world, int dir);
int grab_item(struct world *mzx_world, int offset, int dir);
void set_mesg(struct world *mzx_world, const char *str);
void set_mesg_direct(struct board *src_board, const char *str);
void rotate(struct world *mzx_world, int x, int y, int dir);
void check_find_player(struct world *mzx_world);
int take_key(struct world *mzx_world, int color);
int give_key(struct world *mzx_world, int color);

extern bool pal_update;
extern bool editing;

#ifdef CONFIG_EDITOR
CORE_LIBSPEC void play_game(struct world *mzx_world);
CORE_LIBSPEC void draw_viewport(struct world *src_board);
CORE_LIBSPEC void set_caption(struct world *mzx_world, struct board *board,
 struct robot *robot, int editor);

CORE_LIBSPEC extern bool debug_mode;
CORE_LIBSPEC extern const char *const world_ext[2];
CORE_LIBSPEC extern void (*edit_world)(struct world *mzx_world,
 int reload_curr_file);
CORE_LIBSPEC extern void (*debug_counters)(struct world *mzx_world);
CORE_LIBSPEC extern void (*draw_debug_box)(struct world *mzx_world,
 int x, int y, int d_x, int d_y);

CORE_LIBSPEC extern int (*debug_robot)(struct world *mzx_world,
 struct robot *cur_robot, int id);
CORE_LIBSPEC extern void (*edit_breakpoints)(struct world *mzx_world);
#endif // CONFIG_EDITOR

#ifdef CONFIG_UPDATER
CORE_LIBSPEC extern void (*check_for_updates)(struct world *mzx_world,
 struct config_info *conf);
#endif

__M_END_DECLS

#endif // __GAME_H
