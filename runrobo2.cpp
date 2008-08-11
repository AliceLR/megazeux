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

//Part two of RUNROBOT.CPP

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

#define parsedir(a,b,c,d) parsedir(a,b,c,d,_bl[0],_bl[1],_bl[2],_bl[3])

#pragma warn -sig

// side-effects: mangles the input string
// bleah; this is so unreadable; just a very quick dirty hack
static void magic_load_mod(char far *filename) {
	int magic;
	magic = str_len(filename);
	if ( ( magic > 1 ) && ( filename[magic-1] == '*' ) ) {
		filename[magic-1] = '\000';
		load_mod(filename);
		load_mod("*");
	} else {
		load_mod(filename);
	}
}

//Run a single robot through a single cycle.
//If id is negative, only run it if .status is 2
void run_robot(int id,int x,int y) {
	enter_func("run_robot");
	if(id<0) if(robots[-id].status!=2) return;
	int t1,t2,t3,t4,t5,t6,t7,t8,t9,t0,t10,t11,fad=is_faded();//Temps
	int cmd;//Command to run
	int lines_run=0,last_label=-1;//For preventing infinite loops
	char gotoed;//Set to 1 if we shouldn't advance cmd since we went to a lbl
	int old_pos;//Old position to verify gotos DID something
	int _bl[4]={ 0,0,0,0 };//Whether blocked in a given direction (2=OUR bullet)
	unsigned char far *robot;
	unsigned char far *cmd_ptr;//Points to current command
	char done=0;//Set to 1 on a finishing command
	char update_blocked=0;//Set to 1 to update the _bl[4] array
	unsigned char temp[81];
	char first_cmd=1;//It is the first cmd.
	FILE *fp;
	//Reset global prefixes
	first_prefix=mid_prefix=last_prefix=0;
	if(id<0) {
		id=-id;
		robots[id].xpos=x;
		robots[id].ypos=y;
		robots[id].cycle_count=0;
		goto redone;
		}
	//Reset x/y
	robots[id].xpos=x;
	robots[id].ypos=y;
	//Update cycle count
	if((++robots[id].cycle_count)<(robots[id].robot_cycle)) {
		robots[id].status=1;
		exit_func();
		return;
		}
	robots[id].cycle_count=0;
	//Does program exist?
	if(robots[id].program_length<3) {
		exit_func();
		return;//(nope)
		}
	//Walk?
	if((robots[id].walk_dir>0)&&(robots[id].walk_dir<5)) {
		t2=_move(x,y,robots[id].walk_dir-1,1|2|8|16+4*robots[id].can_lavawalk);
		if(t2==3) {//Send to edge, if no response, then to thud.
			if(send_robot_id(id,"edge",1))
				send_robot_id(id,"thud",1);
			}
		else if(t2>0) send_robot_id(id,"thud",1);
		else {
			//Update x/y
			switch(robots[id].walk_dir) {
				case 1: y--; break;
				case 2: y++; break;
				case 3: x++; break;
				case 4: x--; break;
				}
			}
		}
	if(robots[id].cur_prog_line==0) {
		robots[id].status=1;
		goto breaker;//Inactive
		}
redone:
	//Get program location
	prepare_robot_mem(id==NUM_ROBOTS);
	robot=&robot_mem[robots[id].program_location];
	//NOW quit if inactive (had to do walk first)
	if(robot[robots[id].cur_prog_line]==0) {
		robots[id].status=1;
		goto end_prog;
		}
	//Figure blocked vars (accurate until robot program ends OR a put
	//or change command is issued)
	if(id<NUM_ROBOTS) {
		for(t1=0;t1<4;t1++) {
			t4=x; t5=y;
			if(!move_dir(t4,t5,t1)) {
				//Not edge... blocked?
				if((flags[level_id[t4+t5*max_bxsiz]]&A_UNDER)!=A_UNDER) _bl[t1]=1;
				}
			else _bl[t1]=1;//Edge is considered blocked
			}
		}
	if(level_id[player_x+player_y*max_bxsiz]!=127) find_player();
	//Main robot loop
	do {
		gotoed=0;
		old_pos=robots[id].cur_prog_line;
		//Get ptr to command
		cmd_ptr=&robot[robots[id].cur_prog_line+1];
		//Get command number
		cmd=cmd_ptr[0];
		//Act according to command
		t10 = 0;
		switch(cmd) {
			case 0://End
				if(first_cmd) robots[id].status=1;
				goto end_prog;
			case 1://Die
			case 80://Die item
			die:
				clear_robot(id);
				if(id<NUM_ROBOTS) {
					id_remove_top(x,y);
					if(cmd==80) {
						id_remove_top(player_x,player_y);
						player_x=x;
						player_y=y;
						id_place(x,y,127,0,0);
                                                }
                                        }
                                exit_func();
                                return;
                        case 2://Wait
                                t1=(unsigned char)parse_param(&cmd_ptr[1],id);
                                if(t1==robots[id].pos_within_line) break;
                                robots[id].pos_within_line++;
				if(first_cmd) robots[id].status=1;
                                goto breaker;
                        case 3://Cycle
                                robots[id].robot_cycle=(unsigned char)parse_param(&cmd_ptr[1],id);
                                done=1;
                                break;
                        case 4://Go dir num
                                //get num
                                t1=(unsigned char)parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
                                //Inc. pos. or break
                                if(t1==robots[id].pos_within_line) break;
                                robots[id].pos_within_line++;
			case 68://Go dir label
                                if(id==NUM_ROBOTS) break;
                                //Parse dir
                                t1=parsedir(cmd_ptr[2],x,y,robots[id].walk_dir);
                                t1--;
                                if((t1<0)||(t1>3)) break;
                                t2=_move(x,y,t1,1|2|8|16|4*robots[id].can_lavawalk);
                                if((t2>0)&&(cmd==68)) {
                                        //blocked- send to label
                                        send_robot_id(id,tr_msg(&cmd_ptr[next_param(cmd_ptr,1)+1],id),1);
                                        gotoed=1;
                                        break;
					}
                                move_dir(x,y,t1);
                                if(cmd==68) {
                                        //not blocked- make sure only moves once!
                                        done=1;
                                        break;
                                        }
                                goto breaker;
                        case 5://Walk dir
                                if(id==NUM_ROBOTS) break;
                                t1=parsedir(cmd_ptr[2],x,y,robots[id].walk_dir);
                                if((t1<1)||(t1>4)) {
					robots[id].walk_dir=0;
                                        break;
                                        }
                                robots[id].walk_dir=t1;
                                break;
                        case 6://Become color thing param
                                if(id==NUM_ROBOTS) goto die;
                                t1=x+y*max_bxsiz;
                                //Color-
                                t2=level_color[t1];
                                t3=fix_color(parse_param(&cmd_ptr[1],id),t2);
                                level_color[t1]=t3;
				//Point to thing...
                                t4=next_param(cmd_ptr,1);
                                //Thing-
                                level_id[t1]=t6=parse_param(&cmd_ptr[t4],id);
                                //Param- Only change if not becoming a robot
                                if((t6!=123)&&(t6!=124)) {
                                        level_param[t1]=(unsigned char)parse_param(&cmd_ptr[t4+3],id);
                                        clear_robot(id);
                                        //If became a scroll, sensor, or sign...
                                        if((t6==122)||(t6==125)||(t6==126)||(t6==127))
                                                id_remove_top(x,y);
                                        //Delete "under"? (if became another type of "under")
					if(flags[t6]&A_UNDER) {
                                                //Became an under, so delete under.
                                                level_under_param[t1]=level_under_id[t1]=0;
                                                level_under_color[t1]=7;
                                                }
                                        exit_func();
                                        return;
                                        }
                                //Became a robot. Whoopi.
                                break;
                        case 7://Char
                                robots[id].robot_char=(unsigned char)parse_param(&cmd_ptr[1],id);
				break;
                        case 8://Color
                                if(id==NUM_ROBOTS) break;
                                level_color[x+y*max_bxsiz]=fix_color(parse_param(&cmd_ptr[1],id),
                                        level_color[x+y*max_bxsiz]);
                                break;
                        case 9://Gotoxy
                                if(id==NUM_ROBOTS) break;
                                done=1;
                                t1=parse_param(&cmd_ptr[1],id);
                                t2=parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
                                prefix_xy(t3,t3,t1,t2,t3,t3,x,y);
				if((t1==x)&&(t2==y)) break;
                                //delete at x y
                                t3=level_id[t1+t2*max_bxsiz];
                                t4=level_param[t1+t2*max_bxsiz];
                                if(t3==122) clear_sensor(t4);
                                if((t3==126)||(t3==125)) clear_scroll(t4);
                                if((t3==123)||(t3==124)) {
                                        clear_robot(t4);
                                        robot=&robot_mem[robots[id].program_location];
                                        cmd_ptr=&robot[robots[id].cur_prog_line+1];
                                        }
                                if(t3==127) break;//Abort jump if player there
				//move
                                id_place(t1,t2,level_id[x+y*max_bxsiz],level_color[x+y*max_bxsiz],id);
                                //remove
                                id_remove_top(x,y);
                                break;
                        case 10://set c #
                        case 11://inc c #
                        case 12://dec c #
                                tr_msg(&cmd_ptr[2],id);
                                if(str_len(ibuff)>=COUNTER_NAME_SIZE)
                                        ibuff[COUNTER_NAME_SIZE-1]=0;
                                t1=parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
				if(cmd==10) set_counter(ibuff,t1,id);
                                else if(cmd==11) inc_counter(ibuff,t1,id);
                                else dec_counter(ibuff,t1,id);
                                last_label=-1;//Allows looping "infinitely"
                                break;
                        case 16://if c?n l
                                //Get first number
                                t1=parse_param(&cmd_ptr[1],id);
                                //Get second number
                                t2=parse_param(&cmd_ptr[3+next_param(cmd_ptr,1)],id);
                                //Comparison
                                t4=0;//Not true
				switch(parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id)) {
                                        case 0:
                                                if(t1==t2) t4=1;
                                                break;
                                        case 1:
                                                if(t1<t2) t4=1;
                                                break;
                                        case 2:
                                                if(t1>t2) t4=1;
                                                break;
                                        case 3:
                                                if(t1>=t2) t4=1;
						break;
                                        case 4:
                                                if(t1<=t2) t4=1;
                                                break;
                                        case 5:
                                                if(t1!=t2) t4=1;
                                                break;
                                        }
                                if(t4) {
                                        send_robot_id(id,tr_msg(&cmd_ptr[1+next_param(cmd_ptr,
                                                3+next_param(cmd_ptr,1))],id),1);
                                        gotoed=1;
					}
                                break;
                        case 18://if condition label
                        case 19://if not cond. label
                                t1=parse_param(&cmd_ptr[1],id);
                                t2=((unsigned int)t1)>>8;
                                t1=((unsigned int)t1)&255;
                                t2=parsedir(t2,x,y,robots[id].walk_dir);
                                //t1=condition, t2=direction if any (already parsed)
                                t3=0;//Set to 1 for true
				switch(t1) {
                                        case 0://Walking dir
						if(t2<5) {
                                                        if(robots[id].walk_dir==t2) t3=1;
                                                        break;
                                                        }
                                                if(t2==14) {
                                                        if(robots[id].walk_dir==0) t3=1;
                                                        break;
                                                        }
                                                //assumed anydir
                                                if(robots[id].walk_dir!=0) t3=1;
                                                break;
                                        case 1://swimming
						if(id==NUM_ROBOTS) break;
                                                t2=level_under_id[x+y*max_bxsiz];
                                                if((t2>19)&&(t2<25)) t3=1;
                                                break;
                                        case 2://firewalking
                                                if(id==NUM_ROBOTS) break;
                                                t2=level_under_id[x+y*max_bxsiz];
                                                if((t2==26)||(t2==63)) t3=1;
                                                break;
                                        case 3://touching dir
                                                if(id==NUM_ROBOTS) break;
                                                if((t2<5)&&(t2>0)) {
							//Is player in dir t2?
                                                        t4=x; t5=y;
                                                        if(move_dir(t4,t5,t2-1)) break;
                                                        if((player_x==t4)&&(player_y==t5)) t3=1;
                                                        break;
                                                        }
                                                //either anydir or nodir
                                                //is player touching at all?
                                                for(t6=0;t6<4;t6++) {//try all dirs
                                                        t4=x; t5=y;
                                                        if(move_dir(t4,t5,t6)) continue;
                                                        if((player_x==t4)&&(player_y==t5)) t3=1;
							}
                                                //t3=1 for anydir
                                                if((t2==14)||(t2==0)) //We want NODIR though, so
                                                        t3^=1;             //reverse t3.
                                                break;
                                        case 4://Blocked dir
                                                //If REL PLAYER or REL COUNTERS, use special code
                                                if(mid_prefix==2) {
                                                        //Rel player
                                                        for(t1=0;t1<4;t1++) {
                                                                _bl[t1]=0;
                                                                t4=player_x; t5=player_y;
								if(!move_dir(t4,t5,t1)) {
                                                                        //Not edge... blocked?
                                                                        if((flags[level_id[t4+t5*max_bxsiz]]&A_UNDER)!=A_UNDER) _bl[t1]=1;
                                                                        }
                                                                else _bl[t1]=1;//Edge is considered blocked
                                                                }
                                                        if((t2<5)&&(t2>0)) {
                                                                t3=_bl[t2-1];
                                                                }
                                                        else {
                                                                //either anydir or nodir
                                                                //is blocked any dir at all?
								t3=_bl[0]|_bl[1]|_bl[2]|_bl[3];
                                                                //t3=1 for anydir
                                                                if((t2==14)||(t2==0)) //We want NODIR though, so
                                                                        t3^=1;             //reverse t3.
                                                                }
                                                        //Fix blocked arrays
                                                        if(id<NUM_ROBOTS) {
                                                                for(t1=0;t1<4;t1++) {
                                                                        _bl[t1]=0;
                                                                        t4=x; t5=y;
                                                                        if(!move_dir(t4,t5,t1)) {
                                                                                //Not edge... blocked?
										if((flags[level_id[t4+t5*max_bxsiz]]&A_UNDER)!=A_UNDER) _bl[t1]=1;
                                                                                }
                                                                        else _bl[t1]=1;//Edge is considered blocked
                                                                        }
                                                                }
                                                        }
                                                else if(mid_prefix==3) {
                                                        //Rel counters
                                                        t2=get_counter("THISX");
                                                        t3=get_counter("THISY");
                                                        for(t1=0;t1<4;t1++) {
                                                                _bl[t1]=0;
								t4=t2; t5=t3;
                                                                if(!move_dir(t4,t5,t1)) {
                                                                        //Not edge... blocked?
                                                                        if((flags[level_id[t4+t5*max_bxsiz]]&A_UNDER)!=A_UNDER) _bl[t1]=1;
                                                                        }
                                                                else _bl[t1]=1;//Edge is considered blocked
                                                                }
                                                        if((t2<5)&&(t2>0)) {
                                                                t3=_bl[t2-1];
                                                                }
                                                        else {
                                                                //either anydir or nodir
								//is blocked any dir at all?
                                                                t3=_bl[0]|_bl[1]|_bl[2]|_bl[3];
                                                                //t3=1 for anydir
                                                                if((t2==14)||(t2==0)) //We want NODIR though, so
                                                                        t3^=1;             //reverse t3.
                                                                }
                                                        if(id<NUM_ROBOTS) {
                                                                for(t1=0;t1<4;t1++) {
                                                                        _bl[t1]=0;
                                                                        t4=x; t5=y;
                                                                        if(!move_dir(t4,t5,t1)) {
                                                                                //Not edge... blocked?
										if((flags[level_id[t4+t5*max_bxsiz]]&A_UNDER)!=A_UNDER) _bl[t1]=1;
                                                                                }
                                                                        else _bl[t1]=1;//Edge is considered blocked
                                                                        }
                                                                }
                                                        }
                                                else {
                                                        if(id==NUM_ROBOTS) break;
                                                        //Same as player except with anything
                                                        if((t2<5)&&(t2>0)) {
                                                                t3=_bl[t2-1];
                                                                break;
								}
                                                        //either anydir or nodir
                                                        //is blocked any dir at all?
                                                        t3=_bl[0]|_bl[1]|_bl[2]|_bl[3];
                                                        //t3=1 for anydir
                                                        if((t2==14)||(t2==0)) //We want NODIR though, so
                                                                t3^=1;             //reverse t3.
                                                        }
                                                break;
                                        case 5://Aligned
                                                if(id==NUM_ROBOTS) break;
                                                if((player_x==x)||(player_y==y)) t3=1;
						break;
                                        case 6://AlignedNS
                                                if(id==NUM_ROBOTS) break;
                                                if(player_x==x) t3=1;
                                                break;
                                        case 7://AlignedEW
                                                if(id==NUM_ROBOTS) break;
                                                if(player_y==y) t3=1;
                                                break;
                                        case 8://LASTSHOT dir (only 1-4 accepted)
                                                if(id==NUM_ROBOTS) break;
                                                if((t2<1)||(t2>4)) break;
						if(t2==robots[id].last_shot_dir) t3=1;
                                                break;
                                        case 9://LASTTOUCH dir (only 1-4 accepted)
                                                if(id==NUM_ROBOTS) break;
                                                t2=parsedir(t2,x,y,robots[id].walk_dir);
                                                if((t2<1)||(t2>4)) break;
                                                if(t2==robots[id].last_touch_dir) t3=1;
                                                break;
                                        case 10://RTpressed
                                                if(rt_pressed) t3=1;
                                                break;
                                        case 11://LTpressed
						if(lf_pressed) t3=1;
                                                break;
                                        case 12://UPpressed
                                                if(up_pressed) t3=1;
                                                break;
                                        case 13://dnpressed
                                                if(dn_pressed) t3=1;
                                                break;
                                        case 14://sppressed
                                                if(sp_pressed) t3=1;
                                                break;
                                        case 15://delpressed
						if(del_pressed) t3=1;
                                                break;
                                        case 16://musicon
                                                if(music_on) t3=1;
                                                break;
                                        case 17://soundon
                                                if(sfx_on) t3=1;
                                                break;
                                        }
                                if(cmd==19) t3^=1;//Reverse truth if NOT is present
                                if(t3) {//jump
                                        send_robot_id(id,tr_msg(&cmd_ptr[5],id),1);
					gotoed=1;
                                        }
                                break;
                        case 20://if any thing label
                        case 21://if no thing label
                                //Get foreg/backg allowed in t1/t2
                                t1=parse_param(&cmd_ptr[1],id);//Color
                                t3=parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);//Thing
                                t4=parse_param(&cmd_ptr[next_param(cmd_ptr,next_param(cmd_ptr,1))],id);//Param
                                if(t1&256) {
                                        t1^=256;
                                        if(t1==32) t1=t2=16;
					else if(t1<16) t2=16;
                                        else {
                                                t2=t1-16;
                                                t1=16;
                                                }
                                        }
                                else {
                                        t2=t1>>4;
                                        t1=t1&15;
                                        }
                                t5=cmd-20;
                                for(t7=0;t7<10000;t7++) {
					t6=level_id[t7];
                                        if(t6!=t3) continue;
                                        t6=level_color[t7];
                                        if(t1<16) {
                                                if((t6&15)!=t1) continue;
                                                }
                                        if(t2<16) {
                                                if((t6>>4)!=t2) continue;
                                                }
                                        if(t4<256) {
                                                t6=level_param[t7];
                                                if(t6!=t4) continue;
						}
                                        t5=21-cmd;
                                        break;
                                        }
                                if(t5) {
                                        send_robot_id(id,tr_msg(&cmd_ptr[1+next_param(cmd_ptr,
                                                next_param(cmd_ptr,next_param(cmd_ptr,1)))],id),1);
                                        gotoed=1;
                                        }
                                break;
                        case 22://if thing dir
                        case 23://if NOT thing dir
				t1=x; t2=y;
                                t4=1;
                                t3=next_param(cmd_ptr,next_param(cmd_ptr,next_param(cmd_ptr,t4)));
                                t9=1+next_param(cmd_ptr,t3);
                                t3=parse_param(&cmd_ptr[t3],id);
                                goto if_thing_dir;
                        case 24://if thing x y
                                t4=1;
                                t3=next_param(cmd_ptr,next_param(cmd_ptr,next_param(cmd_ptr,t4)));
                                t9=1+next_param(cmd_ptr,next_param(cmd_ptr,t3));
                                t1=parse_param(&cmd_ptr[t3],id);
                                t2=parse_param(&cmd_ptr[next_param(cmd_ptr,t3)],id);
				prefix_xy(t8,t8,t1,t2,t8,t8,x,y);
				t10 = 0;
				goto if_thing_at_xy;
                        case 25://if at x y label
                                if(id==NUM_ROBOTS) break;
                                t1=parse_param(&cmd_ptr[1],id);
                                t2=parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
                                prefix_xy(t3,t3,t1,t2,t3,t3,x,y);
                                if((t1!=x)||(t2!=y)) break;
                                send_robot_id(id,tr_msg(&cmd_ptr[1+next_param(cmd_ptr,
                                        next_param(cmd_ptr,1))],id),1);
                                gotoed=1;
                                break;
			case 26://if dir of player is thing, "label"

				t1=player_x; t2=player_y;// X/Y
				t3=parse_param(&cmd_ptr[1],id);// Direction
				t4=next_param(cmd_ptr,1);// Location of thing
				// Byte of label string
				t9=1+next_param(cmd_ptr,next_param(cmd_ptr,next_param(cmd_ptr,t4)));
			if_thing_dir:
				t10 = 0;
				t3=parsedir(t3,x,y,robots[id].walk_dir);
				// if((t3==11)&&(cmd==26)) {
			   if(t3==11) {
					t10=999; //set t10, used to set cmd
					goto if_thing_at_xy;
					}
				if((t3<1)||(t3>4)) goto if_thing_no;
				if(move_dir(t1,t2,t3-1)) goto if_thing_no;
				t10 = 0;
			if_thing_at_xy:
				//t3/t4 <- color (fg/bk)
                                //t5 <- thing
                                //t6 <- param
                                t3=parse_param(&cmd_ptr[t4],id);
                                t5=parse_param(&cmd_ptr[next_param(cmd_ptr,t4)],id);
                                t6=parse_param(&cmd_ptr[next_param(cmd_ptr,
                                        next_param(cmd_ptr,t4))],id);
                                if(t3&256) {
                                        t3^=256;
                                        if(t3<16) t4=16;
                                        else if(t3==32) t3=t4=16;
                                        else {
                                                t4=t3-16;
                                                t3=16;
                                                }
                                        }
                                else {
                                        t4=t3>>4;
                                        t3=t3&15;
                                        }
                                if(t10==999) goto under_player; //checks t10, used to check cmd
                                t7=level_id[t1+t2*max_bxsiz];
                                if(t7!=t5) goto if_thing_no;
                                t7=level_color[t1+t2*max_bxsiz];
                                if(t3<16)
                                        if((t7&15)!=t3) goto if_thing_no;
                                if(t4<16)
                                        if((t7>>4)!=t4) goto if_thing_no;
                                if(t6<256) {
                                        t7=level_param[t1+t2*max_bxsiz];
                                        if(t7!=t6) goto if_thing_no;
                                        }
                                //Jump to label if not cmd 23
                                if(cmd==23) break;
                                send_robot_id(id,tr_msg(&cmd_ptr[t9],id),1);
                                gotoed=1;
                                break;
                        if_thing_no:
                                //Jump to label if cmd 23
                                if(cmd==23) {
                                        send_robot_id(id,tr_msg(&cmd_ptr[t9],id),1);
                                        gotoed=1;
                                        }
                                break;
                        under_player:
                                //If UNDER/BENEATH PLAYER thing  ( and if thing under)
                                t7=level_under_id[t1+t2*max_bxsiz];
                                if(t7!=t5) goto if_thing_no; //for next few statements, did not used to go to if_thing_no
                                t7=level_under_color[t1+t2*max_bxsiz];
                                if(t3<16)
                                        if((t7&15)!=t3) goto if_thing_no;
                                if(t4<16)
                                        if((t7>>4)!=t4) goto if_thing_no;
                                if(t6<256) {
                                        t7=level_under_param[t1+t2*max_bxsiz];
                                        if(t7!=t6) goto if_thing_no;
                                        }
                        //POSSIBLE FIX FOR UNDER BUGGY!
                                if(cmd==23) break;
                                send_robot_id(id,tr_msg(&cmd_ptr[t9],id),1);
                                gotoed=1;
                                break;
                        case 27://double c
                                tr_msg(&cmd_ptr[2],id);
                                if(!str_cmp(ibuff,"SCORE")) score<<=1;
                                else {
                                        t1=get_counter(ibuff,id);
                                        t1<<=1;
                                        set_counter(ibuff,t1,id);
                                        }
                                last_label=-1;//Allows looping "infinitely"
                                break;
                        case 28://half c
                                tr_msg(&cmd_ptr[2],id);
                                if(!str_cmp(ibuff,"SCORE")) score>>=1;
                                else {
                                        t1=get_counter(ibuff,id);
                                        t1>>=1;
                                        set_counter(ibuff,t1,id);
                                        }
                                last_label=-1;//Allows looping "infinitely"
                                break;
                        case 29://Goto
                                send_robot_id(id,tr_msg(&cmd_ptr[2],id),1);
                                gotoed=1;
                                break;
                        case 30://Send robot label
                                send_robot(tr_msg(&cmd_ptr[2],id,ibuff2),tr_msg(
                                        &cmd_ptr[next_param(cmd_ptr,1)+1],id));
                                break;
                        case 31://Explode
                                if(id==NUM_ROBOTS) goto die;
                                level_param[x+y*max_bxsiz]=((unsigned char)parse_param(&cmd_ptr[1],id)-1)*16;
                                level_id[x+y*max_bxsiz]=38;
                                clear_robot(id);
                                exit_func();
                                return;
                        case 32://put thing dir
                                if(id==NUM_ROBOTS) break;
                                //t1=color (w/?) t2=thing t3=param
                                t0=1;
                                t1=parse_param(&cmd_ptr[t0],id); t0=next_param(cmd_ptr,t0);
                                t2=parse_param(&cmd_ptr[t0],id); t0=next_param(cmd_ptr,t0);
                                t3=parse_param(&cmd_ptr[t0],id); t0=next_param(cmd_ptr,t0);
                                //t4 x t5 y t6 dir
                        lay_bomb_entry:
                                t4=x; t5=y;
                                t6=parsedir(parse_param(&cmd_ptr[t0],id),x,y,robots[id].walk_dir);
                        put_to_dir:
                                if((t6<1)||(t6>4)) break;
                                if(move_dir(t4,t5,t6-1)) break;
                        put_at_XY:
                                if((t2>121)&&(t2<128)) break;
                        duplicate_entry:
                                t7=level_id[t4+t5*max_bxsiz];
                                t8=level_param[t4+t5*max_bxsiz];
                                if(t7==127) break;
                                if((t7==123)||(t7==124)) {
                                        clear_robot(t8);
                                        robot=&robot_mem[robots[id].program_location];
                                        cmd_ptr=&robot[robots[id].cur_prog_line+1];
                                        }
                                if((t7==125)||(t7==126))
                                        clear_scroll(t8);
                                if(t8==122)
                                        clear_sensor(t8);
                                t1=fix_color(t1,level_color[t4+t5*max_bxsiz]);
                                //Put thing
                                id_place(t4,t5,t2,t1,t3);
                                //Are we still "alive"?
                                if(id!=GLOBAL_ROBOT) {
                                        t1=level_id[x+y*max_bxsiz];
                                        if((t1!=123)&&(t1!=124)) {
                                                exit_func();
                                                return;//Nope.
                                                }
                                        t1=level_param[x+y*max_bxsiz];
                                        if(t1!=id) {
                                                exit_func();
                                                return;//Nope.
                                                }
                                        }
                                //Figure blocked vars
                                update_blocked=1;
                                break;
                        case 33://Give # item
                                t1=parse_param(&cmd_ptr[1],id);
                                t2=cmd_ptr[next_param(cmd_ptr,1)+1];
                                inc_counter(item_to_counter[t2],t1);
                                last_label=-1;//Allows looping "infinitely"
                                break;
                        case 34://Take # item
                        case 35://Take # item "label"
                                t1=parse_param(&cmd_ptr[1],id);
                                t2=cmd_ptr[next_param(cmd_ptr,1)+1];
                                t4=0;
                                if(t2!=3) {
                                        t3=get_counter(item_to_counter[t2]);
                                        if(t3<t1) t4=1;
                                        else dec_counter(item_to_counter[t2],t1);
                                        }
                                else {
                                        if(score<t1) t4=1;
                                        else score-=t1;
                                        }
                                if((t4)&(cmd==35)) {
                                        send_robot_id(id,tr_msg(&cmd_ptr[next_param(cmd_ptr,1)+4],
                                                id),1);
                                        gotoed=1;
                                        }
                                last_label=-1;//Allows looping "infinitely"
                                break;
                        case 36://Endgame
                                set_counter("LIVES",0);
                        case 37://Endlife
                                set_counter("HEALTH",0);
                                break;
                        case 38://Mod
                                magic_load_mod(tr_msg(&cmd_ptr[2],id));
                                volume_mod(volume);
                                break;
                        case 39://sam
                                t1=parse_param(&cmd_ptr[1],id);//Freq
                                t2=next_param(cmd_ptr,1)+1;//Loc of string
                                play_sample(t1,tr_msg(&cmd_ptr[t2],id));
                                break;
                        case 40://Volume
                                volume=volume_target=t1=(unsigned char)parse_param(&cmd_ptr[1],id);
                                volume_mod(t1);
                                break;
                        case 41://End mod
                                end_mod();
                                break;
                        case 42://End sam
                                end_sample();
                                break;
                        case 43://Play notes
                                play_str(&cmd_ptr[2]);
                                break;
                        case 44://End play
                                clear_sfx_queue();
                                break;
                        case 45://wait play "str"
                        case 46://wait play
                                t1=topindex-backindex;
                                if(t1<0) t1=topindex+(NOISEMAX-backindex);
                                if(t1>10) goto breaker;
                                if(cmd==45) play_str(&cmd_ptr[2]);
                                break;
                        //case 47:Blank line
                        case 48://sfx num
                                play_sfx(parse_param(&cmd_ptr[1],id));
                                break;
                        case 49://play sfx notes
                                if(!is_playing()) play_str(&cmd_ptr[2]);
                                break;
                        case 50://open
                                if(id==NUM_ROBOTS) break;
                                t1=parsedir(parse_param(&cmd_ptr[1],id),x,y,robots[id].walk_dir);
                                if((t1<1)||(t1>4)) break;
                                t2=x; t3=y;
                                if(move_dir(t2,t3,t1-1)) break;
                                t4=level_id[t2+t3*max_bxsiz];
                                if((t4!=41)&&(t4!=47)) break;
                                //Become pushable for a split second
                                t6=level_id[x+y*max_bxsiz];
                                level_id[x+y*max_bxsiz]=123;
                                grab_item(t2,t3,0);
                                //Re-find robot
                                if(level_id[x+y*max_bxsiz]!=123); {
                                        for(t1=0;t1<4;t1++) {
                                                t4=x; t5=y;
                                                if(!move_dir(t4,t5,t1)) {
                                                        //Not edge... robot?
                                                        if(level_id[t4+t5*max_bxsiz]==123) {
                                                                if(level_param[t4+t5*max_bxsiz]==id) {
                                                                        x=t4;
                                                                        y=t5;
                                                                        break;
                                                                        }
                                                                }
                                                        }
                                                }
                                        }
                                //Become old pushable status
                                level_id[x+y*max_bxsiz]=t6;
                                //Figure blocked vars
                                update_blocked=1;
                                break;
                        case 51://Lockself
                        case 52://Unlockself
                                robots[id].is_locked=52-cmd;
                                break;
                        case 53://Send DIR "label"
                                if(id==NUM_ROBOTS) break;
                                t1=x; t2=y;
                                t3=parse_param(&cmd_ptr[1],id);
                                t4=1+next_param(cmd_ptr,1);
                        send_dir:
                                //t1/t2=x/y
                                //t3=dir
                                //t4=loc of label
                                t3=parsedir(t3,x,y,robots[id].walk_dir);
                                if((t3<1)||(t3>4)) break;
                                if(move_dir(t1,t2,t3-1)) break;
                        send_at_XY:
                                t3=level_id[t1+t2*max_bxsiz];
                                if((t3!=123)&&(t3!=124)) break;
                                t3=level_param[t1+t2*max_bxsiz];
                                send_robot_id(t3,tr_msg(&cmd_ptr[t4],id));
                                 //Use t4 instead of 2 'cuase send
                                 //x y "label" uses 3.
                                break;
                        case 54://Zap label num
                        case 55://Restore label num
                                tr_msg(&cmd_ptr[2],id);
                                t1=(unsigned char)parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
                                for(t2=0;t2<t1;t2++) {
                                        if(cmd==54) {
                                                if(zap_label(robot,ibuff)==0) break;
                                                }
                                        else {
                                                if(restore_label(robot,ibuff)==0) break;
                                                }
                                        }
                                break;
                        case 56://Lockplayer
                                player_ns_locked=player_ew_locked=player_attack_locked=1;
                                break;
                        case 57://unlockplayer
                                player_ns_locked=player_ew_locked=player_attack_locked=0;
                                break;
                        case 58://lockplayer ns
                                player_ns_locked=1;
                                break;
                        case 59://lockplayer ew
                                player_ew_locked=1;
                                break;
                        case 60://lockplayer attack
                                player_attack_locked=1;
                                break;
                        case 61://move player dir
                        case 62://move pl dir "label"
                                t1=parsedir(parse_param(&cmd_ptr[1],id),x,y,robots[id].walk_dir);
                                if((t1<1)||(t1>4)) break;
                                t2=player_x; t3=player_y; t4=curr_board;
                                //Have to fix vars and move to next command NOW, in case player
                                //is send to another screen!
                                first_prefix=mid_prefix=last_prefix=0;
                                robots[id].pos_within_line=0;
                                robots[id].cur_prog_line+=robot[robots[id].cur_prog_line]+2;
                                if(!robot[robots[id].cur_prog_line])
                                        robots[id].cur_prog_line=0;
                                robots[id].cycle_count=0;
                                robots[id].xpos=x;
                                robots[id].ypos=y;
                                //Move player
                                t0=target_where;
                                move_player(t1-1);
                                if((player_x==t2)&&(player_y==t3)&&(curr_board==t4)&&(cmd==62)&&
                                        (target_where==t0)) {
                                                send_robot_id(id,tr_msg(&cmd_ptr[1+next_param(cmd_ptr,1)],
                                                        id),1);
                                                gotoed=1;
                                                goto breaker;
                                                }
                                exit_func();
                                return;
                        case 63://Put player x y
                                t1=parse_param(&cmd_ptr[1],id);
                                t2=parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
                                prefix_xy(t3,t3,t1,t2,t3,t3,x,y);
                        put_player_XY:
                                //Put at t1,t2
                                done=1;
                                if((player_x==t1)&&(player_y==t2)) break;
                                id_remove_top(player_x,player_y);
                                t3=level_id[t1+t2*max_bxsiz];
                                t4=level_param[t1+t2*max_bxsiz];
                                if((t3==123)||(t3==124)) {
                                        clear_robot(t4);
                                        robot=&robot_mem[robots[id].program_location];
                                        cmd_ptr=&robot[robots[id].cur_prog_line+1];
                                        }
                                if((t3==125)||(t3==126))
                                        clear_scroll(t4);
                                if(t3==122) step_sensor(t4);
                                id_place(t1,t2,127,0,0);
                                player_x=t1;
                                player_y=t2;
                                //still alive?
                                if((player_x==x)&&(player_y==y)) {
                                        exit_func();
                                        return;
                                        }
                                break;
                        case 66://if player x y
                                t1=parse_param(&cmd_ptr[1],id);
                                t2=parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
                                prefix_xy(t3,t3,t1,t2,t3,t3,x,y);
                                if((t1!=player_x)||(t2!=player_y)) break;
                                send_robot_id(id,tr_msg(&cmd_ptr[next_param(cmd_ptr,
                                        next_param(cmd_ptr,1))+1],id),1);
                                gotoed=1;
                                break;
                        case 67://put player dir
                                if(id==NUM_ROBOTS) break;
                                t1=x; t2=y;
                                t3=parsedir(parse_param(&cmd_ptr[1],id),x,y,robots[id].walk_dir);
                                if((t3<1)||(t3>4)) break;
                                if(move_dir(t1,t2,t3-1)) break;
                                goto put_player_XY;
                        case 69://rotate cw
                        case 70://rotate ccw
                                if(id==NUM_ROBOTS) break;
                                rotate(x,y,cmd-69);
                                //Figure blocked vars
                                update_blocked=1;
                                break;
			case 71://switch dir dir
                                if(id==NUM_ROBOTS) break;
                                t1=t4=x; t2=t5=y;
                                t3=parsedir(parse_param(&cmd_ptr[1],id),x,y,robots[id].walk_dir);
                                if((t3<1)||(t3>4)) break;
                                if(move_dir(t1,t2,t3-1)) break;
                                t6=parsedir(parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id)
                                        ,x,y,robots[id].walk_dir);
                                if((t6<1)||(t6>4)) break;
                                if(move_dir(t4,t5,t6-1)) break;
                                //Switch t1,t2 with t4,t5
                                if((t1==t4)&&(t2==t5)) break;
                                t3=level_id[t1+t2*max_bxsiz];
                                t6=level_param[t1+t2*max_bxsiz];
                                t7=level_color[t1+t2*max_bxsiz];
                                level_id[t1+t2*max_bxsiz]=level_id[t4+t5*max_bxsiz];
                                level_param[t1+t2*max_bxsiz]=level_param[t4+t5*max_bxsiz];
                                level_color[t1+t2*max_bxsiz]=level_color[t4+t5*max_bxsiz];
                                level_id[t4+t5*max_bxsiz]=t3;
                                level_param[t4+t5*max_bxsiz]=t6;
                                level_color[t4+t5*max_bxsiz]=t7;
                                //Figure blocked vars
                                update_blocked=1;
                                break;
                        case 72://Shoot
                                if(id==NUM_ROBOTS) break;
                                t3=parsedir(cmd_ptr[2],x,y,robots[id].walk_dir);
                                if((t3<1)||(t3>4)) break;
                                if(_bl[--t3]&2) break;//No shooty a shot
                                _shoot_c(x,y,t3,robots[id].bullet_type);
                                //Figure blocked vars
                                if(!_bl[t3]) _bl[t3]=3;
                                break;
                        case 73://laybomb
                                t1=0;
                                goto lay_bomb;
                        case 74://laybomb high
                                t1=128;
                        lay_bomb:
                                if(id==NUM_ROBOTS) break;
                                t3=parsedir(parse_param(&cmd_ptr[1],id),x,y,robots[id].walk_dir);
                                if(t3==11) {
                                        //underneath
                                        level_under_id[x+y*max_bxsiz]=37;
                                        level_under_param[x+y*max_bxsiz]=t1;
                                        level_under_color[x+y*max_bxsiz]=8;
                                        break;
                                        }
                                if((t3<1)||(t3>4)) break;
                                //Simulate a put
                                t3=t1; t1=8; t2=37; t0=1;
                                goto lay_bomb_entry;
                        case 75://shoot missile
                                if(id==NUM_ROBOTS) break;
                                t3=parsedir(parse_param(&cmd_ptr[1],id),x,y,robots[id].walk_dir);
                                if((t3<1)||(t3>4)) break;
                                _shoot_missile_c(x,y,t3-1);
                                //Figure blocked vars
                                update_blocked=1;
                                break;
                        case 76://shoot seeker
                                if(id==NUM_ROBOTS) break;
                                t3=parsedir(parse_param(&cmd_ptr[1],id),x,y,robots[id].walk_dir);
                                if((t3<1)||(t3>4)) break;
                                _shoot_seeker_c(x,y,t3-1);
                                //Figure blocked vars
                                update_blocked=1;
                                break;
                        case 77://spit fire
                                if(id==NUM_ROBOTS) break;
                                t3=parsedir(parse_param(&cmd_ptr[1],id),x,y,robots[id].walk_dir);
                                if((t3<1)||(t3>4)) break;
                                _shoot_fire_c(x,y,t3-1);
                                //Figure blocked vars
                                update_blocked=1;
                                break;
                        case 78://lazer wall
                                if(id==NUM_ROBOTS) break;
                                t1=(unsigned char)parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
                                t3=parsedir(cmd_ptr[2],x,y,robots[id].walk_dir);
                                if((t3<1)||(t3>4)) break;
                                _shoot_lazer_c(x,y,t3-1,t1,level_color[x+y*max_bxsiz]);
                                //Figure blocked vars
                                update_blocked=1;
                                break;
                        case 79://put thing at x y
                                t0=1;
                                t1=parse_param(&cmd_ptr[t0],id); t0=next_param(cmd_ptr,t0);
                                t2=parse_param(&cmd_ptr[t0],id); t0=next_param(cmd_ptr,t0);
                                t3=parse_param(&cmd_ptr[t0],id); t0=next_param(cmd_ptr,t0);
                                t4=parse_param(&cmd_ptr[t0],id); t0=next_param(cmd_ptr,t0);
                                t5=parse_param(&cmd_ptr[t0],id); t0=next_param(cmd_ptr,t0);
                                prefix_xy(t9,t9,t4,t5,t9,t9,x,y);
                                goto put_at_XY;
                        case 81://Send x y "label"
                                t1=parse_param(&cmd_ptr[1],id);
                                t2=parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
                                t4=1+next_param(cmd_ptr,next_param(cmd_ptr,1));
                                prefix_xy(t3,t3,t1,t2,t3,t3,x,y);
                                goto send_at_XY;
                        case 82://copyrobot ""
                                tr_msg(&cmd_ptr[2],id);
                                for(t1=0;t1<=NUM_ROBOTS;t1++)
                                        if(str_cmp(robots[t1].robot_name,ibuff)==0) break;
                                if(t1>NUM_ROBOTS) break;
                        copy_robot_num:
                                if(t1==id) break;
                                //Finally- Copy!
                                copy_robot(id,t1);
                                if(robots[id].cur_prog_line>0)
                                        robots[id].cur_prog_line=1;
                                robots[id].pos_within_line=0;
                                robot=&robot_mem[robots[id].program_location];
                                goto breaker;
                        case 83://copyrobot x y
                                t1=parse_param(&cmd_ptr[1],id);
                                t2=parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
                                prefix_xy(t3,t3,t1,t2,t3,t3,x,y);
                        copy_robot_at_XY:
                                t3=level_id[t1+t2*max_bxsiz];
                                if((t3!=123)&&(t3!=124)) break;
                                t1=level_param[t1+t2*max_bxsiz];
                                goto copy_robot_num;
                        case 84://copyrobot dir
                                if(id==NUM_ROBOTS) break;
                                t1=x; t2=y;
                                t3=parsedir(parse_param(&cmd_ptr[1],id),x,y,robots[id].walk_dir);
                                if((t3<1)||(t3>4)) break;
                                if(move_dir(t1,t2,t3-1)) break;
                                goto copy_robot_at_XY;
                        case 85://dupe self dir
                                if(id==NUM_ROBOTS) break;
                                t4=x; t5=y;
                                t3=parsedir(parse_param(&cmd_ptr[1],id),x,y,robots[id].walk_dir);
                                if((t3<1)||(t3>4)) break;
                                if(move_dir(t4,t5,t3-1)) break;
                                goto dupe_at_XY;
                        case 86://dupe self xy
                                t4=parse_param(&cmd_ptr[1],id);
                                t5=parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
                                prefix_xy(t3,t3,t4,t5,t3,t3,x,y);
                        dupe_at_XY:
                                //Make copy
                                t3=find_robot();
                                if(!t3) break;
                                if(copy_robot(t3,id)) {
                                        robots[t3].used=0;
                                        break;
                                        }
                                robot=&robot_mem[robots[id].program_location];
                                cmd_ptr=&robot[robots[id].cur_prog_line+1];
                                robots[t3].cur_prog_line=1;
                                robots[t3].pos_within_line=0;
                                if(id!=GLOBAL_ROBOT) {
                                        t1=level_color[x+y*max_bxsiz];
                                        t2=level_id[x+y*max_bxsiz];
                                        }
                                else {
                                        t1=7;
                                        t2=124;
                                        }
                                goto duplicate_entry;
                        case 87://bulletn
                        case 88://s
                        case 89://e
                        case 90://w
                                t1=cmd-87;
                                bullet_char[t1]=bullet_char[t1+4]=bullet_char[t1+8]=
                                        parse_param(&cmd_ptr[1],id);
                                break;
                        case 91://givekey col
                        case 92://givekey col "l"
                                t1=15&parse_param(&cmd_ptr[1],id);//Color
                                if(give_key(t1))
                                        if(cmd==92) {
                                                send_robot_id(id,tr_msg(&cmd_ptr[1+next_param(cmd_ptr,1)],
                                                        id),1);
                                                gotoed=1;
                                                }
                                break;
                        case 93://takekey col
                        case 94://takekey col "l"
                                t1=15&parse_param(&cmd_ptr[1],id);//Color
                                if(take_key(t1))
                                        if(cmd==94) {
                                                send_robot_id(id,tr_msg(&cmd_ptr[1+next_param(cmd_ptr,1)],
                                                        id),1);
                                                gotoed=1;
                                                }
                                break;
                        case 95://inc c r
                        case 96://dec c r
                        case 97://set c r
                                tr_msg(&cmd_ptr[2],id);
                                if(str_len(ibuff)>=COUNTER_NAME_SIZE)
                                        ibuff[COUNTER_NAME_SIZE-1]=0;
                                t1=next_param(cmd_ptr,1);
                                //t1 points to low range to set to
                                t2=parse_param(&cmd_ptr[t1],id);
                                t1=next_param(cmd_ptr,t1);
                                t3=parse_param(&cmd_ptr[t1],id);
                                //Get random number from t2 to t3
                                if(t2==t3) t4=t2;
                                else {
                                        if(t3<t2) {
                                                t4=t2;
                                                t2=t3;
                                                t3=t4;
                                                }
                                        t4=((random_num())%(t3-t2+1))+t2;
                                        //t4 <- random num
                                        }
                                if(cmd==97) set_counter(ibuff,t4,id);
                                else if(cmd==95) inc_counter(ibuff,t4,id);
                                else dec_counter(ibuff,t4,id);
                                last_label=-1;//Allows looping "infinitely"
                                break;
                        case 98://Trade givenum givetype takenum taketype poorlabel
                                t1=1;
                                t2=parse_param(&cmd_ptr[t1],id); t1=next_param(cmd_ptr,t1);
                                t3=parse_param(&cmd_ptr[t1],id); t1=next_param(cmd_ptr,t1);
                                t4=parse_param(&cmd_ptr[t1],id); t1=next_param(cmd_ptr,t1);
                                t5=parse_param(&cmd_ptr[t1],id); t1=next_param(cmd_ptr,t1);
                                t9=0;
                                if(t5!=3) {
                                        t6=get_counter(item_to_counter[t5]);
                                        if(t6<t4) t9=1;
                                        else set_counter(item_to_counter[t5],t6-t4);
                                        }
                                else {
                                        if(score<t6) t9=1;
                                        else score-=t4;
                                        }
                                if(t9) {
                                        send_robot_id(id,tr_msg(&cmd_ptr[1+t1],id),1);
                                        gotoed=1;
                                        }
                                else {
                                        if(t3!=3) inc_counter(item_to_counter[t3],t2);
                                        else score+=t2;
                                        }
                                last_label=-1;//Allows looping "infinitely"
                                break;
                        case 99://Send DIR of player "label"
                                t1=player_x; t2=player_y;
                                t3=parse_param(&cmd_ptr[1],id);
                                t4=1+next_param(cmd_ptr,1);
                                goto send_dir;
                        case 100://put thing dir of player
                                t0=1;
                                t1=parse_param(&cmd_ptr[t0],id); t0=next_param(cmd_ptr,t0);
                                t2=parse_param(&cmd_ptr[t0],id); t0=next_param(cmd_ptr,t0);
                                t3=parse_param(&cmd_ptr[t0],id); t0=next_param(cmd_ptr,t0);
                                t6=parsedir(parse_param(&cmd_ptr[t0],id),x,y,robots[id].walk_dir);
                                t4=player_x; t5=player_y;
                                if(t6==11) {
                                        //Put BENEATH player
                                        if((t2>121)&&(t2<128)) break;
                                        t1=fix_color(t1,level_under_color[t4+t5*max_bxsiz]);
                                        t7=level_under_id[t4+t5*max_bxsiz];
                                        t8=level_under_param[t4+t5*max_bxsiz];
                                        if(t7==122)
                                                clear_sensor(t8);
                                        //Put it now.
                                        level_under_id[t4+t5*max_bxsiz]=t2;
                                        level_under_color[t4+t5*max_bxsiz]=t1;
                                        level_under_param[t4+t5*max_bxsiz]=t3;
                                        break;
                                        }
                                goto put_to_dir;
                        case 101:// /"dirs"
                        case 232://Persistent go [str]
                                if(id==NUM_ROBOTS) break;
                                //get next dir char, if none, next cmd
                                t1=cmd_ptr[++robots[id].pos_within_line+1];
                                //t1='n' 's' 'w' 'e' or 'i'
                                if(t1=='n') t1=0;
                                else if(t1=='N') t1=0;
                                else if(t1=='s') t1=1;
                                else if(t1=='S') t1=1;
                                else if(t1=='e') t1=2;
                                else if(t1=='E') t1=2;
                                else if(t1=='w') t1=3;
                                else if(t1=='W') t1=3;
                                else if(t1=='i') goto breaker;
                                else if(t1=='I') goto breaker;
                                else goto next_cmd;//invalid char means next cmd (including null)
                                //move in dir
                                if((_move(x,y,t1,1|2|8|16|4*robots[id].can_lavawalk))&&//4 lava
                                        (cmd==232)) robots[id].pos_within_line--;//persistent...
                                move_dir(x,y,t1);
                                goto breaker;
                        case 102://Mesg
                                set_mesg(tr_msg(&cmd_ptr[2],id));
                                break;
                        case 103:
                        case 104:
                        case 105:
                        case 116:
                        case 117:
                                //Box messages!
                                robot_box_display(&cmd_ptr[-1],id,temp);
                                //Move to end of all box mesg cmds.
                                do {
                                        robots[id].cur_prog_line+=robot[robots[id].cur_prog_line]+2;
                                reek:
                                        //At next line- check type
                                        if(!robot[robots[id].cur_prog_line]) goto end_prog;
                                        t1=robot[robots[id].cur_prog_line+1];
                                        if((t1!=47)&&(t1!=103)&&(t1!=104)&&(t1!=105)&&(t1!=106)&&(t1!=116)&&(t1!=117)) break;
                                } while(1);
                                //Labels
                                if(!temp[0]) goto breaker;
                                send_robot_id(id,temp,1);
                                gotoed=1;
                                goto breaker;
                        case 106://label
                                if(last_label==robots[id].cur_prog_line) //Passed here already?
                                        goto breaker;//Yep.
                                last_label=robots[id].cur_prog_line;
                                break;
                        case 107://comment-do nothing!
                                //(unless first char is a @)
                                if(cmd_ptr[2]!='@') break;
                                //Rename robot
                                if(cmd_ptr[1]<3) {
                                        robots[id].robot_name[0]=0;
                                        break;
                                        }
                                tr_msg(&cmd_ptr[3],id);
                                if(str_len(ibuff)>14) ibuff[14]=0;
                                str_cpy(robots[id].robot_name,ibuff);
                                break;
                        //case 108://zapped label-do nothing!
                        case 109://teleport
                                tr_msg(&cmd_ptr[2],id);
                                for(t1=0;t1<NUM_BOARDS;t1++) {
                                        if(board_where[t1]==W_NOWHERE) continue;
                                        if(str_cmp(&board_list[t1*25],ibuff)==0) break;
                                        }
                                if(t1==NUM_BOARDS) break;
                                t3=t1;
                                t4=next_param(cmd_ptr,1);
                                t1=parse_param(&cmd_ptr[t4],id);
                                t2=parse_param(&cmd_ptr[next_param(cmd_ptr,t4)],id);
                                //x/y=t1/t2 brd=t3
                                //Prefix the XY pair
                                t5=board_xsiz;
                                t6=board_ysiz;
                                board_xsiz=board_ysiz=400;
                                prefix_xy(t4,t4,t1,t2,t4,t4,x,y);
                                board_xsiz=t5;
                                board_ysiz=t6;
                                //Set vars
                                target_board=t3;
                                target_x=t1;
                                target_y=t2;
                                target_where=-1;
                                done=1;
                                break;
                        case 110://scrollview dir num
                                t3=parsedir(parse_param(&cmd_ptr[1],id),x,y,robots[id].walk_dir);
                                if((t3<1)||(t3>4)) break;
                                t2=parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
				switch(t3) {
                                        case 1:
                                                scroll_y-=t2;
                                                break;
                                        case 2:
                                                scroll_y+=t2;
                                                break;
                                        case 3:
                                                scroll_x+=t2;
                                                break;
                                        case 4:
                                                scroll_x-=t2;
                                                break;
                                        }
                                break;
                        case 111://input string
                                m_hide();
                                save_screen(current_pg_seg);
                                if(fad) {
                                        clear_screen(1824,current_pg_seg);
                                        insta_fadein();
                                        }
                                draw_window_box(3,11,77,14,current_pg_seg,EC_DEBUG_BOX,
                                        EC_DEBUG_BOX_DARK,EC_DEBUG_BOX_CORNER);
                                write_string(tr_msg(&cmd_ptr[2],id),5,12,EC_DEBUG_LABEL,
                                        current_pg_seg);
                                m_show();
                                input_string[0]=0;
                                t1=curr_table;
                                switch_keyb_table(1);
                                intake(input_string,70,5,13,current_pg_seg,15,1,0);
                                if(fad) insta_fadeout();
                                switch_keyb_table(t1);
                                restore_screen(current_pg_seg);
                                input_size=str_len(input_string);
                                num_input=atoi(input_string);
                                break;
                        case 112://If string "" "l"
                                if(!str_cmp(tr_msg(&cmd_ptr[2],id),input_string)) {
                                        send_robot_id(id,tr_msg(&cmd_ptr[1+next_param(cmd_ptr,1)],
                                                id),1);
                                        gotoed=1;
                                        }
                                break;
                        case 113://If string not "" "l"
                                if(str_cmp(tr_msg(&cmd_ptr[2],id),input_string)) {
                                        send_robot_id(id,tr_msg(&cmd_ptr[1+next_param(cmd_ptr,1)],
                                                id),1);
                                        gotoed=1;
                                        }
                                break;
                        case 114://If string matches "" "l"
                                //compare
                                tr_msg(&cmd_ptr[2],id);
                                for(t1=0;t1<str_len(ibuff);t1++) {
                                        t2=ibuff[t1];
                                        if((t2>='a')&&(t2<='z')) t2-=32;
                                        t3=input_string[t1];
                                        if((t3>='a')&&(t3<='z')) t3-=32;
                                        if(t2=='?') continue;
                                        if(t2=='#') {
                                                if((t3<'0')||(t3>'9')) break;
                                                continue;
                                                }
                                        if(t2=='_') {
                                                if((t3<'A')||(t3>'Z')) break;
                                                continue;
                                                }
                                        if(t2=='*') {
                                                t1=32767;
                                                break;
                                                }
                                        if(t2!=t3) break;
                                        }
                                if(t1<str_len(ibuff)) break;
                                //Matches
                                send_robot_id(id,tr_msg(&cmd_ptr[1+next_param(cmd_ptr,1)],id),1);
                                gotoed=1;
                                break;
                        case 115://Player char
                                t2=(unsigned char)parse_param(&cmd_ptr[1],id);
                                for(t1=0;t1<4;t1++)
                                        player_char[t1]=t2;
                                break;
                        case 118://move all thing dir
                                done=1;
                                t1=parse_param(&cmd_ptr[1],id);//Color
                                t3=parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);//Thing
                                t4=parse_param(&cmd_ptr[next_param(cmd_ptr,next_param(cmd_ptr,1))],id);//Param
                                if(t1&256) {
                                        t1^=256;
                                        if(t1==32) t1=t2=16;
                                        else if(t1<16) t2=16;
                                        else {
                                                t2=t1-16;
                                                t1=16;
                                                }
                                        }
                                else {
                                        t2=t1>>4;
                                        t1=t1&15;
                                        }
                                //t1/t2=fg/bk t3=thing t4=param t5=dir
                                t5=parsedir(parse_param(&cmd_ptr[next_param(cmd_ptr,
                                        next_param(cmd_ptr,next_param(cmd_ptr,1)))],id),x,y,robots[id].walk_dir);
                                if((t5<1)||(t5>4)) break;
                                //if dir is 1 or 4, search top to bottom.
                                //if dir is 2 or 3, search bottom to top.
                                //t6 set to start, t7 is dest., t8 is inc.
                                if((t5==1)||(t5==4)) {
                                        t6=0; t7=max_bxsiz*max_bysiz; t8=1;
                                        }
                                else {
                                        t6=max_bxsiz*max_bysiz-1; t7=-1; t8=-1;
                                        }
                                for(;t6!=t7;t6+=t8) {
                                        if(level_id[t6]!=t3) continue;
                                        t9=level_color[t6];
                                        if(t1<16)
                                                if((t9&15)!=t1) continue;
                                        if(t2<16)
                                                if((t9>>4)!=t2) continue;
                                        if(t4<256)
                                                if(level_param[t6]!=t4) continue;
                                        //Correct thing- now move it! :>
                                        _move(t6%max_bxsiz,t6/max_bxsiz,t5-1,1+2+4+8+16);
                                        }
                                break;
                        case 119://copy x y x y
                                //t1,t2 to t3,t4
                                t9=1;
                                t1=parse_param(&cmd_ptr[t9],id); t9=next_param(cmd_ptr,t9);
                                t2=parse_param(&cmd_ptr[t9],id); t9=next_param(cmd_ptr,t9);
                                t3=parse_param(&cmd_ptr[t9],id); t9=next_param(cmd_ptr,t9);
                                t4=parse_param(&cmd_ptr[t9],id);
                                prefix_xy(t1,t2,t9,t9,t3,t4,x,y);
                                goto copy_XY;
                        case 120://set edge color
                                edge_color=parse_param(&cmd_ptr[1],id);
                                break;
                        case 121://board dir is ""
                        case 122://board dir is none
                                t3=parsedir(parse_param(&cmd_ptr[1],id),x,y,robots[id].walk_dir);
                                if((t3<1)||(t3>4)) break;
                                if(cmd==122) board_dir[t3-1]=NO_BOARD;
                                else {
                                        t2=next_param(cmd_ptr,1)+1;
                                        //Find board by given name
                                        tr_msg(&cmd_ptr[t2],id);
                                        for(t1=0;t1<NUM_BOARDS;t1++) {
                                                if(board_where[t1]==W_NOWHERE) continue;
                                                if(!str_cmp(&board_list[t1*25],ibuff)) break;
                                                }
                                        if(t1<NUM_BOARDS) board_dir[t3-1]=t1;
                                        }
                                break;
                        case 123://char edit
                                t2=next_param(cmd_ptr,1);
                                for(t1=0;t1<14;t1++) {
                                        temp[t1]=parse_param(&cmd_ptr[t2],id);
                                        t2=next_param(cmd_ptr,t2);
                                        }
                                ec_change_char_nou((unsigned char)(parse_param(&cmd_ptr[1],id)),temp);
                                break;
                        case 124://Become push
                        case 125://Become nonpush
                                if(id==NUM_ROBOTS) break;
                                level_id[x+y*max_bxsiz]=cmd-1;
                                break;
                        case 126://blind
                                blind_dur=parse_param(&cmd_ptr[1],id);
                                break;
                        case 127://firewalker
                                firewalker_dur=parse_param(&cmd_ptr[1],id);
                                break;
                        case 128://freezetime
                                freeze_time_dur=parse_param(&cmd_ptr[1],id);
                                break;
                        case 129://slow time
                                slow_time_dur=parse_param(&cmd_ptr[1],id);
                                break;
                        case 130://wind
                                wind_dur=parse_param(&cmd_ptr[1],id);
                                break;
                        case 131://avalanche
                                for(t1=0;t1<10000;t1++) {
                                        if((t2=flags[level_id[t1]])&A_UNDER) {
                                                if(t2&A_ENTRANCE) continue;
                                                if(random_num()%18==0) {
                                                        t2=t1%max_bxsiz;
                                                        t3=t1/max_bxsiz;
                                                        if(t2>=board_xsiz) continue;
                                                        if(t3>=board_ysiz) continue;
                                                        id_place(t2,t3,8,7,0);
                                                        }
                                                }
                                        }
                                break;
                        case 132://copy dir dir
                                if(id==NUM_ROBOTS) break;
                                //t1,t2 to t3,t4
                                t1=t3=x; t2=t4=y;
                                t5=parsedir(parse_param(&cmd_ptr[1],id),x,y,robots[id].walk_dir);
                                if((t5<1)||(t5>4)) break;
                                if(move_dir(t1,t2,t5-1)) break;
                                t5=parsedir(parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id),
                                        x,y,robots[id].walk_dir);
                                if((t5<1)||(t5>4)) break;
                                if(move_dir(t3,t4,t5-1)) break;
                        copy_XY:
                                t5=level_id[t1+t2*max_bxsiz];
                                t6=level_param[t1+t2*max_bxsiz];
                                t7=level_id[t3+t4*max_bxsiz];
                                if(t7==127) break;//No copy over player
                                if(t5==127) break;//No copying player
                                if(t5==122) {
                                        /* Replicate a sensor */
                                        t8=find_sensor();
                                        if(!t8) break;// Out of sensors
                                        copy_sensor(t8,t6);// Copy sensor
                                        t6=t8;// Set param to newest copy
                                        }
                                if((t5==123)||(t5==124)) {
                                        /* Replicate a robot */
                                        t8=find_robot();
                                        if(!t8) break;// Out of robots
                                        copy_robot(t8,t6);// Copy robot
                                        robot=&robot_mem[robots[id].program_location];
                                        cmd_ptr=&robot[robots[id].cur_prog_line+1];
                                        t6=t8;// Set param to newest copy
                                        }
                                if((t5==125)||(t5==126)) {
                                        /* Replicate a scroll */
                                        t8=find_scroll();
                                        if(!t8) break;// Out of scrolls
                                        copy_scroll(t8,t6);// Copy scroll
                                        t6=t8;// Set param to newest copy
                                        }
                                //Remove at location
                                if((t7==123)||(t7==124)) {
                                        //delete robot
                                        clear_robot(level_param[t3+t4*max_bxsiz]);
                                        robot=&robot_mem[robots[id].program_location];
                                        cmd_ptr=&robot[robots[id].cur_prog_line+1];
                                        }
                                if((t7==125)||(t7==126))
                                        //delete scroll
                                        clear_scroll(level_param[t3+t4*max_bxsiz]);
                                if(t7==122)
                                        //delete sensor
                                        clear_sensor(level_param[t3+t4*max_bxsiz]);
                                level_id[t3+t4*max_bxsiz]=t5;
                                level_param[t3+t4*max_bxsiz]=t6;
                                level_color[t3+t4*max_bxsiz]=level_color[t1+t2*max_bxsiz];
                                if((t3==x)&&(t4==y)) {
                                        exit_func();
                                        return;
                                        }
                                //Figure blocked vars
                                update_blocked=1;
                                break;
                        case 133://become lavawalker
                        case 134://become non lavawalker
                                robots[id].can_lavawalk=134-cmd;
                                break;
                        case 135://change color thing param color thing param
                                t9=1;
                                t1=parse_param(&cmd_ptr[t9],id); t9=next_param(cmd_ptr,t9);
                                t3=parse_param(&cmd_ptr[t9],id); t9=next_param(cmd_ptr,t9);
                                t4=parse_param(&cmd_ptr[t9],id); t9=next_param(cmd_ptr,t9);
                                t5=parse_param(&cmd_ptr[t9],id); t9=next_param(cmd_ptr,t9);
                                t7=parse_param(&cmd_ptr[t9],id); t9=next_param(cmd_ptr,t9);
                                t8=parse_param(&cmd_ptr[t9],id);
                                //t1/t3/t4 to t5/t7/t8
                                //Change fgbk code to seperate fg/bk codes
                                if(t1&256) {
                                        t1^=256;
                                        if(t1==32) t1=t2=16;
                                        else if(t1<16) t2=16;
                                        else {
                                                t2=t1-16;
                                                t1=16;
                                                }
                                        }
                                else {
                                        t2=t1>>4;
                                        t1=t1&15;
                                        }
                                if(t5&256) {
                                        t5^=256;
                                        if(t5==32) t5=t6=16;
                                        else if(t5<16) t6=16;
                                        else {
                                                t6=t5-16;
                                                t5=16;
                                                }
                                        }
                                else {
                                        t6=t5>>4;
                                        t5=t5&15;
                                        }
                                //t1/t2 = fg/bk orig
                                //t3    = thing orig
                                //t4    = param orig
                                //t5/t6 = fg/bk new
                                //t7    = thing new
                                //t8    = param new
                                //Check for changing between incompatible objects
                                if((t7==122)&&(t3!=122)) break;
                                if(((t7==123)||(t7==124))&&(t3!=123)&&(t3!=124)) break;
                                if(((t7==125)||(t7==126))&&(t3!=125)&&(t3!=126)) break;
                                if((t3==127)||(t7==127)) break;
                                //Changing to Lava or Fire or Ice or Energizer or CW/CCW
                                //or Life
                                if((t7==25)||(t7==26)||(t7==33)||(t7==45)||
                                        (t7==46)||(t7==63)||(t7==66)) t8=0;
                                //Changing to explosion
                                if(t7==38) t8&=0xF3;//Ignore stages above 3
                                //Search for id t3 of color fg=t1 bg=t2. Param==t4
                                for(t9=0;t9<10000;t9++) {
                                        if(t3==41)//Door finds open door
                                                if(level_id[t9]==42) goto cid_okay;
                                        if(t3==67)//Whirlpool finds any of them
                                                if((level_id[t9]>66)&&(level_id[t9]<71)) goto cid_okay;
                                        if(level_id[t9]!=t3) continue;
                                cid_okay:
                                        t0=level_color[t9];
                                        if(t1<16)
                                                if((t0&15)!=t1) continue;
                                        if(t2<16)
                                                if((t0>>4)!=t2) continue;
                                        if(t4<256)
                                                if(level_param[t9]!=t4) continue;
                                        if(t5<16) t0=(t0&240)|t5;
                                        if(t6<16) t0=(t0&15)|(t6<<4);
                                        level_color[t9]=t0;
                                        level_id[t9]=t7;
                                        if(t7==t3) goto skip_del;
                                        if((t7==124)&&(t3==123)) goto skip_del;
                                        if((t7==123)&&(t3==124)) goto skip_del;
                                        if((t7==126)&&(t3==125)) goto skip_del;
                                        if((t7==125)&&(t3==126)) goto skip_del;
                                        if((t3==123)||(t3==124)) {
                                                //delete robot
                                                clear_robot(level_param[t9]);
                                                robot=&robot_mem[robots[id].program_location];
                                                cmd_ptr=&robot[robots[id].cur_prog_line+1];
                                                }
                                        if((t3==125)||(t3==126))
                                                //delete scroll
                                                clear_scroll(level_param[t9]);
                                        if(t3==122)
                                                //delete sensor
                                                clear_sensor(level_param[t9]);
                                skip_del:
                                        if(t7==0) {
                                                offs_remove_id(t9,0);
                                                //If this LEAVES a space, use given color
                                                if(level_id[t9]==0) level_color[t9]=t0;
                                                }
                                        else {
                                                if(t8<256)
                                                        if(t7<122)
                                                                level_param[t9]=t8;
                                                }
                                        }
                                //Are we still "alive"?
                                if(id!=GLOBAL_ROBOT) {
                                        t1=level_id[x+y*max_bxsiz];
                                        if((t1!=123)&&(t1!=124)) {
                                                exit_func();
                                                return;//Nope.
                                                }
                                        }
                                //Figure blocked vars
                                update_blocked=1;
                                break;
                        case 136:// player color
                                t1=parse_param(&cmd_ptr[1],id);
                                if(get_counter("INVINCO",0))
                                        saved_pl_color=fix_color(t1,saved_pl_color);
                                else player_color=fix_color(t1,player_color);
                                break;
                        case 137:// bullet color
                                for(t1=0;t1<3;t1++)
                                        bullet_color[t1]=fix_color(parse_param(&cmd_ptr[1],id),
                                                bullet_color[t1]);
                                break;
                        case 138:// missile color
                                missile_color=fix_color(parse_param(&cmd_ptr[1],id),missile_color);
                                break;
                        case 139://message row
                                b_mesg_row=parse_param(&cmd_ptr[1],id);
                                if(b_mesg_row>24) b_mesg_row=24;
                                break;
                        case 140://REL SELF
                                if(id==NUM_ROBOTS) break;
                        case 141://REL PLAYER
                        case 142://REL COUNTERS
                                first_prefix=mid_prefix=last_prefix=cmd-139;
                                lines_run--;
                                goto next_cmd_prefix;
                        case 143://set id char # to 'c'
                                t1=parse_param(&cmd_ptr[1],id);
                                if((t1>=0)&&(t1<=454))
                                        id_chars[t1]=parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
                                break;
                        case 144://jump mod order #
                                jump_mod(parse_param(&cmd_ptr[1],id));
                                break;
                        case 145://ask yes/no
                                if(fad) {
                                        clear_screen(1824,current_pg_seg);
                                        insta_fadein();
                                        }
                                if(!ask_yes_no(tr_msg(&cmd_ptr[2],id))) send_robot_id(id,"YES",1);
                                else send_robot_id(id,"NO",1);
                                gotoed=1;
                                if(fad) insta_fadeout();
                                break;
                        case 146://fill health
                                set_counter("HEALTH",health_limit);
                                break;
                        case 147://thick arrow dir char
                                t1=parsedir(parse_param(&cmd_ptr[1],id),x,y,robots[id].walk_dir);
                                if((t1<1)||(t1>4)) break;
                                id_chars[249+t1]=parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
                                break;
                        case 148://thin arrow dir char
                                t1=parsedir(parse_param(&cmd_ptr[1],id),x,y,robots[id].walk_dir);
                                if((t1<1)||(t1>4)) break;
                                id_chars[253+t1]=parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
                                break;
                        case 149://set max health
                                health_limit=parse_param(&cmd_ptr[1],id);
                                set_counter("HEALTH",get_counter("HEALTH"));
                                break;
                        case 150://save pos
                                t0=0;
                        save_pos:
                                pl_saved_x[t0]=player_x;
                                pl_saved_y[t0]=player_y;
                                pl_saved_board[t0]=curr_board;
                                break;
                        case 151://restore
                                t0=0;
                        restore_pos:
                                t1=pl_saved_x[t0];
                                t2=pl_saved_y[t0];
                                t3=pl_saved_board[t0];
                        exch_pos_entry:
                                //Teleport
                                target_board=t3;
                                target_x=t1;
                                target_y=t2;
                                target_where=0;
                                done=1;
                                break;
                        case 152://exchange
                                t0=0;
                        exchange_pos:
                                t1=pl_saved_x[t0];
                                t2=pl_saved_y[t0];
                                t3=pl_saved_board[t0];
                                pl_saved_x[t0]=player_x;
                                pl_saved_y[t0]=player_y;
                                pl_saved_board[t0]=curr_board;
                                goto exch_pos_entry;
                        case 153://mesg col
                                b_mesg_col=parse_param(&cmd_ptr[1],id);
                                if(b_mesg_col>79) b_mesg_col=79;
                                break;
                        case 154://center mesg
                                b_mesg_col=255;
                                break;
                        case 155://clear mesg
                                b_mesg_timer=0;
                                break;
                        case 156://resetview
                                scroll_x=scroll_y=0;
                                break;
                        case 157://modsam freq num
                                t1=parse_param(&cmd_ptr[1],id);
                                if(music_on) spot_sample(t1,
                                        parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id));
                                break;
                        case 159://scrollbase
                                scroll_base_color=parse_param(&cmd_ptr[1],id);
                                break;
                        case 160://scrollcorner
                                scroll_corner_color=parse_param(&cmd_ptr[1],id);
                                break;
                        case 161://scrolltitle
                                scroll_title_color=parse_param(&cmd_ptr[1],id);
                                break;
                        case 162://scrollpointer
                                scroll_pointer_color=parse_param(&cmd_ptr[1],id);
                                break;
                        case 163://scrollarrow
                                scroll_arrow_color=parse_param(&cmd_ptr[1],id);
                                break;
                        case 164://viewport x y
                                viewport_x=parse_param(&cmd_ptr[1],id);
                                viewport_y=parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
                                if((viewport_x+viewport_xsiz)>80) viewport_x=80-viewport_xsiz;
                                if((viewport_y+viewport_ysiz)>25) viewport_y=25-viewport_ysiz;
                                break;
                        case 165://viewport xsiz ysiz
                                viewport_xsiz=parse_param(&cmd_ptr[1],id);
                                viewport_ysiz=parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
                                if(viewport_xsiz<1) viewport_xsiz=1;
                                if(viewport_ysiz<1) viewport_ysiz=1;
                                if(viewport_xsiz>80) viewport_xsiz=80;
                                if(viewport_ysiz>25) viewport_ysiz=25;
                                if((viewport_x+viewport_xsiz)>80) viewport_x=80-viewport_xsiz;
                                if((viewport_y+viewport_ysiz)>25) viewport_y=25-viewport_ysiz;
                                break;
                        case 168://save pos #
                                t0=parse_param(&cmd_ptr[1],id)-1;
                                if((t0<0)||(t0>7)) t0=0;
                                goto save_pos;
                        case 169://restore pos #
                                t0=parse_param(&cmd_ptr[1],id)-1;
                                if((t0<0)||(t0>7)) t0=0;
                                goto restore_pos;
                        case 170://exchange pos #
                                t0=parse_param(&cmd_ptr[1],id)-1;
                                if((t0<0)||(t0>7)) t0=0;
                                goto exchange_pos;
                        case 171://restore pos # duplicate self
                                t0=parse_param(&cmd_ptr[1],id)-1;
                                if((t0<0)||(t0>7)) t0=0;
                                t1=pl_saved_x[t0];
                                t2=pl_saved_y[t0];
                                t3=pl_saved_board[t0];
                        exch_pos_entry2:
                                //Teleport
                                target_board=t3;
                                target_x=t1;
                                target_y=t2;
                                target_where=0;
                                t3=find_robot();
                                if(t3) {
                                        copy_robot(t3,id);
                                        if(robots[t3].cur_prog_line>0)
                                                robots[t3].cur_prog_line=1;
                                        robots[t3].pos_within_line=0;
                                        if(id!=GLOBAL_ROBOT) {
                                                target_d_id=level_id[x+y*max_bxsiz];
                                                target_d_color=level_color[x+y*max_bxsiz];
                                                }
                                        else {
                                                target_d_id=124;
                                                target_d_color=7;
                                                }
                                        find_player();
                                        id_place(player_x,player_y,target_d_id,
                                                target_d_color,t3);
                                        //Find a space location for the player
                                        for(player_y=0;player_y<board_xsiz;player_y++) {
                                                for(player_x=0;player_x<board_xsiz;player_x++) {
                                                        if(A_UNDER&flags[level_id[player_x+player_y*max_bxsiz]])
                                                                goto sailormoon;
                                                        }
                                                }
                                sailormoon:
                                        if((player_x<board_xsiz)&&(player_y<board_ysiz))
                                                id_place(player_x,player_y,127,0,0);
                                        else player_x=player_y=0;
                                        }
                                done=1;
                                break;
                        case 172://exchange pos # duplicate self
                                t0=parse_param(&cmd_ptr[1],id)-1;
                                if((t0<0)||(t0>7)) t0=0;
                                t1=pl_saved_x[t0];
                                t2=pl_saved_y[t0];
                                t3=pl_saved_board[t0];
                                pl_saved_x[t0]=player_x;
                                pl_saved_y[t0]=player_y;
                                pl_saved_board[t0]=curr_board;
                                goto exch_pos_entry2;
                        case 173://Pl bulletn
                        case 174://Pl bullets
                        case 175://Pl bullete
                        case 176://Pl bulletw
                        case 177://Nu bulletn
                        case 178://Nu bullets
                        case 179://Nu bullete
                        case 180://Nu bulletw
                        case 181://En bulletn
                        case 182://En bullets
                        case 183://En bullete
                        case 184://En bulletw
                                bullet_char[cmd-173]=parse_param(&cmd_ptr[1],id);
                                break;
                        case 185://Pl bcolor
                        case 186://Nu bcolor
                        case 187://En bcolor
                                bullet_color[cmd-185]=parse_param(&cmd_ptr[1],id);
                                break;
                        case 193://Rel self first
                                if(id==NUM_ROBOTS) break;
                        case 195://Rel player first
                        case 197://Rel counters first
                                first_prefix=((cmd-193)>>1)+5;
                                lines_run--;
                                goto next_cmd_prefix;
                        case 194://Rel self last
                                if(id==NUM_ROBOTS) break;
                        case 196://Rel player last
                        case 198://Rel counters last
                                last_prefix=((cmd-194)>>1)+5;
                                lines_run--;
                                goto next_cmd_prefix;
                        case 199://Mod fade out
                                volume_inc=-8;
                                volume_target=0;
                                break;
                        case 200://Mod fade in
                                volume_inc=8;
                                volume_target=255;
                                magic_load_mod(tr_msg(&cmd_ptr[2],id));
                                volume_mod(volume=0);
                                break;
                        case 201://Copy block sx sy xsiz ysiz dx dy
                        case 243://Copy overlay block sx sy xs ys dx dy
                                t9=1;
                                t1=parse_param(&cmd_ptr[t9],id); t9=next_param(cmd_ptr,t9);
                                t2=parse_param(&cmd_ptr[t9],id); t9=next_param(cmd_ptr,t9);
                                t3=parse_param(&cmd_ptr[t9],id); t9=next_param(cmd_ptr,t9);
                                t4=parse_param(&cmd_ptr[t9],id); t9=next_param(cmd_ptr,t9);
                                t5=parse_param(&cmd_ptr[t9],id); t9=next_param(cmd_ptr,t9);
                                t6=parse_param(&cmd_ptr[t9],id);
                                //Prefixes.
                                prefix_xy(t1,t2,t0,t0,t5,t6,x,y);
                                //Clip and verify parameters.
                                if(t1<0) t1=0;
                                if(t2<0) t2=0;
                                if(t5<0) t5=0;
                                if(t6<0) t6=0;
                                if(t1>=board_xsiz) t1=board_xsiz-1;
                                if(t2>=board_ysiz) t2=board_ysiz-1;
                                if(t5>=board_xsiz) t5=board_xsiz-1;
                                if(t6>=board_ysiz) t6=board_ysiz-1;
                                if(t3<1) t3=1;
                                if(t4<1) t4=1;
                                if((t1+t3)>board_xsiz) t3=board_xsiz-t1;
                                if((t5+t3)>board_xsiz) t3=board_xsiz-t5;
                                if((t2+t4)>board_ysiz) t4=board_ysiz-t2;
                                if((t6+t4)>board_ysiz) t4=board_ysiz-t6;
                                //Copy.
                                if((t1==t5)&&(t2==t6)) break;//None to do!
                                if((t5<t1)||((t5==t1)&&(t6<t2))) {
                                        //Copy starting at UL, going by columns.
                                        for(t7=t1;t7<(t1+t3);t7++) {
                                                for(t8=t2;t8<(t2+t4);t8++) {
                                                        t9=t7+t8*max_bxsiz;
                                                        t0=((t7-t1)+t5)+((t8-t2)+t6)*max_bxsiz;
                                                        //Copy from t9 to t0
                                                        if(cmd==243) {
                                                                overlay[t0]=overlay[t9];
                                                                overlay_color[t0]=overlay_color[t9];
                                                                continue;
                                                                }
                                                        //Clear anything there...
                                                        cmd=level_id[t0];
                                                        if(cmd==127) continue;//No copy over player
                                                        if(cmd==122)
                                                                clear_sensor(level_param[t0]);
                                                        else if((cmd==123)||(cmd==124)) {
                                                                clear_robot(level_param[t0]);
                                                                robot=&robot_mem[robots[id].program_location];
                                                                cmd_ptr=&robot[robots[id].cur_prog_line+1];
                                                                }
                                                        else if((cmd==125)||(cmd==126))
                                                                clear_scroll(level_param[t0]);
                                                        //Copy any robot/scroll at t9
                                                        cmd=level_id[t9];
                                                        t10=level_param[t9];
                                                        if(cmd==122) {//(sensor)
                                                                //...make a copy...
                                                                if(!(t11=find_sensor())) continue;
                                                                copy_sensor(t11,t10);
                                                                t10=t11;
                                                                }
                                                        if((cmd==123)||(cmd==124)) {//(robot)
                                                                //...make a copy...
                                                                if(!(t11=find_robot())) continue;
                                                                if(copy_robot(t11,t10)) {
                                                                        robots[t11].used=0;
                                                                        continue;
                                                                        }
                                                                robot=&robot_mem[robots[id].program_location];
                                                                cmd_ptr=&robot[robots[id].cur_prog_line+1];
                                                                //...and deallocate original if it was number 0.
                                                                t10=t11;
                                                                }
                                                        if((cmd==125)||(cmd==126)) {//(scroll)
                                                                //...make a copy...
                                                                if(!(t11=find_scroll())) continue;
                                                                if(copy_scroll(t11,t10)) {
                                                                        robots[t11].used=0;
                                                                        continue;
                                                                        }
                                                                t10=t11;
                                                                }
                                                        //Player?
                                                        if(cmd==127) {
                                                                level_id[t0]=level_param[t0]=
                                                                        level_under_id[t0]=level_under_param[t0]=0;
                                                                level_color[t0]=level_under_color[t0]=7;
                                                                }
                                                        else {
                                                                //Place
                                                                level_id[t0]=cmd;
                                                                level_param[t0]=t10;
                                                                level_color[t0]=level_color[t9];
                                                                level_under_id[t0]=level_under_id[t9];
                                                                level_under_color[t0]=level_under_color[t9];
                                                                level_under_param[t0]=level_under_param[t9];
                                                                }
                                                        //Loop. (whew!)
                                                        }
                                                }
                                        //Done (UL -> LR)
                                        }
                                else {
                                        //Copy starting at LR, going left by columns.
                                        for(t7=(t1+t3-1);t7>=t1;t7--) {
                                                for(t8=(t2+t4-1);t8>=t2;t8--) {
                                                        t9=t7+t8*max_bxsiz;
                                                        t0=((t7-t1)+t5)+((t8-t2)+t6)*max_bxsiz;
                                                        //Copy from t9 to t0
                                                        if(cmd==243) {
                                                                overlay[t0]=overlay[t9];
                                                                overlay_color[t0]=overlay_color[t9];
                                                                continue;
                                                                }
                                                        //Clear anything there...
                                                        cmd=level_id[t0];
                                                        if(cmd==127) continue;//No copy over player
                                                        if(cmd==122)
                                                                clear_sensor(level_param[t0]);
                                                        else if((cmd==123)||(cmd==124)) {
                                                                clear_robot(level_param[t0]);
                                                                robot=&robot_mem[robots[id].program_location];
                                                                cmd_ptr=&robot[robots[id].cur_prog_line+1];
                                                                }
                                                        else if((cmd==125)||(cmd==126))
                                                                clear_scroll(level_param[t0]);
                                                        //Copy any robot/scroll at t9
                                                        cmd=level_id[t9];
                                                        t10=level_param[t9];
                                                        if(cmd==122) {//(sensor)
                                                                //...make a copy...
                                                                if(!(t11=find_sensor())) continue;
                                                                copy_sensor(t11,t10);
                                                                t10=t11;
                                                                }
                                                        if((cmd==123)||(cmd==124)) {//(robot)
                                                                //...make a copy...
                                                                if(!(t11=find_robot())) continue;
                                                                if(copy_robot(t11,t10)) {
                                                                        robots[t11].used=0;
                                                                        continue;
                                                                        }
                                                                robot=&robot_mem[robots[id].program_location];
                                                                cmd_ptr=&robot[robots[id].cur_prog_line+1];
                                                                //...and deallocate original if it was number 0.
                                                                t10=t11;
                                                                }
                                                        if((cmd==125)||(cmd==126)) {//(scroll)
                                                                //...make a copy...
                                                                if(!(t11=find_scroll())) continue;
                                                                if(copy_scroll(t11,t10)) {
                                                                        robots[t11].used=0;
                                                                        continue;
                                                                        }
                                                                t10=t11;
                                                                }
                                                        //Player?
                                                        if(cmd==127) {
                                                                level_id[t0]=level_param[t0]=
                                                                        level_under_id[t0]=level_under_param[t0]=0;
                                                                level_color[t0]=level_under_color[t0]=7;
                                                                }
                                                        else {
                                                                //Place
                                                                level_id[t0]=cmd;
                                                                level_param[t0]=t10;
                                                                level_color[t0]=level_color[t9];
                                                                level_under_id[t0]=level_under_id[t9];
                                                                level_under_color[t0]=level_under_color[t9];
                                                                level_under_param[t0]=level_under_param[t9];
                                                                }
                                                        //Loop. (whew!)
                                                        }
                                                }
                                        //Done (UL -> LR)
                                        }
                                update_blocked=(cmd==201);
                                break;
                        case 202://Clip input
                                //Chop up to and through first section of whitespace.
                                //First, until non space or end
                                if(!input_size) break;
                                t1=0;
                                do {
                                        if(input_string[t1]==32) break;
                                } while((++t1)<input_size);
                                if(input_string[t1]==32) {
                                        do {
                                                if(input_string[t1]!=32) break;
                                        } while((++t1)<input_size);
                                        }
                                //Chop UNTIL t1. (t1 points to first No-Chop)
                                str_cpy(input_string,&input_string[t1]);
                                input_size=str_len(input_string);
                                num_input=atoi(input_string);
                                break;
                        case 203://Push dir
                                if(id==NUM_ROBOTS) break;
                                t1=parsedir(parse_param(&cmd_ptr[1],id),x,y,robots[id].walk_dir);
                                if((t1<1)||(t1>4)) break;
                                t2=x; t3=y;
                                if(move_dir(t2,t3,t1-1)) break;
                                t4=flags[t5=level_id[t2+t3*max_bxsiz]];
                                if((t5!=123)&&(t5!=127)) {
                                        if((t3<3)&&(!(t4&A_PUSHNS))) break;
                                        else if((t3>2)&&(!(t4&A_PUSHEW))) break;
                                        }
                                _push(x,y,t1-1,0);
                                update_blocked=1;
                                break;
                        case 204://Scroll char dir
                                t9=(unsigned char)parse_param(&cmd_ptr[1],id);
                                t2=parsedir(parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id),
                                        x,y,robots[id].walk_dir);
                                if((t2<1)||(t2>4)) break;
                                ec_read_char(t9,temp);
                                switch(t2) {
                                        case 1:
                                                t1=temp[0];
                                                for(t2=0;t2<13;t2++) temp[t2]=temp[t2+1];
                                                temp[13]=t1;
                                                break;
                                        case 2:
                                                t1=temp[13];
                                                for(t2=14;t2>0;t2--) temp[t2]=temp[t2-1];
                                                temp[0]=t1;
                                                break;
                                        case 3:
                                                for(t1=0;t1<14;t1++) {
                                                        t2=temp[t1]&1;
                                                        temp[t1]>>=1;
                                                        if(t2) temp[t1]|=128;
                                                        }
                                                break;
                                        case 4:
                                                for(t1=0;t1<14;t1++) {
                                                        t2=temp[t1]&128;
                                                        temp[t1]<<=1;
                                                        if(t2) temp[t1]|=1;
                                                        }
                                        }
                                ec_change_char_nou(t9,temp);
                                break;
                        case 205://Flip char dir
                                t9=(unsigned char)parse_param(&cmd_ptr[1],id);
                                t2=parsedir(parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id),
                                        x,y,robots[id].walk_dir);
                                if((t2<1)||(t2>4)) break;
                                ec_read_char(t9,temp);
                                switch(t2) {
                                        case 1:
                                        case 2:
                                                for(t1=0;t1<7;t1++) {
                                                        t2=temp[t1];
                                                        temp[t1]=temp[13-t1];
                                                        temp[13-t1]=t2;
                                                        }
                                                break;
                                        case 3:
                                        case 4:
                                                for(t1=0;t1<14;t1++) {
                                                        t3=128;
                                                        t4=1;
                                                        for(t2=0;t2<4;t2++) {
                                                                if((temp[t1]&t3)&&(temp[t1]&t4)) ;/* Both on */
                                                                else if((temp[t1]&t3)||(temp[t1]&t4)) temp[t1]^=t3+t4;
                                                                t3>>=1;
                                                                t4<<=1;
                                                                }
                                                        }
                                        }
                                ec_change_char_nou(t9,temp);
                                break;
                        case 206://copy char char
                                t1=(unsigned char)parse_param(&cmd_ptr[1],id);
                                t2=(unsigned char)parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
                                ec_read_char(t1,temp);
                                ec_change_char_nou(t2,temp);
                                break;
                        case 210://change sfx
                                t1=next_param(cmd_ptr,1)+1;
                                if(str_len(&cmd_ptr[t1])>68) cmd_ptr[t1+68]=0;
                                str_cpy(&custom_sfx[parse_param(&cmd_ptr[1],id)*69],&cmd_ptr[t1]);
                                break;
                        case 211://color intensity #%
                                set_palette_intensity(parse_param(&cmd_ptr[1],id));
                                pal_update=1;
                                break;
                        case 212://color intensity # #%
                                set_color_intensity(parse_param(&cmd_ptr[1],id)%16,
                                        parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id));
                                pal_update=1;
                                break;
                        case 213://color fade out
                                vquick_fadeout();
                                break;
                        case 214://color fade in
                                vquick_fadein();
                                break;
                        case 215://set color # r g b
                                t9=1;
                                t1=parse_param(&cmd_ptr[t9],id); t9=next_param(cmd_ptr,t9);
                                t2=parse_param(&cmd_ptr[t9],id); t9=next_param(cmd_ptr,t9);
                                t3=parse_param(&cmd_ptr[t9],id); t9=next_param(cmd_ptr,t9);
                                t4=parse_param(&cmd_ptr[t9],id);
                                t1%=16;
                                set_rgb(t1,t2,t3,t4);
                                pal_update=1;
                                break;
                        case 216://Load char set ""
                                ec_load_set_nou(tr_msg(&cmd_ptr[2],id));
                                break;
                        case 217://multiply str #
                        case 218://divide str #
                        case 219://modulo str #
                                tr_msg(&cmd_ptr[2],id);
                                if(str_len(ibuff)>=COUNTER_NAME_SIZE)
                                        ibuff[COUNTER_NAME_SIZE-1]=0;
                                t1=next_param(cmd_ptr,1);
                                //t1 points to number to use
                                t2=parse_param(&cmd_ptr[1],id);
                                t1=parse_param(&cmd_ptr[t1],id);
                                if(t1==0) t2=0;
                                else if(cmd==217) t2=t2*t1;
                                else if(cmd==218) t2=t2/t1;
                                else t2=t2%t1;
                                set_counter(ibuff,t2,id);
                                last_label=-1;//Allows looping "infinitely"
                                break;
                        case 220://Player char dir 'c'
                                t1=parsedir(parse_param(&cmd_ptr[1],id),x,y,robots[id].walk_dir);
                                t2=parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
                                if((t1<1)||(t1>4)) break;
                                player_char[t1-1]=t2;
                                break;
                        case 222://Load palette
                                fp=fopen(tr_msg(&cmd_ptr[2],id),"rb");
                                if(fp==NULL) break;
                                for(t1=0;t1<16;t1++) {
                                        t2=fgetc(fp);
                                        t3=fgetc(fp);
                                        t4=fgetc(fp);
                                        set_rgb(t1,t2,t3,t4);
                                        }
                                pal_update=1;
                                //Done.
                                fclose(fp);
                                break;
                        case 224://Mod fade #t #s
                                volume_inc=parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
                                volume_target=parse_param(&cmd_ptr[1],id);
                                if(volume_target==volume) {
                                        volume_inc=volume_target=0;
                                        break;
                                        }
                                if(volume_inc==0) volume_inc=1;
                                if((volume<volume_target)&&(volume_inc<0)) volume_inc=-volume_inc;
                                if((volume>volume_target)&&(volume_inc>0)) volume_inc=-volume_inc;
                                break;
                        case 225://Scrollview x y
                                t1=parse_param(&cmd_ptr[1],id);
                                t2=parse_param(&cmd_ptr[next_param(cmd_ptr,1)],id);
                                prefix_xy(t3,t3,t1,t2,t3,t3,x,y);
                                //t1/t2 target to put in upper left corner
                                //offset off of player position
                                scroll_x=scroll_y=0;
                                calculate_xytop(t4,t5);
                                scroll_x=t1-t4;
                                scroll_y=t2-t5;
                                break;
                        case 226://Swap world str
                                str_cpy(temp,tr_msg(&cmd_ptr[2],id));
                                //Clear world and reload
                                clear_world();
                                update_done[0]|=254;
                                //(DON'T clear game params)
                        redo:
                                if(load_world(temp,-1)) {
                                        t1=error("Error swapping to next world",1,23,current_pg_seg,0x2C01);
                                        if(t1==2) goto redo;
                                        }
                                str_cpy(curr_file,temp);
                                select_current(first_board);
                                send_robot_def(0,10);
                                target_where=-2;
                                target_board=first_board;
                                target_x=player_x;
                                target_y=player_y;
                                exit_func();
                                return;
                        case 227://If allignedrobot str str
                                if(id==NUM_ROBOTS) break;
                                tr_msg(&cmd_ptr[2],id);
                                for(t1=0;t1<NUM_ROBOTS;t1++) {
                                        if(!str_cmp(robots[t1].robot_name,ibuff)) {
                                                if(x==robots[t1].xpos) break;
                                                if(y==robots[t1].ypos) break;
                                                }
                                        }
                                if(t1<NUM_ROBOTS) {
                                        send_robot_id(id,tr_msg(&cmd_ptr[1+next_param(cmd_ptr,1)],
                                                id),1);
                                        gotoed=1;
                                        }
                                break;
                        case 229://Lockscroll
                                //The scrolling is locked at the current position.
                                //Further scrollview cmds can change it.
                                t1=scroll_x;
                                t2=scroll_y;
                                scroll_x=scroll_y=0;
                                calculate_xytop(t3,t4);
                                locked_x=t3;
                                locked_y=t4;
                                scroll_x=t1;
                                scroll_y=t2;
                                break;
                        case 230://Unlockscroll
                                locked_x=locked_y=65535;
                                break;
                        case 231://If first string str str
                                t1=-1;
                                if(!input_size) break;
                                t1=0;
                                do {
                                        if(input_string[t1]==32) break;
                                } while((++t1)<input_size);
                                if(input_string[t1]==32) input_string[t1]=0;
                                else t1=-1;
                                //Compare
                                if(!str_cmp(input_string,tr_msg(&cmd_ptr[2],id))) {
                                        send_robot_id(id,tr_msg(&cmd_ptr[1+next_param(cmd_ptr,1)],
                                                id),1);
                                        gotoed=1;
                                        }
                                if(t1>=0) input_string[t1]=32;
                                break;
                        case 233://Wait mod fade
                                if(volume!=volume_target) goto breaker;
                                break;
                        case 235://Enable saving
                        case 236://Disable saving
                        case 237://Enable sensoronly saving
                                save_mode=cmd-235;
                                break;
                        case 238://Status counter ## str
                                t1=parse_param(&cmd_ptr[1],id);
                                t2=1+next_param(cmd_ptr,1);
                                if((t1<1)||(t1>6)) break;
                                tr_msg(&cmd_ptr[t2],id);
                                if(str_len(ibuff)>=COUNTER_NAME_SIZE)
                                        ibuff[COUNTER_NAME_SIZE-1]=0;
                                str_cpy(&status_shown_counters[COUNTER_NAME_SIZE*(t1-1)],ibuff);
                                break;
                        case 239://Overlay on
                        case 240://Overlay static
                        case 241://Overlay transparent
                                overlay_mode=cmd-238;
                                break;
                        case 242://put col ch overlay x y
                                if(!overlay_mode) overlay_mode=3;
                                t0=1;
                                t1=parse_param(&cmd_ptr[t0],id); t0=next_param(cmd_ptr,t0);
                                t2=parse_param(&cmd_ptr[t0],id); t0=next_param(cmd_ptr,t0);
                                t3=parse_param(&cmd_ptr[t0],id); t0=next_param(cmd_ptr,t0);
                                t4=parse_param(&cmd_ptr[t0],id);
                                prefix_xy(t9,t9,t3,t4,t9,t9,x,y);
                                //t1=color t2=char t3/t4=pos
                                t1=fix_color(t1,overlay_color[t3+t4*max_bxsiz]);
                                overlay[t3+t4*max_bxsiz]=t2;
                                overlay_color[t3+t4*max_bxsiz]=t1;
                                break;
                        case 245://Change overlay col ch col ch
                        case 246://Change overlay col col
                                if(!overlay_mode) overlay_mode=3;
                                t0=1;
                                t1=parse_param(&cmd_ptr[t0],id); t0=next_param(cmd_ptr,t0);
                                if(cmd==245) {
                                        t3=parse_param(&cmd_ptr[t0],id);
                                        t0=next_param(cmd_ptr,t0);
                                        }
                                t5=parse_param(&cmd_ptr[t0],id);
                                if(cmd==245) {
                                        t0=next_param(cmd_ptr,t0);
                                        t7=parse_param(&cmd_ptr[t0],id);
                                        }
                                //Change fgbk code to seperate fg/bk codes
                                if(t1&256) {
                                        t1^=256;
                                        if(t1==32) t1=t2=16;
                                        else if(t1<16) t2=16;
                                        else {
                                                t2=t1-16;
                                                t1=16;
                                                }
                                        }
                                else {
                                        t2=t1>>4;
                                        t1=t1&15;
                                        }
                                //col t1/t2 ch t3 to col t5 ch t7
                                for(t9=0;t9<10000;t9++) {
                                        if(cmd==245)
                                                if(overlay[t9]!=t3) continue;
                                        t0=overlay_color[t9];
                                        if(t1<16)
                                                if((t0&15)!=t1) continue;
                                        if(t2<16)
                                                if((t0>>4)!=t2) continue;
                                        t0=fix_color(t5,t0);
                                        overlay_color[t9]=t0;
                                        if(cmd==245) overlay[t9]=t7;
                                        }
                                break;
                        case 247://Write overlay col str # #
                                if(!overlay_mode) overlay_mode=3;
                                t0=1;
                                t1=parse_param(&cmd_ptr[t0],id); t0=next_param(cmd_ptr,t0);
                                t2=t0+1; t0=next_param(cmd_ptr,t0);
                                t3=parse_param(&cmd_ptr[t0],id); t0=next_param(cmd_ptr,t0);
                                t4=parse_param(&cmd_ptr[t0],id);
                                prefix_xy(t9,t9,t3,t4,t9,t9,x,y);
                                //Write str cmd_ptr[t2] at t3/t4 in color t1
                                t5=str_len(tr_msg(&cmd_ptr[t2],id));
                                for(t6=0;t6<t5;t6++) {
                                        t7=t3+t4*max_bxsiz;
                                        overlay_color[t7]=t1;
                                        overlay[t7]=ibuff[t6];
                                        if((++t3)>=board_xsiz) break;
                                        }
                                break;
                        case 251://Loop start
                                robots[id].loop_count=0;
                                break;
                        case 252://Loop #
                                t1=parse_param(&cmd_ptr[1],id);
                                if((robots[id].loop_count++)>=t1) break;
                                //Search backwards for a cmd 251 or start of program
                                t2=robots[id].cur_prog_line;
                                do {
                                        if(robot[t2-1]==0xFF) break;
                                        t2-=robot[t2-1]+2;
                                } while(robot[t2+1]!=251);
                                robots[id].cur_prog_line=t2;
                                last_label=-1;//Allows looping "infinitely" even w/label inside
                                break;
                        case 253://Abort loop
                                //Search forwards for a cmd 252 or start of program
                                t2=robots[id].cur_prog_line;
                                do {
                                        if(robot[t2]==0) break;
                                        t2+=robot[t2]+2;
                                } while(robot[t2+1]!=252);
                                robots[id].cur_prog_line=t2;
                                if(!robot[t2]) goto end_prog;
                                break;
                        case 254://Disable mesg edge
                        case 255://Enable mesg edge
                                mesg_edges=cmd-254;
                                break;
                        }
                //Go to next command! First erase prefixes...
        next_cmd:
                first_prefix=mid_prefix=last_prefix=0;
        next_cmd_prefix:
                //Next line
                robots[id].pos_within_line=first_cmd=0;
                if(old_pos==robots[id].cur_prog_line) gotoed=0;//No move=didn't goto
                if(!gotoed) robots[id].cur_prog_line+=robot[robots[id].cur_prog_line]+2;
                if(!robot[robots[id].cur_prog_line]) {
                        //End of program
                end_prog:
                        robots[id].cur_prog_line=0;
                        break;
                        }
                if((update_blocked)&&(id<NUM_ROBOTS)) {
                        update_blocked=0;
                        for(t1=0;t1<4;t1++) {
                                _bl[t1]=0;
                                t4=x; t5=y;
                                if(!move_dir(t4,t5,t1)) {
                                        //Not edge... blocked?
                                        if((flags[level_id[t4+t5*max_bxsiz]]&A_UNDER)!=A_UNDER) _bl[t1]=1;
                                        }
                                else _bl[t1]=1;//Edge is considered blocked
                                }
                        //Update player position too
                        if(level_id[player_x+player_y*max_bxsiz]!=127) find_player();
                        }
        } while(((++lines_run)<40)&&(!done));
breaker:
        robots[id].cycle_count=0;//In case a label changed it
        //Reset x/y (from movements)
        robots[id].xpos=x;
        robots[id].ypos=y;
        exit_func();
}
