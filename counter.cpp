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

// Code for setting and retrieving counters and keys.

#include "sfx.h"
#include "runrobot.h"
#include "idarray.h"
#include "data.h"
#include "const.h"
#include "string.h"
#include "struct.h"
#include <stdlib.h>
#include "counter.h"

int player_restart_x=0,player_restart_y=0;
char was_zapped=0;
extern char mid_prefix;
void prefix_xy(int&fx,int&fy,int&mx,int&my,int&lx,int&ly,int robotx,
	int roboty);

//Get a counter. Include robot id if a robot is running.
int get_counter(char far *name,unsigned char id) {
	int t1;
	if(!str_cmp(name,"LOOPCOUNT")) return robots[id].loop_count;
	//Now search counter structs
	for(t1=0;t1<NUM_COUNTERS;t1++)
		if(!str_cmp(name,counters[t1].counter_name)) break;
	if(t1<NUM_COUNTERS) return counters[t1].counter_value;
	//First, check for robot specific counters
	if(!str_cmp(name,"BULLETTYPE")) return robots[id].bullet_type;
	if(!str_cmp(name,"PLAYERDIST"))
		return abs(robots[id].xpos-player_x)+abs(robots[id].ypos-player_y);
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
	if(!str_cmp(name,"BULLETTYPE")) {
		if(value>2) value=2;
		robots[id].bullet_type=value;
		return;
		}
	//Next, check for global, non-standard counters
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