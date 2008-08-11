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

/* Declarations for BOARDMEM.CPP */

#ifndef __BOARDMEM_H
#define __BOARDMEM_H

#include <stdio.h>

char allocate_board_space(long size,unsigned char id,char conv_mem_ok=0);
void deallocate_board_space(unsigned char id);
unsigned int RLE2_size(unsigned char far *plane);
unsigned int RLE2_store(unsigned char far *where,unsigned char far *plane);
void RLE2_save(FILE *fp,unsigned char far *plane);
unsigned int RLE2_read(unsigned char far *where,unsigned char far *plane,
 char change_xysizes=1);
void RLE2_load(FILE *fp,unsigned char far *plane,char change_xysizes=1);
long size_of_current_board(void);
char store_current(unsigned char id);
char grab_current(unsigned char id);
//loading=0 to save to fp, non-0 to load from fp
char disk_board(unsigned char id,FILE *fp,char loading,
 unsigned char xor_with);
//Frees up memory. Also call end_sam if needed.
void free_up_board_memory(void);
//Converts max_bsiz_mode into max_bxsiz and max_bysiz
void convert_max_bsiz_mode(void);

#endif