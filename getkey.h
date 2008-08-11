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

/* GETKEY.H- Declarations for GETKEY.CPP */

#ifndef __GETKEY_H
#define __GETKEY_H

#include "mouse.h"

//Note- Be prepared to deal with MOUSE_EVENT!
char keywaiting(void);
int getkey(void);
//Acknowledges you have dealt with the mouse event
void acknowledge_mouse(void);

//The event that caused the MOUSE_EVENT, call acknowledge_mouse
//to actually pop it off of the queue (doesn't clear mouse_event)
extern mouse_info_rec mouse_event;

//If set to 1 (default) mouse events are automatically popped off of
//the queue as soon as getkey returns a MOUSE_EVENT.
extern char auto_pop_mouse_events;

//If set to 0, help via F1 will be ignored
extern char allow_help;

#endif