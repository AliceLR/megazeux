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

/* Prototypes and declarations for MOUSE.CPP */

#ifndef __MOUSE_H
#define __MOUSE_H

//Code returned by getkey for mouse events
#define MOUSE_EVENT -513

//Size of mouse "click" ahead buffer
#define MOUSE_BUFFERSIZE 16

//Bit defines for mouse driver function 12 -- define handler
//Also used for mouse queue status
#define MOUSEMOVE      1
#define LEFTBPRESS     2
#define LEFTBRELEASE   4
#define RIGHTBPRESS    8
#define RIGHTBRELEASE 16

//Bits for button presses from button status function
#define LEFTBDOWN  1
#define RIGHTBDOWN 2

//Mouse information record
struct mouse_info_rec {
  char buttonstat;
  char cx,cy;
};

typedef struct mouse_info_rec mouse_info_rec;

extern char mousehidden;
extern unsigned int mouseinstalled;
extern char driver_activated;
extern volatile int mousex,mousey;
extern int mouse_count;

//Initialize the mouse routines
void m_init(void);

//Deinitialize the mouse routines
void m_deinit(void);

//Change the current video segment
void m_vidseg(unsigned int newseg);

//Hide the mouse cursor
void m_hide(void);

//Show the mouse cursor
void m_show(void);

//Move the mouse cursor
void m_move(int newx,int newy);

//Return non-0 if there are events waiting in the buffer
char m_check(void);

//Look at the next event in the buffer, but don't pull it out
void m_preview(mouse_info_rec *mir);

//Get and remove next event from the buffer
void m_get(mouse_info_rec *mir);

//Return the current status of the mouse buttons
int m_buttonstatus(void);

#endif