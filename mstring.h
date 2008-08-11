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

extern char strings[16][64];
int string_type(char far *expression);
char *get_string(char far *expression, char far *str_buffer);
char get_str_char(char far *expression);
int get_str_int(char far *expression);
void set_string(char far *expression, char *set);
void set_str_char(char far *expression, char set);
void set_str_int(char far *expression, int val);
void load_string_board(char far *expression, int x, int y, int w, 
 int h, char l, char far *src);
