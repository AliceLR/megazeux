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

// game.c
CORE_LIBSPEC void title_screen(struct world *mzx_world);
CORE_LIBSPEC void load_board_module(struct world *mzx_world);
boolean load_game_module(struct world *mzx_world, char *filename,
 boolean fail_if_same);

// game_menu.c
void game_menu(struct world *mzx_world);
void main_menu(struct world *mzx_world);

// game_update.c
int update(struct world *mzx_world, boolean is_title, boolean *fadein);
extern bool pal_update;

// game_update_board.c
void update_board(struct world *mzx_world);

#ifdef CONFIG_EDITOR
CORE_LIBSPEC void play_game(struct world *mzx_world, boolean *fadein);
CORE_LIBSPEC void draw_viewport(struct world *src_board);
#endif

__M_END_DECLS

#endif // __GAME_H
