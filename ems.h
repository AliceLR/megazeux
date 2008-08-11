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

/* EMS.H- Declarations for EMS.CPP */

#ifndef __EMS_H
#define __EMS_H

// Note- Pages are 16K, or 16384 bytes, in size

extern int avail_pages_EMS;
extern char far *page_frame_EMS;

char setup_EMS();
int free_mem_EMS();
unsigned int alloc_EMS(int n);
void free_EMS(unsigned int h);
void map_page_EMS(unsigned int h,int phys_page,int log_page);
void save_map_state_EMS(unsigned int h);
void restore_map_state_EMS(unsigned int h);

#endif