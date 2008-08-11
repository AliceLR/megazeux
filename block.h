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

/* Declarations */

#ifndef __BLOCK_H
#define __BLOCK_H

int block_cmd(void);
int rtoo_obj_type(void);
int choose_char_set(void);
char save_file_dialog(char far *title,char far *prompt,char far *dest);
//Have overlay_mode set to wanted mode with bits 64/128 set properly
void export_ansi(char far *file,int x1,int y1,int x2,int y2,char text_only);
int export_type(void);
int import_type(void);
//Pass curr_thing and curr_param so overwritten objects can be handled
//correctly
void import_ansi(char far *filename,char obj_type,int &curr_thing,
 int &curr_param,int start_x=0,int start_y=0);
int import_ansi_obj_type(void);

#endif
