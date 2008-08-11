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

// EMS.CPP- Simple functions to initialize EMS and allocate and deallocate
//          EMS memory.

#include "ems.h"
#include <io.h>
#include <dos.h>
#include <fcntl.h>
#include <_null.h>

int avail_pages_EMS=0;//EMS pages available to our application
char far *page_frame_EMS=NULL;//Location in memory of our page frame
char setup=0;

//Returns -1 for error (no EMS software/hardware/etc)
//Otherwise, sets up total_pages_EMS, page_frame_EMS, and
//avail_pages_EMS.
char setup_EMS() {
	int fh;
	union REGS rg;

	if((fh=open("EMMXXXX0",O_RDONLY,&fh))==-1) return -1;

	rg.h.ah=0x44;
	rg.h.al=0x00;
	rg.x.bx=fh;
	int86(0x21,&rg,&rg);
	close(fh);
	if(rg.x.cflag) return -1;
	if(!(rg.x.dx & 0x80)) return -1;

	rg.h.ah=0x40;
	int86(0x67,&rg,&rg);
	if(rg.h.ah!=0) return -1;

	//EMS available! Get # pages
	rg.h.ah=0x42;
	int86(0x67,&rg,&rg);
	if(rg.x.cflag) return -1;
	avail_pages_EMS=rg.x.bx;

	//Get page frame
	rg.h.ah=0x41;
	int86(0x67,&rg,&rg);
	if(rg.h.ah!=0) return -1;
	page_frame_EMS=(char far *)MK_FP(rg.x.bx,0);

	setup=1;

	return 0;
}

//Returns the number of free pages of EMS available. -1 for error.
int free_mem_EMS() {
	union REGS rg;

	if(!setup) return 0;

	rg.h.ah=0x42;
	int86(0x67,&rg,&rg);
	if(rg.x.cflag) return -1;
	return rg.x.bx;
}

//Allocates n pages of EMS. Returns a handle or 0 for out of memory
//or EMS not installed. (So this can be used even if EMS is not found)
unsigned int alloc_EMS(int n) {
	union REGS rg;

	if(!setup) return 0;
	if(n>avail_pages_EMS) return 0;

	rg.h.ah=0x43;
	rg.x.bx=n;
	int86(0x67,&rg,&rg);
	if(rg.h.ah) return 0;
	return rg.x.dx;
}

//Frees a previously allocated EMS handle. -1 for error.
void free_EMS(unsigned int h) {
	union REGS rg;
	int i;

	if(!setup) return;
	if(h==0) return;//It's OK to deallocate a "NULL" handle!

	for(i=0;i<5;i++) {
		rg.h.ah=0x45;
		rg.x.dx=h;
		int86(0x67,&rg,&rg);
		if(rg.h.ah==0) break;
		}
}

//Maps logical page #log_page of EMS handle #h to
//physical page #phys_page. phys_page must range from 0 to 3. Used to not
//map if already mapped, to save time, but this had to be changed since the
//music code now uses EMS and we can't be sure of the current mapped state.
void map_page_EMS(unsigned int h,int phys_page,int log_page) {
	union REGS rg;

	if(!setup) return;

	rg.h.ah=0x44;
	rg.h.al=phys_page;
	rg.x.bx=log_page;
	rg.x.dx=h;
	int86(0x67,&rg,&rg);
}

//Saves EMS map state- h is a handle to save under (must be alloc'd)
void save_map_state_EMS(unsigned int h) {
	union REGS rg;

	if(!setup) return;

	rg.h.ah=0x47;
	rg.x.dx=h;
	int86(0x67,&rg,&rg);
}

//Restores EMS map state- h is a handle to save under (must be alloc'd)
void restore_map_state_EMS(unsigned int h) {
	union REGS rg;

	if(!setup) return;

	rg.h.ah=0x48;
	rg.x.dx=h;
	int86(0x67,&rg,&rg);
}