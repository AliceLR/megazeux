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

/* PARAM.CPP- Millions of dialog boxes for inputting parameters of
				  a specified id. If a parameter is already available,
				  it is edited. Special functions are called in other
				  files for robots, scrolls, and signs. Uses current_pg_seg
				  for page segment.*/

#include "helpsys.h"
#include "mouse.h"
#include "graphics.h"
#include "intake.h"
#include "scrdisp.h"
#include "error.h"
#include "string.h"
#include "param.h"
#include "window.h"
#include "roballoc.h"
#include "data.h"
#include <_null.h>
#include "edit.h"
#include "robo_ed.h"

//Default parameters (-2=0 and no edit, -3=character, -4=board)
int def_params[128]={
	-2,-2,-2,-2,-2,-3,-2,-3,-2,-2,-3,-2,-3,-2,-2,-2,//0-15
	-2,-3,-2,-2,-2,-2,-2,-2,-2,-2,-2,0,-2,-2,10,0,//16-31
	0,-2,-2,5,0,128,32,-2,-2,0,-2,-4,-4,-2,-2,0,//32-47
	-2,0,-2,-3,-3,-3,-3,17,0,-2,-2,-2,0,4,0,-2,//48-63
	-2,-2,-2,-4,-4,-4,-4,-2,0,-2,48,0,-3,-3,0,127,//64-79
	32,26,2,1,0,2,65,2,66,2,129,34,66,66,12,10,//80-95
	-2,194,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,//96-111
	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,0,0,0,0,0,-2 };//112-127

//Returns parameter or -1 for ESC. Must pass a legit parameter, or -1 to
//use default or load up a new robot/scroll/sign.
int edit_param(unsigned char id,int param) {
	int t1;
	if(def_params[id]==-2) {//No editing allowed
		//If has a param, return it, otherwise return 0
		if(param>-1) return param;
		return 0;
		}
	if(def_params[id]==-3) {//Character
		if(param<0) param=177;
		return char_selection(param,current_pg_seg);
		}
	if(def_params[id]==-4) {//Board
		if(param<0) param=0;
		return choose_board(param,"Choose destination board",current_pg_seg);
		}
	if(param<0) param=def_params[id];
	//Switch according to thing
	switch(id) {
		case 27://Chest
			param=pe_chest(param);
			break;
		case 30://Health
		case 35://Ammo
			param=pe_health(param);
			break;
		case 31://Ring
		case 32://Potion
			param=pe_ring(param);
			break;
		case 36://Bomb
			param=pe_bomb(param);
			break;
		case 37://Lit bomb
			param=pe_lit_bomb(param);
			break;
		case 38://Explosion
			param=pe_explosion(param);
			break;
		case 41://Door
			param=pe_door(param);
			break;
		case 47://Gate
			param=pe_gate(param);
			break;
		case 49://Transport
			param=pe_transport(param);
			break;
		case 55://Pouch
			param=pe_pouch(param);
			break;
		case 56://Pusher
		case 62://Missile
		case 75://Spike
		case 78://Shooting fire
			param=pe_pusher(param);
			if(id==78) param<<=1;
			break;
		case 60://Lazer gun
			param=pe_lazer_gun(param);
			break;
		case 61://Bullet
			param=pe_bullet(param);
			break;
		case 72://Ricochet panel
			param=pe_ricochet_panel(param);
			break;
		case 74://Mine
			param=pe_mine(param);
			break;
		case 80://Snake
			param=pe_snake(param);
			break;
		case 81://Eye
			param=pe_eye(param);
			break;
		case 82://Thief
			param=pe_thief(param);
			break;
		case 83://Slime blob
			param=pe_slime_blob(param);
			break;
		case 84://Runner
			param=pe_runner(param);
			break;
		case 85://Ghost
			param=pe_ghost(param);
			break;
		case 86://Dragon
			param=pe_dragon(param);
			break;
		case 87://Fish
			param=pe_fish(param);
			break;
		case 88://Shark
		case 91://Spitting tiger
			param=pe_shark(param);
			break;
		case 89://Spider
			param=pe_spider(param);
			break;
		case 90://Goblin
			param=pe_goblin(param);
			break;
		case 92://Bullet gun
		case 93://Spinning gun
			param=pe_bullet_gun(param);
			break;
		case 94://Bear
			param=pe_bear(param);
			break;
		case 95://Bear cub
			param=pe_bear_cub(param);
			break;
		case 97://Missile gun
			param=pe_missile_gun(param);
			break;
		case 122://Sensor
			if(param==-1) {
				//Is there one?
				if(!find_sensor(0)) {
					error("All sensors in use",0,24,current_pg_seg,0x1E01);
					return -1;//No avail. sensors
					}
				//Sensor available. Edit #0.
				param=0;
				}
			param=pe_sensor(param);
			break;
		case 125://Sign
		case 126://Scroll
			if(param==-1) {
				if(!find_scroll(0)) {
					error("All scrolls in use",0,24,current_pg_seg,0x2D01);
					return -1;//No avail. scrolls
					}
				//Scroll avail. Edit #0.
				param=0;
				clear_scroll(0);
				}
			scroll_edit(param);//Can't "ESC" quit accidentally.
			break;
		case 123://Robot
		case 124://Robot
			if(param==-1) {
				//Is there one?
				if(!find_robot(0)) {
					error("All robots in use",0,24,current_pg_seg,0x2301);
					return -1;//No avail. robots
					}
				//Robot avail. Edit #0.
				param=0;
				clear_robot(0);
				}
			//First get name...
			m_hide();
			save_screen(current_pg_seg);
			draw_window_box(16,12,50,14,current_pg_seg,EC_DEBUG_BOX,
				EC_DEBUG_BOX_DARK,EC_DEBUG_BOX_CORNER);
			write_string("Name for robot:",18,13,EC_DEBUG_LABEL,
				current_pg_seg);
			m_show();
			if(intake(robots[param].robot_name,14,34,13,current_pg_seg,15,1,
				0)==27) {
					restore_screen(current_pg_seg);
					return -1;
					}
			restore_screen(current_pg_seg);
			//...and character.
			t1=char_selection(robots[param].robot_char,current_pg_seg);
			if(t1<0) {//ESC means keep char but don't edit robot
				if(param==0) return -1;
				if(t1==-256) t1=0;
				robots[param].robot_char=-t1;
				break;
				}
			robots[param].robot_char=t1;
			//Now edit the program.
			set_context(87);
			robot_editor(param);
			pop_context();
			break;
		}
	//Return param
	return param;
}

//Communial dialog
#define MAX_ELEMS 6
char di_types[MAX_ELEMS]={ DE_BUTTON,DE_BUTTON,0,0,0,0 };
char di_xs[MAX_ELEMS]={ 15,37,0,0,0,0 };
char di_ys[MAX_ELEMS]={ 15,15,0,0,0,0 };
char far *di_strs[MAX_ELEMS]={ "OK","Cancel",NULL,NULL,NULL,NULL };
int di_p1s[MAX_ELEMS]={ 0,-1,0,0,0,0 };
int di_p2s[MAX_ELEMS]={ 0,0,0,0,0,0 };
void far *di_storage[MAX_ELEMS]={ NULL,NULL,NULL,NULL,NULL,NULL };
dialog di={
	10,5,69,22,"Choose settings",3,
	di_types,di_xs,di_ys,
	di_strs,di_p1s,di_p2s,di_storage,2 };
//Internal- Reset dialog to above settings
void reset_di(void) {
	for(int t1=2;t1<MAX_ELEMS;t1++) {
		di_types[t1]=di_xs[t1]=di_ys[t1]=di_p1s[t1]=di_p2s[t1]=0;
		di_strs[t1]=NULL;
		di_storage[t1]=NULL;
		}
	di.curr_element=2;
	di.num_elements=3;
}
#define do_dialog() run_dialog(&di,current_pg_seg,0,2)
char far *potion_fx="No Effect   \0\
Invinco     \0\
Blast       \0\
Health x10  \0\
Health x50  \0\
Poison      \0\
Blind       \0\
Kill Enemies\0\
Lava Walker \0\
Detonate    \0\
Banish      \0\
Summon      \0\
Avalanche   \0\
Freeze Time \0\
Wind        \0\
Slow Time";

int pe_chest(int param) {
	int type=param&15;
	int var=param>>4;
	//First, pick chest contents
	type=list_menu("Empty   \0\
Key     \0\
Coins   \0\
Lives   \0\
Ammo    \0\
Gems    \0\
Health  \0\
Potion  \0\
Ring    \0\
Lo Bombs\0\
Hi Bombs",9,"Choose chest contents",type,11,current_pg_seg);
	if(type<0) return -1;
	switch(type) {
		case 0://Empty
			var=0;
			break;
		case 1://Key
			var=color_selection(var,current_pg_seg);
			if(var>0) var=var&15;
			break;
		case 7:
		case 8://Ring/potion
			var=list_menu(potion_fx,13,"Choose effect",var,16,current_pg_seg);
			break;
		default://Number
			reset_di();
			if(type==3) di_types[2]=DE_NUMBER;
			else di_types[2]=DE_FIVE;
			di_xs[2]=5;
			di_ys[2]=6;
			if(type==3) di_strs[2]="Number of lives: ";
			else di_strs[2]="Amount (multiple of five): ";
			di_p1s[2]=0;
			di_p2s[2]=15;
			di_storage[2]=&var;
			if(do_dialog()) var=-1;
			break;
		}
	if(var<0) return -1;
	return (var<<4)+type;
}

int pe_health(int param) {
	reset_di();
	di_types[2]=DE_NUMBER;
	di_xs[2]=15;
	di_ys[2]=6;
	di_strs[2]="Amount: ";
	di_p1s[2]=0;
	di_p2s[2]=255;
	di_storage[2]=&param;
	if(do_dialog()) return -1;
	return param;
}

int pe_ring(int param) {
	return list_menu(potion_fx,13,"Choose effect",param,16,current_pg_seg);
}

int pe_bomb(int param) {
	char rad=param;
	reset_di();
	di_types[2]=DE_RADIO;
	di_xs[2]=15;
	di_ys[2]=6;
	di_strs[2]="Low strength bomb\nHigh strength bomb";
	di_p1s[2]=2;
	di_p2s[2]=18;
	di_storage[2]=&rad;
	if(do_dialog()) return -1;
	return rad;
}

int pe_lit_bomb(int param) {
	char rad=param>>7;
	int stage=7-(param&63);
	reset_di();
	di_types[2]=DE_RADIO;
	di_xs[2]=15;
	di_ys[2]=6;
	di_strs[2]="Low strength bomb\nHigh strength bomb";
	di_p1s[2]=2;
	di_p2s[2]=18;
	di_storage[2]=&rad;
	di_types[3]=DE_NUMBER;
	di_xs[3]=15;
	di_ys[3]=9;
	di_strs[3]="Fuse length: ";
	di_p1s[3]=1;
	di_p2s[3]=7;
	di_storage[3]=&stage;
	di.num_elements=4;
	if(do_dialog()) return -1;
	return (rad<<7)+(7-stage);
}

int pe_explosion(int param) {
	int size=param>>4;
	int stage=(param&15)+1;
	reset_di();
	di_types[2]=DE_NUMBER;
	di_xs[2]=15;
	di_ys[2]=6;
	di_strs[2]="Explosion stage: ";
	di_p1s[2]=1;
	di_p2s[2]=4;
	di_storage[2]=&stage;
	di_types[3]=DE_NUMBER;
	di_xs[3]=15;
	di_ys[3]=8;
	di_strs[3]="Explosion size:  ";
	di_p1s[3]=0;
	di_p2s[3]=15;
	di_storage[3]=&size;
	di.num_elements=4;
	if(do_dialog()) return -1;
	return ((size)<<4)+stage-1;
}

int pe_door(int param) {
	char hv=param&1;
	char opens=(param>>1)&3;
	char locked=param>>3;
	reset_di();
	di_types[2]=DE_RADIO;
	di_xs[2]=15;
	di_ys[2]=3;
	di_strs[2]="Horizontal\nVertical";
	di_p1s[2]=2;
	di_p2s[2]=18;
	di_storage[2]=&hv;
	di_types[3]=DE_RADIO;
	di_xs[3]=15;
	di_ys[3]=6;
	di_strs[3]="Opens N/W\nOpens N/E\nOpens S/W\nOpens S/E";
	di_p1s[3]=4;
	di_p2s[3]=9;
	di_storage[3]=&opens;
	di_types[4]=DE_CHECK;
	di_xs[4]=15;
	di_ys[4]=11;
	di_strs[4]="Locked door";
	di_p1s[4]=1;
	di_p2s[4]=11;
	di_storage[4]=&locked;
	di.num_elements=5;
	if(do_dialog()) return -1;
	return hv+(opens<<1)+(locked<<3);
}

int pe_gate(int param) {
	char locked=param;
	reset_di();
	di_types[2]=DE_CHECK;
	di_xs[2]=15;
	di_ys[2]=7;
	di_strs[2]="Locked gate";
	di_p1s[2]=1;
	di_p2s[2]=11;
	di_storage[2]=&locked;
	if(do_dialog()) return -1;
	return locked;
}

int pe_transport(int param) {
	reset_di();
	di_types[2]=DE_RADIO;
	di_xs[2]=15;
	di_ys[2]=6;
	di_strs[2]="North\nSouth\nEast\nWest\nAll";
	di_p1s[2]=5;
	di_p2s[2]=5;
	di_storage[2]=&param;
	if(do_dialog()) return -1;
	return param;
}

int pe_pouch(int param) {
	int coins=param>>4;
	int gems=param&15;
	reset_di();
	di_types[2]=DE_FIVE;
	di_xs[2]=6;
	di_ys[2]=6;
	di_strs[2]="Number of gems (mult. of 5):  ";
	di_p1s[2]=0;
	di_p2s[2]=15;
	di_storage[2]=&gems;
	di_types[3]=DE_FIVE;
	di_xs[3]=6;
	di_ys[3]=8;
	di_strs[3]="Number of coins (mult. of 5): ";
	di_p1s[3]=0;
	di_p2s[3]=15;
	di_storage[3]=&coins;
	di.num_elements=4;
	if(do_dialog()) return -1;
	return (coins<<4)+gems;
}

int pe_pusher(int param) {
	reset_di();
	di_types[2]=DE_RADIO;
	di_xs[2]=15;
	di_ys[2]=6;
	di_strs[2]="North\nSouth\nEast\nWest";
	di_p1s[2]=4;
	di_p2s[2]=5;
	di_storage[2]=&param;
	if(do_dialog()) return -1;
	return param;
}

int pe_lazer_gun(int param) {
	int dir=param&3;
	int start=((param>>2)&7)+1;
	int end=(param>>5)+1;
	reset_di();
	di_types[2]=DE_RADIO;
	di_xs[2]=15;
	di_ys[2]=4;
	di_strs[2]="North\nSouth\nEast\nWest";
	di_p1s[2]=4;
	di_p2s[2]=5;
	di_storage[2]=&dir;
	di_types[3]=DE_NUMBER;
	di_xs[3]=15;
	di_ys[3]=9;
	di_strs[3]="Start time: ";
	di_p1s[3]=1;
	di_p2s[3]=8;
	di_storage[3]=&start;
	di_types[4]=DE_NUMBER;
	di_xs[4]=15;
	di_ys[4]=11;
	di_strs[4]="Length on:  ";
	di_p1s[4]=1;
	di_p2s[4]=7;
	di_storage[4]=&end;
	di.num_elements=5;
	if(do_dialog()) return -1;
	return dir+((start-1)<<2)+((end-1)<<5);
}

int pe_bullet(int param) {
	int dir=param&3;
	int type=param>>2;
	reset_di();
	di_types[2]=DE_RADIO;
	di_xs[2]=15;
	di_ys[2]=5;
	di_strs[2]="North\nSouth\nEast\nWest";
	di_p1s[2]=4;
	di_p2s[2]=5;
	di_storage[2]=&dir;
	di_types[3]=DE_RADIO;
	di_xs[3]=15;
	di_ys[3]=10;
	di_strs[3]="Player\nNeutral\nEnemy";
	di_p1s[3]=3;
	di_p2s[3]=7;
	di_storage[3]=&type;
	di.num_elements=4;
	if(do_dialog()) return -1;
	return dir+(type<<2);
}

int pe_ricochet_panel(int param) {
	reset_di();
	di_types[2]=DE_RADIO;
	di_xs[2]=15;
	di_ys[2]=7;
	di_strs[2]="Orientation \\\nOrientation /";
	di_p1s[2]=2;
	di_p2s[2]=13;
	di_storage[2]=&param;
	if(do_dialog()) return -1;
	return param;
}

int pe_mine(int param) {
	param>>=4;
	param--;
	reset_di();
	di_types[2]=DE_NUMBER;
	di_xs[2]=15;
	di_ys[2]=7;
	di_strs[2]="Blast radius:  ";
	di_p1s[2]=1;
	di_p2s[2]=8;
	di_storage[2]=&param;
	if(do_dialog()) return -1;
	return (param-1)<<4;
}

int pe_snake(int param) {
	int dir=param&3;
	int intel=(param>>4)+1;
	char speed=((param>>2)&1)^1;
	reset_di();
	di_types[2]=DE_RADIO;
	di_xs[2]=15;
	di_ys[2]=4;
	di_strs[2]="North\nSouth\nEast\nWest";
	di_p1s[2]=4;
	di_p2s[2]=5;
	di_storage[2]=&dir;
	di_types[3]=DE_NUMBER;
	di_xs[3]=15;
	di_ys[3]=9;
	di_strs[3]="Intellegence: ";
	di_p1s[3]=1;
	di_p2s[3]=8;
	di_storage[3]=&intel;
	di_types[4]=DE_CHECK;
	di_xs[4]=15;
	di_ys[4]=11;
	di_strs[4]="Fast movement";
	di_p1s[4]=1;
	di_p2s[4]=13;
	di_storage[4]=&speed;
	di.num_elements=5;
	if(do_dialog()) return -1;
	return dir+((speed^1)<<2)+((intel-1)<<4);
}

int pe_eye(int param) {
	int intel=(param&7)+1;
	int radius=((param>>3)&7)+1;
	char speed=(param>>6)^1;
	reset_di();
	di_types[2]=DE_NUMBER;
	di_xs[2]=15;
	di_ys[2]=6;
	di_strs[2]="Intellegence: ";
	di_p1s[2]=1;
	di_p2s[2]=8;
	di_storage[2]=&intel;
	di_types[3]=DE_NUMBER;
	di_xs[3]=15;
	di_ys[3]=8;
	di_strs[3]="Blast radius: ";
	di_p1s[3]=1;
	di_p2s[3]=8;
	di_storage[3]=&radius;
	di_types[4]=DE_CHECK;
	di_xs[4]=15;
	di_ys[4]=10;
	di_strs[4]="Fast movement";
	di_p1s[4]=1;
	di_p2s[4]=13;
	di_storage[4]=&speed;
	di.num_elements=5;
	if(do_dialog()) return -1;
	return intel-1+((radius-1)<<3)+((speed^1)<<6);
}

int pe_thief(int param) {
	int intel=(param&7)+1;
	int speed=((param>>3)&3)+1;
	int gems=(param>>7)+1;
	reset_di();
	di_types[2]=DE_NUMBER;
	di_xs[2]=15;
	di_ys[2]=6;
	di_strs[2]="Intellegence:   ";
	di_p1s[2]=1;
	di_p2s[2]=8;
	di_storage[2]=&intel;
	di_types[3]=DE_NUMBER;
	di_xs[3]=15;
	di_ys[3]=8;
	di_strs[3]="Movement speed: ";
	di_p1s[3]=1;
	di_p2s[3]=4;
	di_storage[3]=&speed;
	di_types[4]=DE_NUMBER;
	di_xs[4]=15;
	di_ys[4]=10;
	di_strs[4]="Gems stolen:    ";
	di_p1s[4]=1;
	di_p2s[4]=2;
	di_storage[4]=&gems;
	di.num_elements=5;
	if(do_dialog()) return -1;
	return intel-1+((speed-1)<<3)+((gems-1)<<7);
}

int pe_slime_blob(int param) {
	int speed=(param&7)+1;
	char chk[2]={ (param>>6)&1,(param>>7)&1 };
	reset_di();
	di_types[2]=DE_NUMBER;
	di_xs[2]=15;
	di_ys[2]=6;
	di_strs[2]="Spread speed: ";
	di_p1s[2]=1;
	di_p2s[2]=8;
	di_storage[2]=&speed;
	di_types[3]=DE_CHECK;
	di_xs[3]=15;
	di_ys[3]=8;
	di_strs[3]="Hurts player\nInvincible";
	di_p1s[3]=2;
	di_p2s[3]=12;
	di_storage[3]=chk;
	di.num_elements=4;
	if(do_dialog()) return -1;
	return speed-1+((chk[0])<<6)+((chk[1])<<7);
}

int pe_runner(int param) {
	int dir=param&3;
	int HP=(param>>6)+1;
	int speed=((param>>2)&3)+1;
	reset_di();
	di_types[2]=DE_RADIO;
	di_xs[2]=15;
	di_ys[2]=4;
	di_strs[2]="North\nSouth\nEast\nWest";
	di_p1s[2]=4;
	di_p2s[2]=5;
	di_storage[2]=&dir;
	di_types[3]=DE_NUMBER;
	di_xs[3]=15;
	di_ys[3]=9;
	di_strs[3]="Hit points:";
	di_p1s[3]=1;
	di_p2s[3]=4;
	di_storage[3]=&HP;
	di_types[4]=DE_NUMBER;
	di_xs[4]=15;
	di_ys[4]=11;
	di_strs[4]="Movement speed: ";
	di_p1s[4]=1;
	di_p2s[4]=4;
	di_storage[4]=&speed;
	di.num_elements=5;
	if(do_dialog()) return -1;
	return dir+((speed-1)<<2)+((HP-1)<<6);
}

int pe_ghost(int param) {
	int intel=(param&7)+1;
	int speed=((param>>4)&3)+1;
	char invinco=((param>>3)&1);
	reset_di();
	di_types[2]=DE_NUMBER;
	di_xs[2]=15;
	di_ys[2]=6;
	di_strs[2]="Intellegence:  ";
	di_p1s[2]=1;
	di_p2s[2]=8;
	di_storage[2]=&intel;
	di_types[3]=DE_NUMBER;
	di_xs[3]=15;
	di_ys[3]=8;
	di_strs[3]="Movement speed: ";
	di_p1s[3]=1;
	di_p2s[3]=4;
	di_storage[3]=&speed;
	di_types[4]=DE_CHECK;
	di_xs[4]=15;
	di_ys[4]=10;
	di_strs[4]="Invincible";
	di_p1s[4]=1;
	di_p2s[4]=10;
	di_storage[4]=&invinco;
	di.num_elements=5;
	if(do_dialog()) return -1;
	return intel-1+((speed-1)<<4)+((invinco)<<3);
}

int pe_dragon(int param) {
	int fire_rate=(param&3)+1;
	int HP=((param>>5)&3)+1;
	char moves=((param>>2)&1);
	reset_di();
	di_types[2]=DE_NUMBER;
	di_xs[2]=15;
	di_ys[2]=6;
	di_strs[2]="Firing rate: ";
	di_p1s[2]=1;
	di_p2s[2]=4;
	di_storage[2]=&fire_rate;
	di_types[3]=DE_NUMBER;
	di_xs[3]=15;
	di_ys[3]=8;
	di_strs[3]="Hit points:  ";
	di_p1s[3]=1;
	di_p2s[3]=8;
	di_storage[3]=&HP;
	di_types[4]=DE_CHECK;
	di_xs[4]=15;
	di_ys[4]=10;
	di_strs[4]="Moves";
	di_p1s[4]=1;
	di_p2s[4]=5;
	di_storage[4]=&moves;
	di.num_elements=5;
	if(do_dialog()) return -1;
	return fire_rate-1+((HP-1)<<5)+((moves)<<2);
}

int pe_fish(int param) {
	int intel=(param&7)+1;
	char chk[4]={ (param>>6)&1,(param>>5)&1,(param>>7)&1,((param>>3)&1)^1 };
	reset_di();
	di_types[2]=DE_NUMBER;
	di_xs[2]=15;
	di_ys[2]=5;
	di_strs[2]="Intellegence: ";
	di_p1s[2]=1;
	di_p2s[2]=8;
	di_storage[2]=&intel;
	di_types[3]=DE_CHECK;
	di_xs[3]=15;
	di_ys[3]=7;
	di_strs[3]="Hurts player\nAffected by current\n2 hit points\nFast movement";
	di_p1s[3]=4;
	di_p2s[3]=19;
	di_storage[3]=chk;
	di.num_elements=4;
	if(do_dialog()) return -1;
	return intel-1+((chk[0])<<6)+((chk[1])<<5)+((chk[2])<<7)+((chk[3]^1)<<3);
}

int pe_shark(int param) {
	int intel=(param&3)+1;
	int fire_rate=((param>>5)&3)+1;
	int fires=((param>>3)&3);
	reset_di();
	di_types[2]=DE_NUMBER;
	di_xs[2]=15;
	di_ys[2]=4;
	di_strs[2]="Intellegence: ";
	di_p1s[2]=1;
	di_p2s[2]=8;
	di_storage[2]=&intel;
	di_types[3]=DE_NUMBER;
	di_xs[3]=15;
	di_ys[3]=6;
	di_strs[3]="Firing rate:  ";
	di_p1s[3]=1;
	di_p2s[3]=8;
	di_storage[3]=&fire_rate;
	di_types[4]=DE_RADIO;
	di_xs[4]=15;
	di_ys[4]=8;
	di_strs[4]="Fires bullets\nFires seekers\nFires fire\nFires nothing";
	di_p1s[4]=4;
	di_p2s[4]=13;
	di_storage[4]=&fires;
	di.num_elements=5;
	if(do_dialog()) return -1;
	return intel-1+((fire_rate-1)<<5)+(fires<<3);
}

int pe_spider(int param) {
	int intel=(param&3)+1;
	int web=((param>>3)&3);
	char chk[2]={ ((param>>5)&1)^1,((param>>7)&1) };
	reset_di();
	di_types[2]=DE_NUMBER;
	di_xs[2]=15;
	di_ys[2]=3;
	di_strs[2]="Intellegence: ";
	di_p1s[2]=1;
	di_p2s[2]=8;
	di_storage[2]=&intel;
	di_types[3]=DE_RADIO;
	di_xs[3]=15;
	di_ys[3]=5;
	di_strs[3]="Thin web only\nThick web only\nAny web\nAnywhere";
	di_p1s[3]=4;
	di_p2s[3]=14;
	di_storage[3]=&web;
	di_types[4]=DE_CHECK;
	di_xs[4]=15;
	di_ys[4]=10;
	di_strs[4]="Fast movement\n2 hit points\n";
	di_p1s[4]=2;
	di_p2s[4]=13;
	di_storage[4]=chk;
	di.num_elements=5;
	if(do_dialog()) return -1;
	return intel-1+(web<<3)+((chk[0]^1)<<5)+((chk[1])<<7);
}

int pe_goblin(int param) {
	int intel=((param>>6)&3)+1;
	int rest_len=(param&3)+1;
	reset_di();
	di_types[2]=DE_NUMBER;
	di_xs[2]=15;
	di_ys[2]=7;
	di_strs[2]="Intellegence: ";
	di_p1s[2]=1;
	di_p2s[2]=4;
	di_storage[2]=&intel;
	di_types[3]=DE_NUMBER;
	di_xs[3]=15;
	di_ys[3]=9;
	di_strs[3]="Rest length:  ";
	di_p1s[3]=1;
	di_p2s[3]=4;
	di_storage[3]=&rest_len;
	di.num_elements=4;
	if(do_dialog()) return -1;
	return rest_len-1+((intel-1)<<6);
}

int pe_bullet_gun(int param) {
	int dir=(param>>3)&3;
	int intel=((param>>5)&3)+1;
	int fire_rate=(param&7)+1;
	char type=(param>>7);
	reset_di();
	di_types[2]=DE_RADIO;
	di_xs[2]=15;
	di_ys[2]=2;
	di_strs[2]="North\nSouth\nEast\nWest";
	di_p1s[2]=4;
	di_p2s[2]=5;
	di_storage[2]=&dir;
	di_types[3]=DE_NUMBER;
	di_xs[3]=15;
	di_ys[3]=7;
	di_strs[3]="Intellegence: ";
	di_p1s[3]=1;
	di_p2s[3]=4;
	di_storage[3]=&intel;
	di_types[4]=DE_NUMBER;
	di_xs[4]=15;
	di_ys[4]=9;
	di_strs[4]="Firing rate:  ";
	di_p1s[4]=1;
	di_p2s[4]=8;
	di_storage[4]=&fire_rate;
	di_types[5]=DE_RADIO;
	di_xs[5]=15;
	di_ys[5]=11;
	di_strs[5]="Fires bullets\nFires fire";
	di_p1s[5]=2;
	di_p2s[5]=13;
	di_storage[5]=&type;
	di.num_elements=6;
	if(do_dialog()) return -1;
	return fire_rate-1+(dir<<3)+((intel-1)<<5)+(type<<7);
}

int pe_bear(int param) {
	int sens=(param&7)+1;
	int speed=((param>>3)&3)+1;
	char HP=(param>>7);
	reset_di();
	di_types[2]=DE_NUMBER;
	di_xs[2]=15;
	di_ys[2]=6;
	di_strs[2]="Sensitivity:   ";
	di_p1s[2]=1;
	di_p2s[2]=8;
	di_storage[2]=&sens;
	di_types[3]=DE_NUMBER;
	di_xs[3]=15;
	di_ys[3]=8;
	di_strs[3]="Movement speed: ";
	di_p1s[3]=1;
	di_p2s[3]=4;
	di_storage[3]=&speed;
	di_types[4]=DE_CHECK;
	di_xs[4]=15;
	di_ys[4]=10;
	di_strs[4]="2 hit points";
	di_p1s[4]=1;
	di_p2s[4]=12;
	di_storage[4]=&HP;
	di.num_elements=5;
	if(do_dialog()) return -1;
	return sens-1+((speed-1)<<3)+(HP<<7);
}

int pe_bear_cub(int param) {
	int intel=(param&3)+1;
	int switch_rate=((param>>2)&3)+1;
	reset_di();
	di_types[2]=DE_NUMBER;
	di_xs[2]=15;
	di_ys[2]=7;
	di_strs[2]="Intellegence: ";
	di_p1s[2]=1;
	di_p2s[2]=4;
	di_storage[2]=&intel;
	di_types[3]=DE_NUMBER;
	di_xs[3]=15;
	di_ys[3]=9;
	di_strs[3]="Switch rate:  ";
	di_p1s[3]=1;
	di_p2s[3]=4;
	di_storage[3]=&switch_rate;
	di.num_elements=4;
	if(do_dialog()) return -1;
	return intel-1+((switch_rate-1)<<2);
}

int pe_missile_gun(int param) {
	int dir=(param>>3)&3;
	int intel=((param>>5)&3)+1;
	int fire_rate=(param&7)+1;
	char type=(param>>7);
	reset_di();
	di_types[2]=DE_RADIO;
	di_xs[2]=15;
	di_ys[2]=2;
	di_strs[2]="North\nSouth\nEast\nWest";
	di_p1s[2]=4;
	di_p2s[2]=5;
	di_storage[2]=&dir;
	di_types[3]=DE_NUMBER;
	di_xs[3]=15;
	di_ys[3]=7;
	di_strs[3]="Intellegence: ";
	di_p1s[3]=1;
	di_p2s[3]=4;
	di_storage[3]=&intel;
	di_types[4]=DE_NUMBER;
	di_xs[4]=15;
	di_ys[4]=9;
	di_strs[4]="Firing rate:  ";
	di_p1s[4]=1;
	di_p2s[4]=8;
	di_storage[4]=&fire_rate;
	di_types[5]=DE_RADIO;
	di_xs[5]=15;
	di_ys[5]=11;
	di_strs[5]="Fires once\nFires multiple";
	di_p1s[5]=2;
	di_p2s[5]=14;
	di_storage[5]=&type;
	di.num_elements=6;
	if(do_dialog()) return -1;
	return fire_rate-1+(dir<<3)+((intel-1)<<5)+(type<<7);
}

int pe_sensor(int param) {
	char name[15];
	char robot[15];
	unsigned char chr=sensors[param].sensor_char;
	set_context(94);
	str_cpy(name,sensors[param].sensor_name);
	str_cpy(robot,sensors[param].robot_to_mesg);
	reset_di();
	di_types[2]=DE_INPUT;
	di_xs[2]=15;
	di_ys[2]=6;
	di_strs[2]="Sensor's name:    ";
	di_p1s[2]=14;
	di_storage[2]=name;
	di_types[3]=DE_INPUT;
	di_xs[3]=15;
	di_ys[3]=8;
	di_strs[3]="Robot to message: ";
	di_p1s[3]=14;
	di_storage[3]=robot;
	di_types[4]=DE_CHAR;
	di_xs[4]=15;
	di_ys[4]=10;
	di_strs[4]="Sensor character: ";
	di_p1s[4]=1;
	di_storage[4]=&chr;
	di.num_elements=5;
	if(do_dialog()) {
		pop_context();
		return -1;
		}
	str_cpy(sensors[param].sensor_name,name);
	str_cpy(sensors[param].robot_to_mesg,robot);
	sensors[param].sensor_char=chr;
	pop_context();
	return param;
}