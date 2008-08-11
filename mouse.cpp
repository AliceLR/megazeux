/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
 * Copyright (C) 1999 Charles Goetzman
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

// Mouse routines. Converted from code by Dave Kirsch. Very simple w/text
// cursor.

#include "beep.h"
#include <_null.h>
#include "mouse.h"
#include "data.h"

#pragma inline

char mousehidden=0;//Is the mouse hidden? (Additive)
unsigned int mouseinstalled=0;//Is the mouse installed?
char driver_activated=0;//Is our driver installed?

volatile int mousex,mousey,mybutton;//Character position of mouse
volatile int mmx, mmy;
volatile int mbufin=0,mbufout=0;//Mouse buffer pointers
char mousefreeze=0;//Is mouse frozen in place?
mouse_info_rec mbuf[MOUSE_BUFFERSIZE];//Mouse event buffer

unsigned int vseg;//Current segment of video ram.

unsigned char old_color;
unsigned char new_color;
char saved=0;
int oldmx,oldmy;

int mouse_count=5000;//Countdown to pseudo-hide from non-activity.

#define MK_FP( seg,ofs )( (void _seg * )( seg ) +( void near * )( ofs ))
#define pokeb(a,b,c)    (*((char far*)MK_FP((a),(b))) = (char)(c))
#define peekb(a,b)      (*((char far*)MK_FP((a),(b))))
#define POKEATTRIB(x,y,a)	pokeb(vseg,(((y)*80+(x))<<1)+1,a)
#define PEEKATTRIB(x,y)		peekb(vseg,(((y)*80+(x))<<1)+1)
#define POINTS		*((unsigned char far *)0x00000485)

//The new mouse handler.
static void far mousehandler(void) {
  int mmx, mmy;

	register int conditionmask;
	asm {
		push ds
		push ax
		mov ax,DGROUP
		mov ds,ax
		pop ax
		mov conditionmask,ax
		}

	mouse_count=5000;

	if(!mousefreeze) {
		//Save mouse info passed to us from driver
		asm {
			mov mousex, cx
			mov mousey, dx
			mov mybutton, bx //Gotta snag the mouse buttons now too. Spid
			}

		mousex>>=3;//Characters are 8 pixels wide
		if(POINTS==0) mousey=0;
		else mousey/=POINTS;//Scale mousey down

		//See if the mouse has moved.
		if(conditionmask&MOUSEMOVE) {
			if(saved) {
				POKEATTRIB(oldmx,oldmy,old_color);
				saved=0;
				}
			if(!mousehidden) {
				//Draw cursor
				old_color=PEEKATTRIB(mousex,mousey);
/*				_AX=old_color;
				asm rol al,3//Rotate it
				new_color=_AX;*/
				new_color=old_color^255;
				POKEATTRIB(mousex,mousey,new_color);//Write out new mouse cursor
				oldmx=mousex;
				oldmy=mousey;
				saved=1;
				}
			}
		}
	//Now, see if a mouse button was whacked
	if(conditionmask&~MOUSEMOVE) {
		if(((mbufin+1)%MOUSE_BUFFERSIZE)==mbufout) {//Buffer full?
			beep();
			}
		else {
			mbuf[mbufin].buttonstat=conditionmask&~MOUSEMOVE;
			mbuf[mbufin].cx=mousex;
			mbuf[mbufin].cy=mousey;
			mbufin=(mbufin+1)%MOUSE_BUFFERSIZE;
			}
		}
  asm pop ds
}

//Install the mouse routines.
void m_init(void) {
	if(driver_activated) return;//Already done!

	asm {
		sub ax,ax//Mouse driver function 0 -- reset and detect
		int 33h
		mov mouseinstalled,ax
		}
	if(mouseinstalled) {//If a mouse is installed then activate driver
		mousefreeze++;//Make sure handler doesn't do things, yet

		vseg=0xb800;

		//Set up max x and y ranges
		asm {
			mov dx,639	//Pixels horizontally
			mov ax,7		//mouse driver function 7 -- set max x range
			sub cx,cx	//Minimum range
			int 33h

			mov dx,349	//Pixels vertically
			mov ax,8		//mouse driver function 8 -- set max y range
			sub cx,cx	//Minimum range
			int 33h
			}

		//Now install user routine

		asm {
			mov ax,cs
			mov es,ax
			mov dx,offset mousehandler
			//Setup up bits for calling routine */
			mov cx,31
			mov ax,12//Function 12 -- set user routine
			int 33h
			}

		mousex=mousey=0;

		asm {
			mov cx,mousex
			mov dx,mousey
			mov ax,4//mouse driver function 4 -- set mouse position */
			int 33h
			}

		m_show();
		mousefreeze--;
		driver_activated=1;
		}
	//Done!
}

//Save current mouse state to saved_mouse_* globals
void m_snapshot(void) {
	saved_mouse_x = mousex;
	saved_mouse_y = mousey;
	saved_mouse_buttons = mybutton;
}

//Change the current video segment
void m_vidseg(unsigned int newseg) {
	if(!driver_activated) return;
	if(!mousehidden) {
		m_hide();
		vseg=newseg;
		m_show();
		}
	else vseg=newseg;
}

//Hide the mouse cursor
void m_hide(void) {
	if(!driver_activated) return;

	mousefreeze++;
	mousehidden++;

	if(saved) {
		POKEATTRIB(oldmx,oldmy,old_color);
		saved=0;
		}

	mousefreeze--;
}

//Show the mouse cursor
void m_show(void) {
	if(!driver_activated) return;

	mousefreeze++;

	if(mousehidden) mousehidden--;
	else {
		mousefreeze--;
		return;
		}

	if(mousehidden) {
		mousefreeze--;
		return;
		}

	//Draw mouse cursor
	old_color=PEEKATTRIB(mousex,mousey);
/*	_AX=old_color;
	asm rol al,3
	new_color=_AX;*/
	new_color=old_color^255;

	POKEATTRIB(mousex,mousey,new_color);

	oldmx=mousex;
	oldmy=mousey;
	saved=1;

	mousefreeze--;
}

//Move the mouse cursor
void m_move(int newx,int newy) {
	if(!driver_activated) return;

	mousefreeze++;
	//Convert x/y to pixels
	mousex=newx;
	mousey=newy;
	newx = (newx * 8) + 4;
	newy = (newy * 14) + 7;

	m_hide();
	asm {
		mov cx,newx
		mov dx,newy
		mov ax,4//mouse driver function 4 -- set mouse position */
		int 33h
		}
	m_show();
	mousefreeze--;
}

//Returns non-0 if there is something in the mouse buffer
char m_check(void) {
  return mbufin!=mbufout;
}

//Grab a copy of the top event
void m_preview(mouse_info_rec *mir) {
	if(!driver_activated) return;

	if(mbufin!=mbufout)
		*mir=mbuf[mbufout];
	else {
		//Nothing to pull, just report mouse position
		mir->cx=mousex;
		mir->cy=mousey;
		mir->buttonstat=0;
		}
}

//Grab the actual top event
void m_get(mouse_info_rec *mir) {
	if(!driver_activated) return;

	if(mbufin!=mbufout) {
		if(mir!=NULL) *mir=mbuf[mbufout];
		mbufout=(mbufout+1)%MOUSE_BUFFERSIZE;
		}
	else {
		//Nothing to pull, just report mouse position */
		mir->cx=mousex;
		mir->cy=mousey;
		mir->buttonstat=0;
		}
}

//Uninstall the mouse routines
void m_deinit(void) {
	if(!driver_activated) return;

	m_hide();

	asm {
		sub ax,ax
		int 33h
		}

	driver_activated=0;
}

//Get bits of button info
int m_buttonstatus(void) {
	int bits;

	if(!driver_activated) return 0;

	asm {
		mov ax,3
		int 33h
		mov bits,bx
		}

	return bits;
}