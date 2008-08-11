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

/* INTAKE.H- Declarations for INTAKE.CPP */

#ifndef __INTAKE_H
#define __INTAKE_H

#include <_null.h>

//Global insert status
extern char insert_on;

//See code for full docs, preserves mouse cursor, be prepared for a
//MOUSE_EVENT! (must acknowledge_event() it)
int intake(char far *string,int max_len,char x,char y,unsigned int segment,
 unsigned char color,char exit_type,int filter_type,char password=0,
 int *return_x_pos=NULL,char robo_intk=0,char far *macro=NULL);

#endif