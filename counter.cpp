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

// Code for setting and retrieving counters and keys.

#include "sfx.h"
#include "runrobot.h"
#include "idarray.h"
#include "data.h"
// #include <math.h> // Meant for a hypot func to find actual distance, doesnt' work. Spid
#include "const.h"
#include "string.h"
#include "struct.h"
#include <stdlib.h>
#include "counter.h"
#include "mouse.h"
#include "game.h"
#include "game2.h"

int player_restart_x=0,player_restart_y=0,gridxsize=1,gridysize=1;
int myscrolledx=0,myscrolledy=0;
char was_zapped=0;
extern char mid_prefix;
void prefix_xy(int&fx,int&fy,int&mx,int&my,int&lx,int&ly,int robotx,
	int roboty);

//Get a counter. Include robot id if a robot is running.
int get_counter(char far *name,unsigned char id) {
	int t1;
	if(!str_cmp(name,"LOOPCOUNT")) return robots[id].loop_count;
      if(!str_cmp(name,"LOCAL")) return robots[id].blank; //Found this, no idea if it works Spid
	//Now search counter structs
	for(t1=0;t1<NUM_COUNTERS;t1++)
		if(!str_cmp(name,counters[t1].counter_name)) break;
	if(t1<NUM_COUNTERS) return counters[t1].counter_value;
	//First, check for robot specific counters
	if(!str_cmp(name,"BULLETTYPE")) return robots[id].bullet_type;
	if(!str_cmp(name,"PLAYERDIST"))
		return abs(robots[id].xpos-player_x)+abs(robots[id].ypos-player_y);
    //	if(!str_cmp(name,"PLADIST"))
	       //	return 34;
      //		return sqrtl((abs(robots[id].xpos-player_x) * abs(robots[id].xpos-player_x)) +
       //		(abs(robots[id].ypos-player_y) * abs(robots[id].ypos-player_y)));
	if(!str_cmp(name,"HORIZPLD"))
		return abs(robots[id].xpos-player_x);
	if(!str_cmp(name,"VERTPLD"))
		return abs(robots[id].ypos-player_y);
	if(!str_cmp(name,"THISX")) {
		if(mid_prefix<2) return robots[id].xpos;
		else if(mid_prefix==2) return robots[id].xpos-player_x;
		else return robots[id].xpos-get_counter("XPOS");
		}
	if(!str_cmp(name,"THISY")) {
		if(mid_prefix<2) return robots[id].ypos;
		else if(mid_prefix==2) return robots[id].ypos-player_y;
		else return robots[id].ypos-get_counter("YPOS");
		}
	//Next, check for global, non-standard counters
	if(!str_cmp(name,"INPUT")) return num_input;
	if(!str_cmp(name,"INPUTSIZE")) return input_size;
	if(!str_cmp(name,"KEY")) return last_key;
	if(!str_cmp(name,"SCORE")) return (unsigned int)score;
	if(!str_cmp(name,"TIMERESET")) return time_limit;
	if(!str_cmp(name,"PLAYERFACEDIR")) return player_last_dir>>4;
	if(!str_cmp(name,"PLAYERLASTDIR")) return player_last_dir&15;
      // Mouse info Spid
	if(!str_cmp(name,"MOUSEX")) return saved_mouse_x;
	if(!str_cmp(name,"MOUSEY")) return saved_mouse_y;
	if(!str_cmp(name,"BUTTONS")) return saved_mouse_buttons;
      // scroll_x and scroll_y never work, always return 0. Spid
	if(!str_cmp(name,"SCROLLEDX")) {
        calculate_xytop(myscrolledx,myscrolledy);
        return myscrolledx;
        }	
     if(!str_cmp(name,"SCROLLEDY")) {
        calculate_xytop(myscrolledx,myscrolledy);
        return myscrolledy;
        }
      // Player position Spid
      if(!str_cmp(name,"PLAYERX")) return player_x;
      if(!str_cmp(name,"PLAYERY")) return player_y;
      // This WILL be position over the game board, once scroll_ works. Spid
      if(!str_cmp(name,"MBOARDX")) {
        calculate_xytop(myscrolledx,myscrolledy);
        return (mousex + myscrolledx - viewport_x);
        }
      if(!str_cmp(name,"MBOARDY")) {
        calculate_xytop(myscrolledx,myscrolledy);
        return (mousey + myscrolledy - viewport_y);
        }
      // viewport_x viewport_y
	return 0;//Not found
}


//Sets the value of a counter. Include robot id if a robot is running.
void set_counter(char far *name,int value,unsigned char id) {
	int t1;
	//First, check for robot specific counters
	if(!str_cmp(name,"LOOPCOUNT")) {
		robots[id].loop_count=value;
		return;
		}
      if(!str_cmp(name,"LOCAL")) {
            robots[id].blank=value;
            return;
            }
	if(!str_cmp(name,"BULLETTYPE")) {
		if(value>2) value=2;
		robots[id].bullet_type=value;
		return;
		}
	//Next, check for global, non-standard counters

      // SHOULD allow instant scrolling of screen, once scroll_x can be referenced
      // As of yet, doesn't work. Spid
      if(!str_cmp(name,"SCROLLEDX")) {
	    scroll_x= value;
	    return;
	    }
      if(!str_cmp(name,"SCROLLEDY")) {
	    scroll_y= value;
	    return;
	    }
/*	 // These Grids will be used for mouse buttons, once that is in. Spid
	if(!str_cmp(name,"GRIDXSIZE")) {
		gridxsize=value;
		return;
		}
	if(!str_cmp(name,"GRIDYSIZE")) {
		gridysize=value;
		return;
		}
*/      // Shows or hides the hardware cursor Spid
	if(!str_cmp(name,"CURSORSTATE")) {
		if (value==1)  m_show();
		if (value==0)  m_hide();
		}   //Don't return! We need to set the counter, as well.
		// CURSORSTATE is referenced in GAME.CPP
		// This should PROBABLY be a variable, instead of a counter,
		// but I DID have a reason for using a counter, if I ever
		// remember it. Spid
      if(!str_cmp(name,"MOUSEX")) {
            if(value>79) value=79;
            if(value<0) value=0;
	    saved_mouse_y = value;
            m_move(value,mousey);
            return;
      }
      if(!str_cmp(name,"MOUSEY")) {
            if(value>24) value=24;
            if(value<0) value=0;
	    saved_mouse_x = value;
            m_move(mousex,value);
            return;
      }
	if(!str_cmp(name,"INPUT")) {
		num_input=value;
		return;
		}
	if(!str_cmp(name,"INPUTSIZE")) {
		if(value>255) value=255;
		input_size=(unsigned char)value;
		return;
		}
	if(!str_cmp(name,"KEY")) {
		if(value>255) value=255;
		last_key=(unsigned char)value;
		return;
		}
	if(!str_cmp(name,"SCORE")) {
		score=value;
		return;
		}
	if(!str_cmp(name,"TIMERESET")) {
		time_limit=value;
		return;
		}
	if(!str_cmp(name,"PLAYERFACEDIR")) {
		if(value<0) value=0;
		if(value>3) value=3;
		player_last_dir=(player_last_dir&15)+(value<<4);
		return;
		}
	if(!str_cmp(name,"PLAYERLASTDIR")) {
		if(value<0) value=0;
		if(value>4) value=4;
		player_last_dir=(player_last_dir&240)+value;
		return;
		}
	//Check for overflow on HEALTH and LIVES
	if(!str_cmp(name,"HEALTH")) {
		if(value<0) value=0;
		if(value>health_limit)
			value=health_limit;
		}
	if(!str_cmp(name,"LIVES")) {
		if(value<0) value=0;
		if(value>lives_limit)
			value=lives_limit;
		}
	//Invinco
	if(!str_cmp(name,"INVINCO")) {
		if(!get_counter("INVINCO",0))
			saved_pl_color=player_color;
		else if(value==0) {
			clear_sfx_queue();
			play_sfx(18);
			player_color=saved_pl_color;
			}
		}
	//Now search counter structs
	for(t1=0;t1<NUM_COUNTERS;t1++)
		if(!str_cmp(name,counters[t1].counter_name)) break;
	if(t1<NUM_COUNTERS) {
		if(t1<RESERVED_COUNTERS)//Reserved counters can't go below 0
			if(value<0) value=0;
		counters[t1].counter_value=value;
		return;
		}
	//Not found- search for an empty space AFTER the reserved spaces
	for(t1=RESERVED_COUNTERS;t1<NUM_COUNTERS;t1++)
		if(!counters[t1].counter_value) break;
	if(t1<NUM_COUNTERS) {//Space found
		str_cpy(counters[t1].counter_name,name);
		counters[t1].counter_value=value;
		}
	return;
}

//Take a key. Returns non-0 if none of proper color exist.
char take_key(char color) {
	int t1;
	color&=15;
	for(t1=0;t1<NUM_KEYS;t1++)
		if(keys[t1]==color) break;
	if(t1<NUM_KEYS) {
		keys[t1]=NO_KEY;
		return 0;
		}
	return 1;
}

//Give a key. Returns non-0 if no room.
char give_key(char color) {
	int t1;
	color&=15;
	for(t1=0;t1<NUM_KEYS;t1++)
		if(keys[t1]==NO_KEY) break;
	if(t1<NUM_KEYS) {
		keys[t1]=color;
		return 0;
		}
	return 1;
}

//Increase or decrease a counter.
void inc_counter(char far *name,int value,unsigned char id) {
	long t;
	if(!str_cmp(name,"SCORE")) {//Special score inc. code
		if((score+value)<score) score=4294967295ul;
		else score+=value;
		return;
		}
	t=((long)get_counter(name,id))+value;
	if(t>32767L) t=32767L;
	if(t<-32768L) t=-32768L;
	set_counter(name,(int)t,id);
}

void dec_counter(char far *name,int value,unsigned char id) {
	long t;
	if((!str_cmp(name,"HEALTH"))&&(value>0)) {//Prevent hurts if invinco
		if(get_counter("INVINCO",0)>0) return;
		send_robot_def(0,13);
		if(restart_if_zapped) {
			//Restart since we were hurt
			if((player_restart_x!=player_x)||(player_restart_y!=player_y)) {
				id_remove_top(player_x,player_y);
				player_x=player_restart_x;
				player_y=player_restart_y;
				id_place(player_x,player_y,127,0,0);
				was_zapped=1;
				}
			}
		}
	if(!str_cmp(name,"SCORE")) {//Special score dec. code
		if((score-value)>score) score=0;
		else score-=value;
		return;
		}
	t=((long)get_counter(name,id))-value;
	if(t>32767L) t=32767L;
	if(t<-32768L) t=-32768L;
	set_counter(name,(int)t,id);
}
