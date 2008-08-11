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

/* Declarations for ROBALLOC.CPP */

#ifndef __ROBALLOC_H
#define __ROBALLOC_H

extern char robot_mem_type;//W_EMS or W_MEMORY for where robots are
extern int robot_ems;//EMS handle for W_EMS
extern unsigned char far *robot_mem;//Location in mem of global robot memory
extern unsigned int robot_free_mem;//Amount of free mem in bytes
extern char curr_rmem_status;//1 if global robot mem current

//Call before accessing robot memory. May change EMS paging.
void prepare_robot_mem(char global=0);

//Init and exit
int init_robot_mem(void);
void exit_robot_mem(void);

//Reallocate memory. Codes for type are below.
char reallocate_robot_mem(char type,unsigned char id,unsigned int size);
#define T_SCROLL	0
#define T_ROBOT	1

//Clear a robot entirely (struct and program)
void clear_robot(unsigned char id);
//Clears a scroll's struct and program.
void clear_scroll(unsigned char id);
//Clears a sensor's struct
void clear_sensor(unsigned char id);

//Rounds a number up to a multiple of 16. Intended for internal use but
//available for external use in a pinch. Returns minimum 16.
unsigned int _round16(unsigned int n);

//Find, and mark if asked, the first available object of type noted.
//Returns id or 0 if none available.
unsigned char find_robot(char mark=1);
unsigned char find_scroll(char mark=1);
unsigned char find_sensor(char mark=1);

//Copies one thing to another. Returns non-0 for not enough memory. Doesn't
//copy .used variable.
char copy_robot(unsigned char dest,unsigned char source);
char copy_scroll(unsigned char dest,unsigned char source);
//Cannot run out of memory-
void copy_sensor(unsigned char dest,unsigned char source);

#endif