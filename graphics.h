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

/* GRAPHICS.H- Declarations for GRAPHICS.ASM */

#ifndef __GRAPHICS_H
#define __GRAPHICS_H

#ifdef __cplusplus
extern "C" {
#endif

//Note- NONE of these functions preserve the mouse cursor
void color_string(char far *string,int x_start,int y_start,
	unsigned char color,unsigned int segment);
void write_string(char far *string,int x_start,int y_start,
	unsigned char color,unsigned int segment,char taballowed=1);
void write_line(char far *string,int x_start,int y_start,
	unsigned char color,unsigned int segment,char taballowed=1);
void color_line(int length,int x_start,int y_start,unsigned char color,
	unsigned int segment);
void fill_line(int length,int x_start,int y_start,unsigned int char_col,
	unsigned int segment);
void draw_char(int chr,int color,int x_pos,int y_pos,unsigned int segment);
void draw_char_linear(int chr,int color,int pos,unsigned int segment);
void clear_screen(int with,unsigned int segment);
void page_flip(unsigned int page);
/* Page segments-
		0 is at segment 0xB800
		1 is at segment 0xB900
		2 is at segment 0xBA00
		3 is at segment 0xBB00 */

#ifdef __cplusplus
}
#endif

#endif