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

/* Declarations for IDARRAY.ASM */

#ifndef __IDARRAY_H
#define __IDARRAY_H

#ifdef __cplusplus
extern "C" {
#endif

void id_place(int x,int y,unsigned char id,unsigned char color,
	unsigned char param);
//"blank" is a junk byte
void offs_place_id(unsigned int offs,int blank,unsigned char id,
	unsigned char color,unsigned char param);
void id_clear(int x,int y);
void id_remove_top(int x,int y);
//"blank" is a junk byte
void offs_remove_id(unsigned int offs,int blank);
void id_remove_under(int x,int y);

#ifdef __cplusplus
}
#endif

#endif