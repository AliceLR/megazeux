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

//Prototypes for EGACODE.CPP

#ifndef __EGACODE_H
#define __EGACODE_H

//Mode selection (also resets char sets)
void ega_14p_mode(void);
void vga_16p_mode(void);

//Initialization- run AFTER ega_14p_mode, error=out of memory
char ec_init(void);

//Exit- run when all done, simply deallocates memory
void ec_exit(void);

//Auto update functions
void ec_change_byte(int chr,int byte,int new_value);
void ec_change_char(int chr,unsigned char far *matrix);
int ec_read_byte(int chr,int byte);
void ec_read_char(int chr,unsigned char far *matrix);
void ec_mem_load_set(unsigned char far *chars);
void ec_load_ascii(void);
void ec_load_mzx(void);
char ec_save_set(char far *filename);
char ec_load_set(char far *filename);

//Functions that DON'T auto update
void ec_change_byte_nou(int chr,int byte,int new_value);
void ec_change_char_nou(int chr,unsigned char far *matrix);
void ec_update_set(void);

//The current set (14*256 bytes)
extern unsigned char far *curr_set;

#endif