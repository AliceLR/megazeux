/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
 * Copyright (C) 1999 Charles Goetzman
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

/* Declarations for COUNTER.CPP */

#ifndef __COUNTER_H
#define __COUNTER_H

#include <stdio.h>

extern FILE *input_file;
extern FILE *output_file;
extern int player_restart_x;
extern int player_restart_y;
extern char was_zapped;
extern char file_in[12];
extern char file_out[12];

#ifdef __cplusplus
extern "C" {
#endif

//Get a counter. Include robot id if a robot is running.
int get_counter(char far *name,unsigned char id=0);
//Sets the value of a counter. Include robot id if a robot is running.
void set_counter(char far *name,int value=0,unsigned char id=0);
//Decrease or increase a counter.
void inc_counter(char far *name,int value=1,unsigned char id=0);
void dec_counter(char far *name,int value=1,unsigned char id=0);
//Take a key. Returns non-0 if none of proper color exist.
char take_key(char color);
//Give a key. Returns non-0 if no room.
char give_key(char color);
int get_rparam(char far *src, int id);
int translate_coordinates(char far *src, int &x, int &y);

#ifdef __cplusplus
}
#endif

#endif
