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

//Prototype- ROBO_ED.CPP

#ifndef __ROBO_ED_H
#define __ROBO_ED_H

char replace_line(int id,unsigned int pos,unsigned int size,
	unsigned char far *buffer,unsigned char far *robot,char insert=0);
void robot_editor(int id);
long _rts_pre_param(unsigned char far *robot,unsigned char far *string);
void robot_to_string(unsigned char far *robot,unsigned char far *string);
inline char isspace(unsigned char c);
char string_to_robot(unsigned char far *string,unsigned char far *storage,
 int *ret_length);
long _rtd_pre_param(unsigned char far *robot,char x,char y,int color);
void robot_to_display(unsigned char far *robot,char y,int color);
int robo_ed_block_cmd(void);
unsigned int find_line_num(unsigned char far *robot,unsigned int pos);
char export_robot_dialog(void);
void robo_ed_options(void);

#endif