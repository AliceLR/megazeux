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

// Include file for game2.cpp (not game2.asm anymore!)

#ifndef __GAME2_H
#define __GAME2_H

#include "compat.h"

__M_BEGIN_DECLS

#include "data.h"
#include "world_struct.h"

void hurt_player_id(struct world *mzx_world, enum thing id);
void update_board(struct world *mzx_world);
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
int flip_dir(int dir);

__M_END_DECLS

#endif // __GAME2_H
