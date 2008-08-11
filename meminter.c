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

//Interface to seamlessly replace FARMALLOC, FARFREE, and FARCORELEFT with
//appropriate DOS calls.

#include <dos.h>
#include <_null.h>
#include "meminter.h"

#include "error.h"
#include "data.h"

unsigned long cdecl farcoreleft(void) {
	unsigned tmp;
	int tmp2;
	tmp2=allocmem(30000,&tmp);
	if(tmp2!=-1) return((long)(tmp2))*16l;
	freemem(tmp);
	return(30000l*16l);
}

void        cdecl farfree(void far *__block) {
	freemem(FP_SEG(__block));
}

void far  * cdecl farmalloc(unsigned long __nbytes) {
	unsigned seg;
	unsigned size=(__nbytes>>4)+1;
	if(allocmem(size,&seg)!=-1) return NULL;
	return(MK_FP(seg,0));
}

//For use by File funcs

void   cdecl free(void *__block) {
	freemem(FP_SEG(__block));
}

void  *cdecl malloc(unsigned __nbytes) {
	unsigned seg;
	unsigned size=(__nbytes>>4)+1;
	if(allocmem(size,&seg)!=-1) return NULL;
	return(MK_FP(seg,0));
}

void  *cdecl realloc(void *__block,unsigned __size) {
	error2("Error accessing boards",2,24,current_pg_seg,0x0702);
}

unsigned long cdecl coreleft (void) {
	unsigned tmp;
	int tmp2;
	tmp2=allocmem(30000,&tmp);
	if(tmp2!=-1) return((long)(tmp2))*16l;
	freemem(tmp);
	return(30000l*16l);
}