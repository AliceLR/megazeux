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

// getkey function to input from the keyboard.

#include "helpsys.h"
#include <conio.h>
#include "getkey.h"
#include "mouse.h"
#include <_null.h>

mouse_info_rec mouse_event={ 0,0,0 };
char auto_pop_mouse_events=1;
char allow_help=1;

//Local, for keywaiting function
mouse_info_rec temp_mev={ 0,0,0 };

char keywaiting(void) {
	if(kbhit()) return 1;//Key waiting
mouse_check_loop:
	if(m_check()) {//Check for a REAL mouse event
		m_preview(&temp_mev);
		if(temp_mev.buttonstat&(LEFTBPRESS|RIGHTBPRESS)) return 1;
		acknowledge_mouse();
		}
	return 0;
}

int getkey(void) {
	int t1;
	do { } while((!kbhit())&&(!m_check()));
	if((m_check())&&(!kbhit())) {
		m_preview(&mouse_event);
		//Forget release events
		if(!(mouse_event.buttonstat&(LEFTBPRESS|RIGHTBPRESS))) {
			//No new button presses
			acknowledge_mouse();
			return 0;
			}
		//Turn right button into ESC
		if(mouse_event.buttonstat&RIGHTBPRESS) {
			acknowledge_mouse();
			return 27;
			}
		if(auto_pop_mouse_events) acknowledge_mouse();
		return MOUSE_EVENT;
		}
	t1=getch();
	if(t1==0) {
		t1=-getch();
		//Help?
		if((allow_help)&&(t1==-59)) {
			help_system();
			return 0;
			}
		}
	return t1;
}

void acknowledge_mouse(void) {
	m_get(NULL);
}