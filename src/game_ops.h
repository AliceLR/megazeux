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

#ifndef __GAME_OPS_H
#define __GAME_OPS_H

#include "compat.h"

__M_BEGIN_DECLS

#include "data.h"
#include "world_struct.h"

CORE_LIBSPEC void find_player(struct world *mzx_world);

void enable_intro_mesg(void);
void clear_intro_mesg(void);
void draw_intro_mesg(struct world *mzx_world);
void set_mesg(struct world *mzx_world, const char *str);
void set_mesg_direct(struct board *src_board, const char *str);

boolean player_can_save(struct world *mzx_world);
void player_switch_bomb_type(struct world *mzx_world);
void player_cheat_give_all(struct world *mzx_world);
void player_cheat_zap(struct world *mzx_world);

void calculate_xytop(struct world *mzx_world, int *x, int *y);
int move_player(struct world *mzx_world, int dir);
int grab_item(struct world *mzx_world, int offset, int dir);
void rotate(struct world *mzx_world, int x, int y, int dir);
void check_find_player(struct world *mzx_world);
int take_key(struct world *mzx_world, int color);
int give_key(struct world *mzx_world, int color);

void hurt_player_id(struct world *mzx_world, enum thing id);
int flip_dir(int dir);
int find_seek(struct world *mzx_world, int x, int y);

void shoot_lazer(struct world *mzx_world, int x, int y, int dir, int length,
 int color);
int transport(struct world *mzx_world, int x, int y, int dir, enum thing id,
 int param, int color, int can_push);
int push(struct world *mzx_world, int x, int y, int dir, int checking);
void shoot(struct world *mzx_world, int x, int y, int dir, int type);
void shoot_fire(struct world *mzx_world, int x, int y, int dir);
void shoot_seeker(struct world *mzx_world, int x, int y, int dir);
void shoot_missile(struct world *mzx_world, int x, int y, int dir);
enum move_status move(struct world *mzx_world, int x, int y, int dir,
 int flags);
enum dir parsedir(struct world *mzx_world, enum dir old_dir, int x, int y,
 enum dir flow_dir, int bln, int bls, int ble, int blw);

// Function to take an x/y position and return an array offset
// (within the board)

static inline int xy_to_offset(struct board *src_board, int x, int y)
{
  return (y * src_board->board_width) + x;
}

// Take an x/y position and a direction and return a new x/y position (in
// what ret_x/ret_y point to)
// Returns -1 if offscreen, 0 otherwise.

static inline int xy_shift_dir(struct board *src_board, int x, int y,
 int *ret_x, int *ret_y, int direction)
{
  switch(direction)
  {
    case 0: y--; break; // North
    case 1: y++; break; // South
    case 2: x++; break; // East
    case 3: x--; break; // West
  }
  if((x == -1) || (x == src_board->board_width) ||
   (y == -1) || (y == src_board->board_height))
    return -1;

  *ret_x = x;
  *ret_y = y;
  return 0;
}

__M_END_DECLS

#endif // __GAME_OPS_H
