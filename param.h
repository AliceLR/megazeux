/* $Id$
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

/* Declarations for PARAM.CPP */

#ifndef __PARAM_H
#define __PARAM_H

extern int def_params[128];
int edit_param(unsigned char id,int param);

//(internal) "p"arameter "e"dit functions
int pe_chest(int param);
int pe_health(int param);
int pe_ring(int param);
int pe_bomb(int param);
int pe_lit_bomb(int param);
int pe_explosion(int param);
int pe_door(int param);
int pe_gate(int param);
int pe_transport(int param);
int pe_pouch(int param);
int pe_pusher(int param);
int pe_lazer_gun(int param);
int pe_bullet(int param);
int pe_ricochet_panel(int param);
int pe_mine(int param);
int pe_snake(int param);
int pe_eye(int param);
int pe_thief(int param);
int pe_slime_blob(int param);
int pe_runner(int param);
int pe_ghost(int param);
int pe_dragon(int param);
int pe_fish(int param);
int pe_shark(int param);
int pe_spider(int param);
int pe_goblin(int param);
int pe_bullet_gun(int param);
int pe_bear(int param);
int pe_bear_cub(int param);
int pe_missile_gun(int param);
int pe_sensor(int param);

#endif