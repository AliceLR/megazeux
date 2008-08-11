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

// Include file for GAME2.ASM

#ifndef __GAME2_H
#define __GAME2_H

#ifdef __cplusplus
extern "C" {
#endif

void update_screen(void);
void _shoot_lazer_c(int x,int y,int dir,int len,int color);
void _shoot_c(int x,int y,int dir,int type);
void _shoot_fire_c(int x,int y,int dir);
void _shoot_seeker_c(int x,int y,int dir);
void _shoot_missile_c(int x,int y,int dir);
char _move(int x,int y,int dir,unsigned int flags);
char _push(int x,int y,int dir,char checking);
char _transport(int x,int y,int dir,int id,int param,int color,char can_push);
int parsedir(int dir,int x,int y,int flow,int bln,int bls,int ble,
	int blw);

#ifdef __cplusplus
}
#endif

#endif