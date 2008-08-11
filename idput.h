/* $Id: idput.h,v 1.2 1999/01/17 20:35:41 mental Exp $
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

/* Declarations for IDPUT.ASM */

#ifndef __IDPUT_H
#define __IDPUT_H

#ifdef __cplusplus
extern "C" {
#endif

void id_put(unsigned char x_pos,unsigned char y_pos,
	int array_x,int array_y,int ovr_x,int ovr_y,int vid_seg);
void draw_edit_window(int array_x,int array_y,int vid_seg);
void draw_game_window(int array_x,int array_y,int vid_seg);

void _id_put(unsigned char x_pos,unsigned char y_pos,
	int array_x,int array_y,int ovr_x,int ovr_y,void * vid_seg);
void _draw_edit_window(int array_x,int array_y,void * vid_seg);
void _draw_game_window(int array_x,int array_y,void * vid_seg);


#ifdef __cplusplus
}
#endif

#endif
