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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Declarations for PARAM.CPP */

#ifndef __PARAM_H
#define __PARAM_H

extern int def_params[128];
int edit_param(World *mzx_world, int id, int param);

// (internal) "p"arameter "e"dit functions
int pe_chest(World *mzx_world, int param);
int pe_health(World *mzx_world, int param);
int pe_ring(World *mzx_world, int param);
int pe_bomb(World *mzx_world, int param);
int pe_lit_bomb(World *mzx_world, int param);
int pe_explosion(World *mzx_world, int param);
int pe_door(World *mzx_world, int param);
int pe_gate(World *mzx_world, int param);
int pe_transport(World *mzx_world, int param);
int pe_pouch(World *mzx_world, int param);
int pe_pusher(World *mzx_world, int param);
int pe_lazer_gun(World *mzx_world, int param);
int pe_bullet(World *mzx_world, int param);
int pe_ricochet_panel(World *mzx_world, int param);
int pe_mine(World *mzx_world, int param);
int pe_snake(World *mzx_world, int param);
int pe_eye(World *mzx_world, int param);
int pe_thief(World *mzx_world, int param);
int pe_slime_blob(World *mzx_world, int param);
int pe_runner(World *mzx_world, int param);
int pe_ghost(World *mzx_world, int param);
int pe_dragon(World *mzx_world, int param);
int pe_fish(World *mzx_world, int param);
int pe_shark(World *mzx_world, int param);
int pe_spider(World *mzx_world, int param);
int pe_goblin(World *mzx_world, int param);
int pe_bullet_gun(World *mzx_world, int param);
int pe_bear(World *mzx_world, int param);
int pe_bear_cub(World *mzx_world, int param);
int pe_missile_gun(World *mzx_world, int param);
int edit_sensor(World *mzx_world, Sensor *cur_sensor);
int edit_scroll(World *mzx_world, Scroll *cur_scroll);
int edit_robot(World *mzx_world, Robot *cur_robot);

#endif
