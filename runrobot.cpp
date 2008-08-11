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

// Functions for running those robots during gameplay

#include "profile.h"
#include "error.h"
#include "ezboard.h"
#include "saveload.h"
#include <stdio.h>
#include "palette.h"
#include "egacode.h"
#include "intake.h"
#include "edit.h"
#include "random.h"
#include "beep.h"
#include "getkey.h"
#include "graphics.h"
#include "arrowkey.h"
#include "scrdisp.h"
#include "window.h"
#include "sfx.h"
extern int topindex,backindex;
#include "mod.h"
#include "idarray.h"
#include "runrobot.h"
#include "struct.h"
#include "const.h"
#include "data.h"
#include "string.h"
#include "counter.h"
#include "game.h"
#include "game2.h"
#include "roballoc.h"
#include <stdlib.h>

#pragma warn -sig

char far *item_to_counter[9]={
	"GEMS",
	"AMMO",
	"TIME",
	"SCORE",
	"HEALTH",
	"LIVES",
	"LOBOMBS",
	"HIBOMBS",
	"COINS" };

// Send all robots or robot ID to default labels. For easy
// interface to ASM. Label ids-
//
// 0=touch
// 1=bombed
// 2=invinco      } automatically sent to "all"
// 3=pushed
// 4=playershot  \
// 5=neutralshot  } if no find, try shot
// 6=enemyshot   /
// 7=playerhit    } automatically sent to "all"
// 8=lazer
// 9=spitfire
//10=justloaded   } automatically sent to "all"
//11=justentered  } automatically sent to "all"
//12=touchgoop		} automatically sent to "all"
//13=playerhurt   } automatically sent to "all"

void send_robot_def(int robot_id,char mesg_id) {
	enter_func("send_robot_def");
	switch(mesg_id) {
		case 0:
			send_robot_id(robot_id,"TOUCH");
			break;
		case 1:
			send_robot_id(robot_id,"BOMBED");
			break;
		case 2:
			send_robot("ALL","INVINCO");
			break;
		case 3:
			send_robot_id(robot_id,"PUSHED");
			break;
		case 4:
			if(!send_robot_id(robot_id,"PLAYERSHOT")) break;
		shot:
			send_robot_id(robot_id,"SHOT");
			break;
		case 5:
			if(send_robot_id(robot_id,"NEUTRALSHOT")) goto shot;
			break;
		case 6:
			if(send_robot_id(robot_id,"ENEMYSHOT")) goto shot;
			break;
		case 7:
			send_robot("ALL","PLAYERHIT");
			break;
		case 8:
			send_robot_id(robot_id,"LAZER");
			break;
		case 9:
			send_robot_id(robot_id,"SPITFIRE");
			break;
		case 10:
			send_robot("ALL","JUSTLOADED");
			break;
		case 11:
			send_robot("ALL","JUSTENTERED");
			break;
		case 12:
			send_robot("ALL","GOOPTOUCHED");
			break;
		case 13:
			send_robot("ALL","PLAYERHURT");
			break;
		}
	exit_func();
}

void send_robot(char far *robot,char far *mesg,char ignore_lock) {
	int t1,t2,t3,t4,x,y;
	char under;
	enter_func("send_robot");
	if(!str_cmp(robot,"ALL")) {
		//Sent to all
		for(t1=1;t1<=NUM_ROBOTS;t1++)
			if(robots[t1].used)
				send_robot_id(t1,mesg,ignore_lock);
		}
	else {
		t1=1;
		do {
			if(!str_cmp(robot,robots[t1].robot_name))
				if(robots[t1].used)
					send_robot_id(t1,mesg,ignore_lock);
		} while(++t1<=NUM_ROBOTS);
		}
	//Sensors
	//Set t2 to cmd- 0-3 move, 4 die, 256+# char, 512+# color (hex)
	t2=-1;//No command yet
	//Check movement commands
	if(mesg[1]==0) {
		t1=mesg[0]; if((t1>='a')&&(t1<='z')) t1-=32;
		switch(t1) {
			case 'N': t2=0; break;
			case 'S': t2=1; break;
			case 'E': t2=2; break;
			case 'W': t2=3; break;
			}
		}
	//Die?
	if(!str_cmp("DIE",mesg)) t2=4;
	//Char___? (___ can be ### or 'c')
	t1=mesg[4];
	mesg[4]=0;
	if(!str_cmp("CHAR",mesg)) t2=256;
	mesg[4]=t1;
	//Color__? (__ is hex)
	t1=mesg[5];
	mesg[5]=0;
	if(!str_cmp("COLOR",mesg)) t2=512;
	mesg[5]=t1;
	//Get char/color value
	if(t2==256) {
		if(mesg[4]=='\'') t2+=(unsigned char)(mesg[5]);
		else t2+=(unsigned char)(atoi(&mesg[4])&255);
		}
	else if(t2==512) t2+=(unsigned char)strtol(&mesg[5],NULL,16);
	if(t2==-1) {
		exit_func();
		return;//Not a legal command
		}
	//Run the sensor cmd
	for(t1=1;t1<NUM_SENSORS;t1++) {
		if(sensors[t1].used) {
			if((!str_cmp(sensors[t1].sensor_name,robot))||
				(!str_cmp(robot,"ALL"))) {
				//Find sensor
				if((t2<256)||(t2>511)) {//Don't bother for a char cmd
					under=0;
					if(level_under_id[player_x+player_y*max_bxsiz]==122) {
						if(level_under_param[player_x+player_y*max_bxsiz]==t1) {
							under=1;
							x=player_x;
							y=player_y;
							goto gotit;
							}
						}
					for(x=0;x<board_xsiz;x++) {
						for(y=0;y<board_ysiz;y++) {
							t3=x+y*max_bxsiz;
							if((level_id[t3]==122)&&(level_param[t3]==t1)) goto gotit;
							}
						}
					continue;//??? Not found!
					}
			gotit:
				//Cmd
				switch(t2) {
					case 0:
					case 1:
					case 2:
					case 3:
						if(under) {
							//Attempt to move player, then if ok,
							//put sensor underneath and delete from
							//original space.
							t3=_move(x,y,t2,1|2|4|8|16);
							if(t3==0) {
								//Moved! Find player...
								find_player();
								goto player_met;
								}
							else //Sensorthud!
								send_robot(sensors[t1].robot_to_mesg,"SENSORTHUD");
							//Done.
							}
						else {
							//Attempt to move sensor.
							t3=_move(x,y,t2,1|2|4|8|16|128);
							if(t3==2) {
								step_sensor(t1);
							player_met:
								t4=player_x+player_y*max_bxsiz;
								//Met player- so put under player!
								under_player_id=level_under_id[t4];
								under_player_param=level_under_param[t4];
								under_player_color=level_under_color[t4];
								level_under_id[t4]=122;
								level_under_param[t4]=t1;
								level_under_color[t4]=level_color[x+y*max_bxsiz];
								id_remove_top(x,y);
								}
							else if(t3!=0) {
								//Sensorthud!
								send_robot(sensors[t1].robot_to_mesg,"SENSORTHUD");
								}
							//Done.
							}
						break;
					case 4:
						if(under) {
							id_remove_under(x,y);
							clear_sensor(t1);
							}
						else {
							id_remove_top(x,y);
							clear_sensor(t1);
							}
						break;
					default:
						if((t2>255)&&(t2<512)) sensors[t1].sensor_char=t2-256;
						else {
							if(under) level_under_color[x+y*max_bxsiz]=t2-512;
							else level_color[x+y*max_bxsiz]=t2-512;
							}
						break;
					}
				}
			}
		}
	//Done.
	exit_func();
}

char send_robot_id(int id,char far *mesg,char ignore_lock) {
	unsigned int t1=1;
	unsigned char far *robot;
	char mesgcopy[257];
	enter_func("send_robot_id");
	char temp=curr_rmem_status;
	if((robots[id].is_locked)&&(!ignore_lock)) {
		exit_func();
		return 1;//Locked
		}
	if(robots[id].program_length<3) {
		exit_func();
		return 2;//No program!
		}
	str_cpy(mesgcopy,mesg);
	prepare_robot_mem(id==NUM_ROBOTS);
	robot=&robot_mem[robots[id].program_location];
  // Are we going to a subroutine? Returning? - Exo
  if(mesgcopy[0] == '#')
  {
    char far *robot_begin = robot_mem + robots[id].program_location;
    // Is there a stack?
    if((robot_begin[2] == 107) && (robot_begin[4] == '#') &&
     robot_begin[3] > 4)
    {
      int stack_pos = (int)robot_begin[5];
      // Check if the stack position is unitialized (*)
      if(stack_pos == '*')
      {
        stack_pos = 0;
      }

      // returning?
      if((!str_cmp(mesgcopy, "#return")) && (stack_pos != 0))
      {
      	// So return there... but the stack pos must NOT be 0...
        robots[id].cur_prog_line =
	       *((int far *)(robot_begin + 6 + (stack_pos << 1)));
	      robots[id].pos_within_line=0;
	      robots[id].cycle_count=robots[id].robot_cycle-1;
        prepare_robot_mem(temp);
        if(robots[id].status==1) robots[id].status=2;
        // And decrease the stack pos.
        robot_begin[5] = (char)stack_pos - 1;
        exit_func();
        return(0); // "Yippee?" :p
      }
      else
      {
        // returning to the TOP?
        if((!str_cmp(mesgcopy, "#top")) && (stack_pos != 0))
        {
      	  // So return there... but the stack pos must NOT be 0...
          robots[id].cur_prog_line =
	        *((int far *)(robot_begin + 6 + 2));
	        robots[id].pos_within_line=0;
	        robots[id].cycle_count=robots[id].robot_cycle-1;
          prepare_robot_mem(temp);
          if(robots[id].status==1) robots[id].status=2;
          // And reset the stack pos.
          robot_begin[5] = 0;
          exit_func();
          return(0); // "Yippee?" :p
        }
        else
        {
          // Sending.. well it'll continue with the jump, just has to
          // put the return address on the stack.
          // Stack must NOT be full though. What's the stack size? (robot_begin[3] - 3)/2.
          if(stack_pos != ((robot_begin[3] - 3) >> 1))
          {
            stack_pos++;
            // The return position is the NEXT command.
            *((int far *)(robot_begin + 6 + (stack_pos << 1))) =
            robots[id].cur_prog_line +
            *(robot_begin + robots[id].cur_prog_line) + 2;
            // Fix stack pos.
            robot_begin[5] = (char)stack_pos;
            // Continue.
          }
        }
      }
    }
  }

  do {
   if(robot[t1+1]==106) {
  	//Label- is it so?
			if(!str_cmp(&robot[t1+3],mesgcopy)) {
				robots[id].cur_prog_line=t1;
				robots[id].pos_within_line=0;
				robots[id].cycle_count=robots[id].robot_cycle-1;
				prepare_robot_mem(temp);
				if(robots[id].status==1) robots[id].status=2;
				exit_func();
				return 0;//Yippe!
				}
			}
	  next_cmd:
		t1+=robot[t1]+2;
	} while(robot[t1]);
	prepare_robot_mem(temp);
	exit_func();
	return 2;
}

char first_prefix=0;//1-3 normal 5-7 is 1-3 but from a REL FIRST cmd
char mid_prefix=0;
char last_prefix=0;//See first_prefix

//Run a set of x/y pairs through the prefixes
void prefix_xy(int&fx,int&fy,int&mx,int&my,int&lx,int&ly,int robotx,
 int roboty) {
	enter_func("prefix_xy");
	if(level_id[player_x+player_y*max_bxsiz]!=127) find_player();
	switch(first_prefix) {
		case 1:
		case 5:
			fx+=robotx;
			fy+=roboty;
			break;
		case 2:
		case 6:
			fx+=player_x;
			fy+=player_y;
			break;
		case 3:
			fx+=get_counter("FIRSTXPOS");
			fy+=get_counter("FIRSTYPOS");
			break;
		case 7:
			fx+=get_counter("XPOS");
			fy+=get_counter("YPOS");
			break;
		}
	switch(mid_prefix) {
		case 1:
			mx+=robotx;
			my+=roboty;
			break;
		case 2:
			mx+=player_x;
			my+=player_y;
			break;
		case 3:
			mx+=get_counter("XPOS");
			my+=get_counter("YPOS");
			break;
		}
	switch(last_prefix) {
		case 1:
		case 5:
			lx+=robotx;
			ly+=roboty;
			break;
		case 2:
		case 6:
			lx+=player_x;
			ly+=player_y;
			break;
		case 3:
			lx+=get_counter("LASTXPOS");
			ly+=get_counter("LASTYPOS");
			break;
		case 7:
			lx+=get_counter("XPOS");
			ly+=get_counter("YPOS");
			break;
		}
	if(fx<0) fx=0;
	if(fy<0) fy=0;
	if(mx<0) mx=0;
	if(my<0) my=0;
	if(lx<0) lx=0;
	if(ly<0) ly=0;
	if(fx>=board_xsiz) fx=board_xsiz-1;
	if(fy>=board_ysiz) fy=board_ysiz-1;
	if(mx>=board_xsiz) mx=board_xsiz-1;
	if(my>=board_ysiz) my=board_ysiz-1;
	if(lx>=board_xsiz) lx=board_xsiz-1;
	if(ly>=board_ysiz) ly=board_ysiz-1;
	exit_func();
}

//Move an x/y pair in a given direction. Returns non-0 if edge reached.
char move_dir(int&x,int&y,char dir) {
	enter_func("move_dir");
	switch(dir) {
		case 0:
			if(y==0) {
				exit_func();
				return 1;
				}
			y--;
			break;
		case 1:
			if(y==board_ysiz-1) {
				exit_func();
				return 1;
				}
			y++;
			break;
		case 2:
			if(x==board_xsiz-1) {
				exit_func();
				return 1;
				}
			x++;
			break;
		case 3:
			if(x==0) {
				exit_func();
				return 1;
				}
			x--;
			break;
		}
	exit_func();
	return 0;
}

//Returns the numeric value pointed to OR the numeric value represented
//by the counter string pointed to. (the ptr is at the param within the
//command)

//NOTE- CLIPS COUNTER NAMES!
int parse_param(unsigned char far *robot,int id) {
	enter_func("parse_param");
	if(robot[0]==0) {//Numeric
#ifdef PROFILE
		int t1=(int)robot[1]+(int)(robot[2]<<8);
		exit_func();
		return t1;
#else
		return (int)robot[1]+(int)(robot[2]<<8);
#endif
		}
	tr_msg(&robot[1],id,ibuff2);
	//String
	if(str_len(ibuff)>=COUNTER_NAME_SIZE)
		ibuff2[COUNTER_NAME_SIZE-1]=0;
#ifdef PROFILE
	int t1=get_counter(ibuff2,id);
	exit_func();
	return t1;
#else
	return get_counter(ibuff2,id);
#endif
}

//Returns location of next parameter (pos is loc of current parameter)
unsigned int next_param(unsigned char far *ptr,unsigned int pos) {
	return pos+(ptr[pos]?ptr[pos]:2)+1;
}

#define parsedir(a,b,c,d) parsedir(a,b,c,d,_bl[0],_bl[1],_bl[2],_bl[3])

//Internal only. NOTE- IF WE EVER ALLOW ZAPPING OF LABELS NOT IN CURRENT
//ROBOT, USE A COPY OF THE *LABEL BEFORE THE PREPARE_ROBOT_MEM!

char restore_label(unsigned char far *robot,unsigned char far *label) {
	unsigned int t1=1,last=0;
	do {
		if(robot[t1+1]==108) {
			//Zapped Label- is it so?
			if(!str_cmp(&robot[t1+3],label))
				last=t1;
			}
	next_cmd:
		t1+=robot[t1]+2;
	} while(robot[t1]);
	if(!last) return 0;
	robot[last+1]=106;
	return 1;
}

char zap_label(unsigned char far *robot,unsigned char far *label) {
	unsigned int t1=1;
	do {
		if(robot[t1+1]==106) {
			//Zapped Label- is it so?
			if(!str_cmp(&robot[t1+3],label)) {
				robot[t1+1]=108;
				return 1;
				}
			}
		t1+=robot[t1]+2;
	} while(robot[t1]);
	return 0;
}

//Turns a color (including those w/??) to a real color (0-255)
unsigned char fix_color(int color,unsigned char def) {
	if(color<256) return color;
	if(color<272) return (color&15)+(def&240);
	if(color<288) return ((color-272)<<4)+(def&15);
	return def;
}

void robot_box_display(unsigned char far *robot,int id,char far *label_storage) {
	//Important status vars (insert kept in intake.cpp)
	long pos=0,old_pos;//Where IN robot?
	int key;//Key
	int t1,t2;
	int old_keyb=curr_table;
	label_storage[0]=0;
	m_show();
	switch_keyb_table(1);
	//Draw screen
	save_screen(current_pg_seg);
	scroll_edging(4);
	//Write robot name
	if(!robots[id].robot_name[0]) write_string("Interaction",35,4,
		scroll_title_color,current_pg_seg);
	else write_string(robots[id].robot_name,40-str_len(robots[id].robot_name)/2,
		4,scroll_title_color,current_pg_seg);
	//Scan section and mark all invalid counter-controlled options as codes
	//249.
	do {
		if(robot[pos+1]==249) robot[pos+1]=105;
		if(robot[pos+1]!=105) goto not_cco;
		t1=parse_param(&robot[pos+2],id);
		if(!t1) robot[pos+1]=249;
	not_cco:
		pos+=robot[pos]+2;
	} while(robot[pos]);
	pos=0;
	//Backwards
	do {
		if(robot[pos+1]==249) robot[pos+1]=105;
		if(robot[pos+1]!=105) goto not_cco2;
		t1=parse_param(&robot[pos+2],id);
		if(!t1) robot[pos+1]=249;
	not_cco2:
		if(robot[pos-1]==0xFF) break;
		pos-=robot[pos-1]+2;
	} while(1);
	pos=0;
	//Loop
	do {
		//Display scroll
		robot_frame(&robot[pos],id);
		key=getkey();
		old_pos=pos;
		switch(key) {
			case MOUSE_EVENT:
				//Move to line clicked on if mouse is in scroll, else exit
				if((mouse_event.cy>=6)&&(mouse_event.cy<=18)&&
					(mouse_event.cx>=8)&&(mouse_event.cx<=71)) {
						t2=mouse_event.cy-12;
						if(t2==0) goto select;
						//t2<0 = PGUP t2 lines
						//t2>0 = PGDN t2 lines
						if(t2<0) goto pgup;
						goto pgdn;
						}
				key=27;
				break;
			case -72://Up
			up_a_line:
				//Go back a line (if possible)
				if(robot[pos-1]==0xFF) break;//Can't.
				pos-=robot[pos-1]+2;
				t1=robot[pos+1];
				if((t1==106)||(t1==249)) goto up_a_line;
				if(((t1<103)&&(t1!=47))||((t1>106)&&(t1<116))||(t1>117)) pos=old_pos;
				//Done.
				break;
			case -80://Down
			down_a_line:
				//Go forward a line (if possible)
				pos+=robot[pos]+2;
				if(robot[pos]==0) pos=old_pos;
				else {
					t1=robot[pos+1];
					if((t1==106)||(t1==249)) goto down_a_line;
					if(((t1<103)&&(t1!=47))||((t1>106)&&(t1<116))||(t1>117)) pos=old_pos;
					}
				//Done.
				break;
			case 13://Enter
			select:
				if((robot[pos+1]!=104)&&(robot[pos+1]!=105)) {//No option
					key=27;
					break;
					}
				if(robot[pos+1]==105) t1=1+next_param(robot,pos+2);
				else t1=pos+3;
				//Goto option! Stores in label_storage
				str_cpy(label_storage,&robot[t1]);
				//Restore screen and exit
				restore_screen(current_pg_seg);
				switch_keyb_table(old_keyb);
				m_hide();
				return;
			case -81://Pagedown (by 6 lines)
				for(t2=6;t2>0;t2--) {
				pgdn:
					//Go forward a line (if possible)
					old_pos=pos;
				pgdn2:
					pos+=robot[pos]+2;
					if(robot[pos]==0) {
						pos=old_pos;
						break;
						}
					t1=robot[pos+1];
					if((t1==106)||(t1==249)) goto pgdn2;
					if(((t1<103)&&(t1!=47))||((t1>106)&&(t1<116))||(t1>117)) {
						pos=old_pos;
						break;
						}
					}
				break;
			case -73://Pageup (by 6 lines)
				for(t2=-6;t2<0;t2++) {
				pgup:
					//Go back a line (if possible)
					if(robot[pos-1]==0xFF) break;
					pos-=robot[pos-1]+2;
					t1=robot[pos+1];
					if((t1==106)||(t1==249)) goto pgup;
					if(((t1<103)&&(t1!=47))||((t1>106)&&(t1<116))||(t1>117)) {
						pos=old_pos;
						break;
						}
					}
				break;
			case -71://Home
				t2=-30000;
				goto pgup;
			case -79://End
				t2=30000;
				goto pgdn;
			default:
				beep();
			case 27:
			case 0:
				break;
			}
		//Continue?
	} while(key!=27);
	//Scan section and mark all invalid counter-controlled options as codes
	//105.
	pos=0;
	do {
		if(robot[pos+1]!=249) goto not_cco3;
		robot[pos+1]=105;
	not_cco3:
		pos+=robot[pos]+2;
	} while(robot[pos]);
	pos=0;
	//Backwards
	do {
		if(robot[pos+1]!=249) goto not_cco4;
		robot[pos+1]=105;
	not_cco4:
		if(robot[pos-1]==0xFF) break;
		pos-=robot[pos-1]+2;
	} while(1);
	//Restore screen and exit
	restore_screen(current_pg_seg);
	switch_keyb_table(old_keyb);
	m_hide();
}

void robot_frame(unsigned char far *robot,int id) {
	//Displays one frame of a robot. The scroll edging, arrows, and title
	//must already be shown. Simply prints each line. The pointer points
	//to the center line.
	int t1,t2,pos=0;
	int old_pos=pos;
	m_hide();
	//Display center line
	fill_line(64,8,12,32+(scroll_base_color<<8),current_pg_seg);
	display_robot_line(robot,12,id);
	//Display lines above center line
	for(t1=11;t1>=6;t1--) {
		fill_line(64,8,t1,32+(scroll_base_color<<8),current_pg_seg);
		//Go backward to previous line
	bk2:
		if(robot[pos-1]<0xFF) {
			pos-=robot[pos-1]+2;
			t2=robot[pos+1];
			if((t2==106)||(t2==249)) goto bk2;
			if(((t2<103)&&(t2!=47))||((t2>106)&&(t2<116))||(t2>117)) {
				pos+=robot[pos]+2;
				continue;
				}
			display_robot_line(&robot[pos],t1,id);
			}
		//Next line...
		}
	//Display lines below center line
	pos=old_pos;
	for(t1=13;t1<=18;t1++) {
		fill_line(64,8,t1,32+(scroll_base_color<<8),current_pg_seg);
		if(robot[pos]==0) continue;
	ps2:
		pos+=robot[pos]+2;
		t2=robot[pos+1];
		if((t2==106)||(t2==249)) goto ps2;
		if(((t2<103)&&(t2!=47))||((t2>106)&&(t2<116))||(t2>117)) {
			pos-=robot[pos-1]+2;
			continue;
			}
		if(robot[pos]) display_robot_line(&robot[pos],t1,id);
		//Next line...
		}
	m_show();
}

void display_robot_line(unsigned char far *robot,int y,int id) {
	int t1;
	switch(robot[1]) {
		case 103://Normal message
			tr_msg(&robot[3],id);
			ibuff[64]=0;//Clip
			write_string(ibuff,8,y,scroll_base_color,current_pg_seg);
			break;
		case 104://Option
			//Skip over label...
			t1=1+next_param(robot,2);//t1 is pos of string
			color_string(tr_msg(&robot[t1],id),10,y,scroll_base_color,current_pg_seg);
			draw_char('',scroll_arrow_color,8,y,current_pg_seg);
			break;
		case 105://Counter-based option
			//Check counter
			t1=parse_param(&robot[2],id);
			if(!t1) break;//Nothing
			//Skip over counter and label...
			t1=1+next_param(robot,next_param(robot,2));//t1 is pos of string
			color_string(tr_msg(&robot[t1],id),10,y,scroll_base_color,current_pg_seg);
			draw_char('',scroll_arrow_color,8,y,current_pg_seg);
			break;
		case 116://Colored message
			color_string(tr_msg(&robot[3],id),8,y,scroll_base_color,current_pg_seg);
			break;
		case 117://Centered message
			t1=str_len_color(tr_msg(&robot[3],id));
			t1=40-t1/2;
			color_string(ibuff,t1,y,scroll_base_color,current_pg_seg);
			break;
		}
	//Others, like 47 and 106, are blank lines
}

void push_sensor(int id) {
	send_robot(sensors[id].robot_to_mesg,"SENSORPUSHED");
}

void step_sensor(int id) {
	send_robot(sensors[id].robot_to_mesg,"SENSORON");
}

//Translates message at target to internal buffer, returning location
//of this buffer. && becomes &, &INPUT& becomes the last input string,
//and &COUNTER& becomes the value of COUNTER. The size of the string is
//clipped to 80 chars.

unsigned char ibuff[161];
unsigned char ibuff2[161];//For another use of tr_msg (parse_param)
unsigned char cnam[COUNTER_NAME_SIZE];

unsigned char far *tr_msg(unsigned char far *orig_mesg,int id,
 unsigned char far *buffer) {
	int sp=0,dp=0,t1;
	enter_func("tr_msg");
	do {
		if(orig_mesg[sp]!='&') buffer[dp++]=orig_mesg[sp++];
		else {
			if(orig_mesg[++sp]=='&') {
				buffer[dp++]='&';
				sp++;
				}
			else {
				//Input or Counter?
				for(t1=0;t1<(COUNTER_NAME_SIZE-1);t1++) {
					cnam[t1]=orig_mesg[sp++];
					if(orig_mesg[sp]==0) break;
					if(orig_mesg[sp]=='&') {
						sp++;
						break;
						}
					}
				cnam[++t1]=0;
				if(!str_cmp(cnam,"INPUT")) {
					//Input
					str_cpy(&buffer[dp],input_string);
					dp+=str_len(input_string);
					}
				else {
					//Counter
          // Now could also be a string
          if(!strn_cmp(cnam, "$string", 7))
          {
            // Write the value of the counter name
            int index = cnam[7] + STRING_BASE;
            str_cpy(&buffer[dp], counters[index].counter_name);
  					dp+=str_len(counters[index].counter_name);
					}
          else
          {
            // #(counter) is a hex representation.
            if(cnam[0] == '+')
            {
              sprintf(cnam, "%x", get_counter(cnam + 1, id));
            }
            else
            {
              if(cnam[0] == '#')
              {              
                char temp[4];
                sprintf(temp, "%x", get_counter(cnam + 1, id));
                if(temp[1] == 0)
                {
                  temp[2] = 0;
                  temp[1] = temp[0];
                  temp[0] = '0';
                }
                mem_cpy(cnam, temp, 4);
              }
              else
              {
  					    itoa(get_counter(cnam,id),cnam,10);
              }
            }

		  			str_cpy(&buffer[dp],cnam);
  					dp+=str_len(cnam);
					}
  			}
      }
    }
		if(dp>80) {
			dp=80;
			break;
			}
	} while(orig_mesg[sp]);
	buffer[dp]=0;
	exit_func();
	return buffer;
}
