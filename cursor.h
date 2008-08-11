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

/* CURSOR.H- Declarations for CURSOR.ASM */

#ifndef __CURSOR_H
#define __CURSOR_H

#ifdef __cplusplus
extern "C" {
#endif

void cursor_underline(void);
void cursor_solid(void);
void cursor_off(void);
void move_cursor(int x_pos,int y_pos);

extern unsigned char cursor_mode;

#define CURSOR_INVIS 2
#define CURSOR_BLOCK 1
#define CURSOR_UNDERLINE 0

#ifdef __cplusplus
}
#endif

#endif