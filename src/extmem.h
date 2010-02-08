/* MegaZeux
 *
 * Copyright (C) 2007 Kevin Vance <kvance@kvance.com>
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
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

#ifndef __EXTMEM_H
#define __EXTMEM_H

#include "compat.h"

__M_BEGIN_DECLS

#include "board_struct.h"
#include "world_struct.h"

#ifdef CONFIG_NDS

// Move the board's memory from normal RAM to extra RAM.
void store_board_to_extram(Board *board);

// Move the board's memory from normal RAM to extra RAM.
void retrieve_board_from_extram(Board *board);

// Set the current board to cur_board, in regular memory.
void set_current_board(World *mzx_world, Board *cur_board);

// Set the current board to cur_board, in extra memory.
void set_current_board_ext(World *mzx_world, Board *cur_board);

#else // !CONFIG_NDS

/* If we're not on NDS, these inline functions should get optimised
 * away. The implementations are obvious enough.
 */

static inline void store_board_to_extram(Board *board)
{
  // do nothing
}

static inline void retrieve_board_from_extram(Board *board)
{
  // do nothing
}

static inline void set_current_board(World *mzx_world, Board *cur_board)
{
  mzx_world->current_board = cur_board;
}

static inline void set_current_board_ext(World *mzx_world, Board *cur_board)
{
  set_current_board(mzx_world, cur_board);
}

#endif // CONFIG_NDS

__M_END_DECLS

#endif // __BOARD_H
