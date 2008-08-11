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

/* STRING.H- Declarations for STRING.ASM */

#ifndef __STRING_H
#define __STRING_H

#ifdef __cplusplus
extern "C" {
#endif

int str_len_color(char far *str);
int str_len(char far *str);
void str_cpy(char far *dest,char far *source);
void mem_cpy(char far *dest,char far *source,unsigned int len);
void str_cat(char far *dest,char far *source);
//Dest and source can overlap. Set code_bit to 0 if dest is lower in mem,
//or to 1 if dest is higher in mem.
void mem_mov(char far *dest,char far *source,unsigned int len,
	char code_bit);
//XOR a section of memory with a given code
void mem_xor(char far *mem,unsigned int len,unsigned char xor_with);
char _fstricmp(char far *dest,char far *source);//0==equal
void _fstrlwr(char far *str);
void _fstrupr(char far *str);

#define str_cmp _fstricmp
#define str_lwr _fstrlwr
#define str_upr _fstrupr

#ifdef __cplusplus
}
#endif

#endif