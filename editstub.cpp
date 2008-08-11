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

#include "helpsys.h"
#include "runrobot.h"
#include "scrdisp.h"
#include "scrdump.h"
#include "sfx.h"
#include "sfx_edit.h"
#include "counter.h"
#include "game.h"
#include "fill.h"
#include "meter.h"
//#include "password.h"
#include "egacode.h"
#include "block.h"
#include "pal_ed.h"
#include "char_ed.h"
#include "edit_di.h"
#include "beep.h"
#include "boardmem.h"
#include "intake.h"
#include "mod.h"
#include "param.h"
#include "error.h"
#include "idarray.h"
#include "ems.h"
#include "meminter.h"
#include "cursor.h"
#include "retrace.h"
#include "string.h"
#include "edit.h"
#include "ezboard.h"
#include "data.h"
#include "const.h"
#include "graphics.h"
#include "window.h"
#include "getkey.h"
#include "palette.h"
#include "idput.h"
#include "hexchar.h"
#include <dos.h>
#include "roballoc.h"
#include "saveload.h"
#include <stdio.h>
#include "blink.h"
#include "cursor.h"
#include "counter.h"

// What little that is required by the editor is here. - Exo

char debug_mode=0, debug_x=0; //If debug mode is on, and X position of box.

void add_ext(char far *str,char far *ext) {
  int t1,t2=str_len(str);
	//check for existing ext.
	for(t1=0;t1<t2;t1++)
		if(str[t1]=='.') break;
	if(t1<t2) return;
	str[8]=0;//Limit main filename section to 8 chars
	str_cat(str,ext);
}

void draw_debug_box(char ypos) {
	int t1;
	draw_window_box(debug_x,ypos,debug_x+14,24,current_pg_seg,
		EC_DEBUG_BOX,EC_DEBUG_BOX_DARK,EC_DEBUG_BOX_CORNER,0);
	write_string("X/Y:     /\nBoard:\nMem:        k\nEMS:        k\nRobot memory-\n  . /61k    %",
		debug_x+1,ypos+1,EC_DEBUG_LABEL,current_pg_seg);
	write_number(curr_board,EC_DEBUG_NUMBER,debug_x+13,ypos+2,
		current_pg_seg,0,1);
	write_number((int)(farcoreleft()>>10),EC_DEBUG_NUMBER,
		debug_x+12,ypos+3,current_pg_seg,0,1);
	write_number(free_mem_EMS()<<4,EC_DEBUG_NUMBER,debug_x+12,ypos+4,
		current_pg_seg,0,1);
	t1=robot_free_mem/102;
	write_number(t1/10,EC_DEBUG_NUMBER,debug_x+2,ypos+6,current_pg_seg,
		0,1);
	write_number(t1%10,EC_DEBUG_NUMBER,debug_x+4,ypos+6,current_pg_seg,1);
  if(*mod_playing != 0)
  {
	  write_string(mod_playing,debug_x+2,ypos+7,EC_DEBUG_NUMBER,current_pg_seg);
  }
  else
  {
    write_string("(no module)",debug_x+2,ypos+7, EC_DEBUG_NUMBER, current_pg_seg);
  }
	t1=(unsigned int)(robot_free_mem*100L/62464L);
	write_number(t1,EC_DEBUG_NUMBER,debug_x+12,ypos+6,current_pg_seg,0,1);
}

