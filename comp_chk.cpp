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

//COMP_CHK.CPP- The function computer_check, which verifies that the
//              computer and graphics card are high enough. Returns
//              non-0 if not. Make sure to compile anything that could
//              run before this function as 8086 code.

//8086 compatible-
#pragma option -1-
#pragma option -2-

#include "detect.h"
#include "dt_data.h"
#include "comp_chk.h"

//Global variable for VGA availability
char vga_avail=0;
//Card/processor
char card;

//Some strings for output

char far *sorry2="\r\n\r\nSorry, MegaZeux requires an EGA graphics card or better.\r\n$";

//Check graphics card and processor.
char computer_check(void) {
	card=detect_graphics();
	//Uses DOS services to print stuff
	if(card<EGA) {
		asm push ds
		asm mov dx,SEG sorry2
		asm mov ds,dx
		asm lds dx,ds:sorry2
		asm mov ah,9
		asm int 21h
		asm pop ds
		return -1;
		}
	if(card>=VGAm) vga_avail=1;//VGA/MCGA/SVGA card
	return 0;
}
