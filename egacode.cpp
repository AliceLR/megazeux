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

//Code to edit character sets and switch between ega 14 point mode and
//vga 16 point mode

#include "meminter.h"
#include "charset.h"
#include "string.h"
#include <stdio.h>
#include <dos.h>
#include "egacode.h"

//Current set- No reading of characters ever neccesary!
unsigned char far *curr_set;//Size- 14*256 bytes

//Segment of character set memory
#define CS_Seg  0xb800
//Offset of character set 0 (the only one used)
#define CS_Offs 0x0000
//Pointer to character set memory, as unsigned char far
#define CS_Ptr	((unsigned char far *)MK_FP(CS_Seg,CS_Offs))

//Enter 14-byte high character mode and reset character sets
//(EGA native text mode)
void ega_14p_mode(void) {
	asm {
		mov ax,1201h
		mov bl,30h
		int 10h
		mov ax,0003h
		int 10h
		}
}

//Enter 16-byte high character mode and reset character sets
//(VGA native text mode)
void vga_16p_mode(void) {
	asm {
		mov ax,1202h
		mov bl,30h
		int 10h
		mov ax,0003h
		int 10h
		}
}

//Access the character set part of the graphics memory (internal function)
void _access_char_sets(void) {
	asm {
	mov al,5
	mov dx,3ceh
	out dx,al         // outportb(0x3ce,5)
	mov al,0
	mov dx,3cfh
	out dx,al			// outportb(0x3cf,0)
	mov al,6
	mov dx,3ceh
	out dx,al			// outportb(0x3ce,6)
	mov al,0ch
	mov dx,3cfh
	out dx,al			// outportb(0x3cf,0xc)
	mov al,4
	mov dx,3c4h
	out dx,al			// outportb(0x3c4,4)
	mov al,6
	mov dx,3c5h
	out dx,al			// outportb(0x3c5,6)
	mov al,2
	mov dx,3c4h
	out dx,al			// outportb(0x3c4,2)
	mov al,4
	mov dx,3c5h
	out dx,al			// outportb(0x3c5,4)
	mov dx,3ceh
	out dx,al			// outportb(0x3ce,4)
	mov al,2
	mov dx,3cfh
	out dx,al			// outportb(0x3cf,2)
	}
}

//Access the regular text memory (internal function)
void _access_text(void) {
	asm {
	mov al,5
	mov dx,3ceh
	out dx,al			// outportb(0x3ce,5)
	mov al,10h
	mov dx,3cfh
	out dx,al			// outportb(0x3cf,0x10)
	mov al,6
	mov dx,3ceh
	out dx,al			// outportb(0x3ce,6)
	mov al,0eh
	mov dx,3cfh
	out dx,al			// outportb(0x3cf,0xe)
	mov al,4
	mov dx,3c4h
	out dx,al			// outportb(0x3c4,4)
	mov al,2
	mov dx,3c5h
	out dx,al			// outportb(0x3c5,2)
	mov dx,3c4h
	out dx,al			// outportb(0x3c4,2)
	mov al,3
	mov dx,3c5h
	out dx,al			// outportb(0x3c5,3)
	mov al,4
	mov dx,3ceh
	out dx,al			// outportb(0x3ce,4)
	mov al,0
	mov dx,3cfh
	out dx,al			// outportb(0x3cf,0)
	}
}

//Copies the set in memory (curr_set) to the real set in graphics memory
void ec_update_set(void) {
	int t1,t2;
	unsigned char far *charac=CS_Ptr;
	_access_char_sets();
	for(t1=0;t1<256;t1++)
		for(t2=0;t2<14;t2++)
			charac[t1*32+t2]=curr_set[t1*14+t2];
	_access_text();
}

//Changes one byte of one character and updates it
void ec_change_byte(int chr,int byte,int new_value) {
	unsigned char far *charac=CS_Ptr;
	curr_set[chr*14+byte]=new_value;
	_access_char_sets();
	charac[chr*32+byte]=new_value;
	_access_text();
}

//Changes one entire 14-byte character and updates it
void ec_change_char(int chr,unsigned char far *matrix) {
	int t1;
	unsigned char far *charac=CS_Ptr;
	_access_char_sets();
	for(t1=0;t1<14;t1++) {
		curr_set[chr*14+t1]=matrix[t1];
		charac[chr*32+t1]=matrix[t1];
		}
	_access_text();
}

//Changes one byte of one character WITHOUT updating it
void ec_change_byte_nou(int chr,int byte,int new_value) {
	curr_set[chr*14+byte]=new_value;
}

//Changes one entire 14-byte character WITHOUT updating it
void ec_change_char_nou(int chr,unsigned char far *matrix) {
	int t1;
	for(t1=0;t1<14;t1++) {
		curr_set[chr*14+t1]=matrix[t1];
		}
}

//Reads one byte of a character
int ec_read_byte(int chr,int byte) {
	return curr_set[chr*14+byte];
}

//Reads an entire 14-byte character
void ec_read_char(int chr,unsigned char far *matrix) {
	int t1;
	for(t1=0;t1<14;t1++) matrix[t1]=curr_set[chr*14+t1];
}

//Initialization function. Call AFTER setting mode with ega_14p.
//Copies current set to curr_set and insures set 0 is activated.
//Also copies current set to ascii_set. Also runs memory init, returns
//non-0 for error.
char ec_init(void) {
	curr_set=(unsigned char far *)farmalloc(3584);
	if(curr_set==NULL) return -1;
	//Copy default mzx to current and update
	mem_cpy((signed char far *)curr_set,
		(signed char far *)default_mzx_char_set,3584);
	ec_update_set();
	//Done!
	return 0;
}

//Call upon exit to free memory
void ec_exit(void) {
	if(curr_set!=NULL) farfree(curr_set);
}

//Save a character set to disk
char ec_save_set(char far *filename) {
	FILE *fp;
	fp=fopen(filename,"wb");
	if(fp==NULL) return -1;
	fwrite(curr_set,14,256,fp);
	fclose(fp);
	return 0;
}

//Load a character set from disk
char ec_load_set(char far *filename) {
	FILE *fp;
	fp=fopen(filename,"rb");
	if(fp==NULL) return -1;
	fread(curr_set,14,256,fp);
	ec_update_set();
	fclose(fp);
	return 0;
}

//Loads a character set directly from memory (stil 3584 bytes)
void ec_mem_load_set(unsigned char far *chars) {
	mem_cpy((signed char far *)curr_set,(signed char far *)chars,3584);
	ec_update_set();
}

//Loads in the default set (ASCII)
void ec_load_ascii(void) {
	ec_mem_load_set(ascii_set);
}

//Loads in the default set (Megazeux)
void ec_load_mzx(void) {
	ec_mem_load_set(default_mzx_char_set);
}