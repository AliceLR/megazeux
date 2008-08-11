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

//Externs for SCRDISP.CPP

#ifndef __SCRDISP_H
#define __SCRDISP_H

extern int scroll_base_color;
extern int scroll_corner_color;
extern int scroll_pointer_color;
extern int scroll_title_color;
extern int scroll_arrow_color;

void scroll_edit(int id,char type=2);//Type 0/1 to DISPLAY a scroll/sign
void scroll_frame(int id,unsigned int pos);
void scroll_edging(char type);//type==0 scroll, 1 sign, 2 scroll edit
void help_display(unsigned char far *help,unsigned int offs,char far *file,
	char far *label);
void help_frame(unsigned char far *help,unsigned int pos);
char print(unsigned char far *str);

#endif