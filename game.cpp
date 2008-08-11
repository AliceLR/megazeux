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


// I added a bunch of stuff here. Whenever a window is popped up, the mouse
// cursor is re-activated, and then CURSORSTATE is checked upon closing the
// window. If it's 1, the cursor is left on, otherwise it's hidden as normal.
// You can find all instances by searching for CURSORSTATE. Spid


/* Main title screen/gaming code */

#include <dos.h>
#include "egacode.h"
#include "profile.h"
#include "helpsys.h"
#include "scrdisp.h"
#include "error.h"
#include <stdlib.h>
#include "runrobot.h"
#include "arrowkey.h"
#include "idarray.h"
#include "mod.h"
#include "edit.h"
#include "sfx.h"
#include "string.h"
#include "beep.h"
#include "hexchar.h"
#include "retrace.h"
#include "getkey.h"
#include "idput.h"
#include "data.h"
#include "const.h"
#include "game.h"
#include "ezboard.h"
#include "window.h"
#include "roballoc.h"
#include "saveload.h"
#include <stdio.h>
#include "palette.h"
#include "mouse.h"
#include "graphics.h"
#include "random.h"
#include "counter.h"
#include "game2.h"
#include "timer.h"
#include "scrdump.h"
#include "blink.h"
#include "cursor.h"

// Sprites! - Exo
#include "sprite.h"

int get_built_in_messages(void);

char far *main_menu= "Enter- Menu\n"
							"Esc  - Exit to DOS\n"
							"F1/H - Help\n"
							"F2/S - Settings\n"
							"F3/L - Load world\n"
							"F4/R - Restore game\n"
							"F5/P - Play world\n"
							"F6   - Debug menu\n"
							"F8/E - World editor\n"
							"F10  - Quickload"
							"";

char far *game_menu=	"F1    - Help\n"
							"Enter - Menu/status\n"
							"Esc   - Exit to title\n"
							"F2    - Settings\n"
							"F3    - Save game\n"
							"F4    - Restore game\n"
							"F5/Ins- Toggle bomb type\n"
							"F6    - Debug menu\n"
							"F9    - Quicksave\n"
							"F10   - Quickload\n"
							"Arrows- Move\n"
							"Space - Shoot (w/dir)\n"
							"Delete- Bomb";

//Easter Egg enter menu -Koji
// Idea by Exophase.
// Wasting mem with style.. okay, not anymore :( - Exo
/* char far *lame_menu=	"F1    - HELP!!1\n"
							"etner - TIHS MENUE\n"
							"EScapE- DONT PRESS LOL\n"
							"f2    - UR SETTINGS\n"
							"F3    - SAV UR GAEM\n"
							"F4    - LOAD UR GAAM\n"
							"F5-iNs- I DONNO! LOLZ\n"
							"f6    - :((((((((\n"
							"f9    - quickSNAD!\n"
							"ef10  - QUICKLOAD\n"
							"Arrowz- MOVE UR GUY :D :D\n"
							"spaec - shoot UR GUN\n"
							"deleet- DA BOMB!!!111111"; */


int main_menu_keys[11]={ -59,0,27,-60,-61,-62,-63,-64,-65,-66,-68 };
int game_menu_keys[13]={ -59,0,27,-60,-61,-62,-63,-64,-67,-68,0,0,0 };

int key_get;

char bomb_type=1;//Start on hi-bombs
char dead=0;

//For changing screens AFTER an update is done and shown
int target_board=-1;//Where to go
int target_where=-2;//0 for x/y, 1 for entrance
						  //-1 for teleport (so fading isn't used)
int target_x=-1;//Or color of entrance
int target_y=-1;//Or id of entrance
int target_d_id=-1;//For RESTORE/EXCHANGE PLAYER POSITION with DUPLICATION.
int target_d_color=-1;//For RESTORE/EXCHANGE PLAYER POSITION with DUPLICATION.


char pal_update=0;//Whether to update a palette from robot activity


void title_screen(void) {
	char fadein=0;
	int key=0,t1;
	FILE *fp;
	char temp[FILENAME_SIZE];
		  char temp2[FILENAME_SIZE];

#ifdef PROFILE
	char profiling=0;
#endif
	debug_mode=0;
	error_mode=2;
	//Clear world
	clear_world(0);
	clear_zero_objects();
	clear_game_params();
	//Set page (mouse is hidden)
	current_page=0;
	current_pg_seg=VIDEO_SEG;
	page_flip(0);
	m_vidseg(current_pg_seg);
	//Clear screen
	clear_screen(1824,current_pg_seg);
	//Palette
	default_palette();
	insta_fadein();
	//Try to load curr_file
	fp=fopen(curr_file,"rb");
	if(fp!=NULL) {
		fclose(fp);
		if(load_world(curr_file)) clear_world();
		select_current(0);
		clear_game_params();
		set_counter("TIME",time_limit);
		insta_fadeout();
		fadein=1;
	}
	else goto world_load;//Choose world to load

	goto menu_mesg; //stupid hack to shut up tc

	//Main game loop
	//Mouse remains hidden unless menu/etc. is invoked

	do {
		//Update
		if(update(0,fadein)) continue;
		// Update stupid variables
			 //	mynewx= scroll_x;
			 //	mynewy= scroll_y;
		//Keycheck
		if(keywaiting()) {
			//Get key...
			key=getkey();
      key_get = key;
			if((key>='a')&&(key<='z')) key-=32;
			//...and process*/
		process_key:
			switch(key) {
#ifdef PROFILE
				case 'Q'://P
					//Profiling
					if(profiling) {
						set_mesg("Profiling OFF");
						profiling_off();
						}
					else {
						set_mesg("Profiling ON");
						profiling_on();
						}
					profiling^=1;
					break;
#endif
				case ']'://Screen .PCX dump
					dump_screen();
					break;
				case -17://AltW
					//Re-init screen
					vga_16p_mode();
					ega_14p_mode();
					cursor_off();
					blink_off();
					ec_update_set();
					update_palette(0);
					break;
				case 'E'://E
				case -66://F8
					//Editor
					clear_sfx_queue();
					vquick_fadeout();
					for(t1=0;t1<16;t1++)
						set_color_intensity(t1,100);
					edit_world();
					if(debug_mode) error_mode=1;
					else error_mode=2;
					m_hide();
					fadein=1;
					//Clear world...
					clear_world(0);
					clear_zero_objects();
					//...try to load current.
					if(curr_file[0]!=0) {
						//Clear screen
						clear_screen(1824,current_pg_seg);
						//Palette
						default_palette();
						insta_fadein();
						//Load
						if(load_world(curr_file,2)) clear_world();
						//Blackout
						select_current(0);
						insta_fadeout();
						clear_game_params();
						set_counter("TIME",time_limit);
						}
					clear_game_params();
					dead=0;
					break;
				case 'S'://S
				case -60://F2
					//Settings
					m_show();
					t1=is_faded();
					if(t1) {
						clear_screen(1824,current_pg_seg);
						insta_fadein();
						}
					game_settings();
					if(t1) insta_fadeout();
					m_hide();
					break;
				case 13://Enter
					//Menu
					//19x9
					if(get_counter("ENTER_MENU") != 0)
					{
						save_screen(current_pg_seg);
						t1=is_faded();
						if(t1) {
							clear_screen(1824,current_pg_seg);
							insta_fadein();
							}
						draw_window_box(30,4,52,16,current_pg_seg,25,16,24);
						write_string(main_menu,32,5,31,current_pg_seg);
						write_string(" Main Menu ",36,4,30,current_pg_seg);
						m_show();
						do {
						key=getkey();
            key_get = key;
						} while(!key);
						if(t1) insta_fadeout();
						if(key==27) key=0;
						if(key==13) key=0;
						if(key==MOUSE_EVENT) {
							if((mouse_event.cx<31)||(mouse_event.cx>51)||
								(mouse_event.cy<8)||(mouse_event.cy>17)) key=0;
							else key=main_menu_keys[mouse_event.cy-8];
							}
						if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
						restore_screen(current_pg_seg);
						goto process_key;
					}
					break;
				case 27://ESC
					//Quit
					m_show();
					t1=is_faded();
					if(t1) {
						clear_screen(1824,current_pg_seg);
						insta_fadein();
						}
					if(confirm("Quit to DOS- Are you sure?")) key=0;
					if(t1)
					{
						insta_fadeout();
					}
					if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
					break;
				case 'L'://L
				case -61://F3
				world_load:
					//Load
					m_show();
					t1=is_faded();
					if(t1) {
						clear_screen(1824,current_pg_seg);
						insta_fadein();
						}
					if(choose_file("*.MZX",curr_file,"Load World",1)) {
						if(t1) insta_fadeout();
						break;
						}
					if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
					//Load world curr_file
					vquick_fadeout();
					end_mod();
					clear_sfx_queue();
					//Clear screen
					clear_screen(1824,current_pg_seg);
					//Palette
					default_palette();
					insta_fadein();
					if(load_world(curr_file)) clear_world();
					select_current(0);
					send_robot_def(0,10);
					clear_game_params();
					set_counter("TIME",time_limit);
					insta_fadeout();
					fadein=1;
					dead=0;
				menu_mesg:
#ifdef UNREG
					set_mesg("** UNREGISTERED ** Press F1 for help, Enter for menu");
#elif defined(BETA)
					set_mesg("** BETA ** Press F1 for help, Enter for menu");
#else
					set_mesg("- Press F1 for help, Enter for menu -");
#endif
					break;
				case 'R'://R
				case -62://F4
					//Restore
					m_show();
					t1=is_faded();
					if(t1) {
						clear_screen(1824,current_pg_seg);
						insta_fadein();
						}
					if(!choose_file("*.SAV",temp,"Choose game to restore",1)) {
						//Swap out current board...
						store_current();
						clear_current();
						clear_sfx_queue();
						//Load game
						fadein=0;
						if(load_world(temp,0,1,&fadein)) {
							vquick_fadeout();
							if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
							switch_keyb_table(1);
							clear_world();
							return;
							}
						//Swap in starting board
						if(board_where[curr_board]!=W_NOWHERE)
							select_current(curr_board);
						else select_current(0);
						str_cpy(temp2,mod_playing);
																load_mod(temp2);

						send_robot_def(0,10);
						//Copy filename
						str_cpy(curr_sav,temp);
						dead=0;
						vquick_fadeout();
						fadein^=1;
						if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
						goto gameplay;
						}
					if(t1) insta_fadeout();
					if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
					break;
				case 'P'://P
				case -63://F5
					//Play
					//Only from swap?
					if(only_from_swap) {
						m_show();
						error("You can only play this game via a swap from another game",
							0,24,current_pg_seg,0x3101);
						if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
						break;
						}
					//Load world curr_file
					vquick_fadeout();
					//Don't end mod- We want to have smooth transition for that.
					//Clear screen
					clear_screen(1824,current_pg_seg);
					//Palette
					default_palette();
					insta_fadein();
					if(load_world(curr_file,2)) {
						clear_world();
						select_current(0);
						clear_game_params();
						break;
						}
					clear_game_params();
					select_current(first_board);
					send_robot_def(0,11);
					send_robot_def(0,10);
					set_counter("TIME",time_limit);
					insta_fadeout();
					fadein=1;
				gameplay:
					clear_sfx_queue();
					player_restart_x=player_x;
					player_restart_y=player_y;
					play_game(fadein);
					//Done playing- load world again
					//Already faded out from play_game()
					end_mod();
					//Clear screen
					clear_screen(1824,current_pg_seg);
					//Palette
					default_palette();
					for(t1=0;t1<16;t1++)
						set_color_intensity(t1,100);
					insta_fadein();
					if(load_world(curr_file,2)) clear_world();
					clear_game_params();
					select_current(0);
					set_counter("TIME",time_limit);
					insta_fadeout();
					fadein=1;
					dead=0;
					goto menu_mesg;
				case -64://F6
					//Debug menu
					debug_mode^=1;
					if(debug_mode) error_mode=1;
					else error_mode=2;
					break;
/*				case -65://F7
					//SMZX Mode
					set_counter("SMZX_MODE",smzx_mode + 1,0);
					break;*/
				case -68://F10
					//Quickload
					if(curr_sav[0]==0) break;
					m_show();
					//Swap out current board...
					store_current();
					clear_current();
					clear_sfx_queue();
					//Load game
					fadein=0;
					if(load_world(curr_sav,0,1,&fadein)) {
						vquick_fadeout();
						if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
						clear_world();
						fadein=1;
						break;
						}
					//Swap in starting board
					if(board_where[curr_board]!=W_NOWHERE)
						select_current(curr_board);
					else select_current(0);
					str_cpy(temp2,mod_playing);
					load_mod(temp2);

					send_robot_def(0,10);
					dead=0;
					vquick_fadeout();
					fadein^=1;
					if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
					goto gameplay;
				}
			}
	} while(key!=27);

	vquick_fadeout();
	clear_world();
	clear_sfx_queue();
}

void draw_viewport(void) {
	// Sneak this biotch in here
	//mynewx= scroll_x;
	//mynewy= scroll_y;
	int t1,t2;
	enter_func("draw_viewport");
	//Draw the current viewport on current_pg_seg
	if(overall_speed==1) {
		//Special clear- only fills areas that aren't part of the display.
		//Less flicker for speed #1
		if(viewport_y>1) {
			//Top
			for(t1=0;t1<viewport_y;t1++)
				fill_line(80,0,t1,177+(edge_color<<8),current_pg_seg);
			}
		if((viewport_y+viewport_ysiz)<24) {
			//Bottom
			for(t1=viewport_y+viewport_ysiz+1;t1<25;t1++)
				fill_line(80,0,t1,177+(edge_color<<8),current_pg_seg);
			}
		if(viewport_x>1) {
			//Left
			for(t1=0;t1<25;t1++)
				fill_line(viewport_x,0,t1,177+(edge_color<<8),current_pg_seg);
			}
		if((viewport_x+viewport_xsiz)<79) {
			//Right
			t2=viewport_x+viewport_xsiz+1;
			for(t1=0;t1<25;t1++)
				fill_line(80-t2,t2,t1,177+(edge_color<<8),current_pg_seg);
			}
		}
	else clear_screen(177+(edge_color<<8),current_pg_seg);
	//Draw the box
	if(viewport_x>0) {/* left */
		for(t1=0;t1<viewport_ysiz;t1++)
			draw_char('º',edge_color,viewport_x-1,t1+viewport_y,current_pg_seg);
		if(viewport_y>0)
			draw_char('É',edge_color,viewport_x-1,viewport_y-1,current_pg_seg);
		}
	if((viewport_x+viewport_xsiz)<80) {/* right */
		for(t1=0;t1<viewport_ysiz;t1++)
			draw_char('º',edge_color,viewport_x+viewport_xsiz,t1+viewport_y,
				current_pg_seg);
		if(viewport_y>0)
			draw_char('»',edge_color,viewport_x+viewport_xsiz,viewport_y-1,
				current_pg_seg);
		}
	if(viewport_y>0) {/* top */
		for(t1=0;t1<viewport_xsiz;t1++)
			draw_char('Í',edge_color,viewport_x+t1,viewport_y-1,current_pg_seg);
		}
	if((viewport_y+viewport_ysiz)<25) {/* bottom */
		for(t1=0;t1<viewport_xsiz;t1++)
			draw_char('Í',edge_color,viewport_x+t1,viewport_y+viewport_ysiz,
				current_pg_seg);
		if(viewport_x>0)
			draw_char('È',edge_color,viewport_x-1,viewport_y+viewport_ysiz,
				current_pg_seg);
		if((viewport_x+viewport_xsiz)<80)
			draw_char('¼',edge_color,viewport_x+viewport_xsiz,
				viewport_y+viewport_ysiz,current_pg_seg);
		}
	exit_func();
}

//Updates game variables
//Slowed=1 to not update lazwall or time due to slowtime or freezetime
void update_variables(char slowed) {
	static char slowdown=0;//Slows certain things down to every other cycle
	unsigned int t1;
	enter_func("update_variables");
/*	if (get_counter("MZXAKVERSION" == 1)
	{
		overall_speed = get_counter("MZX_SPEED")
	}
*/	slowdown^=1;
	//If odd, we...
	if(!slowdown) {
		//Change scroll color
		if(++scroll_color>15) scroll_color=7;
		//Decrease time limit
		if(!slowed) {
			t1=get_counter("TIME");
			if(t1>0) {
				if(--t1==0) {
					//Out of time
					dec_counter("HEALTH",10);
					set_mesg("Out of time!");
					play_sfx(42);
					//Reset time
					set_counter("TIME",time_limit);
					}
				else set_counter("TIME",t1);
				}
			}
		}
	//Decrease effect durations
	if(blind_dur>0) blind_dur--;
	if(firewalker_dur>0) firewalker_dur--;
	if(freeze_time_dur>0) freeze_time_dur--;
	if(slow_time_dur>0) slow_time_dur--;
	if(wind_dur>0) wind_dur--;
	//Decrease message timer
	if(b_mesg_timer>0) b_mesg_timer--;
	//Invinco
	t1=get_counter("INVINCO");
	if(t1>0) {
		if(t1==1) {
			//Just finished-
			set_counter("INVINCO");
			clear_sfx_queue();
			play_sfx(18);
			*player_color=saved_pl_color;
			}
		else {
			//Decrease
			set_counter("INVINCO",t1-1);
			play_sfx(17);
			*player_color=random_num()&255;
			}
		}
	//Lazerwall start- cycle 0 to 7 then -7 to -1
	if(!slowed) {
		if(((signed char)lazwall_start)<7) lazwall_start++;
		else lazwall_start=-7;
		}
	//Done
	exit_func();
}

void set_mesg(char far *str) {
	enter_func("set_mesg");
	if (get_built_in_messages()){
		//Sets the current message
		if(str_len(str)>80) {
			mem_cpy(bottom_mesg,str,80);
			bottom_mesg[80]=0;
			}
		else str_cpy(bottom_mesg,str);
		b_mesg_timer=160;
		}
	exit_func();
}

void set_3_mesg(char far *str1,int num,char far *str2) {
	char tmp[7];
	enter_func("set_mesg");
	itoa(num,tmp,10);
	//Concatenates 3 strings into the curr. message
	str_cpy(bottom_mesg,str1);
	str_cat(bottom_mesg,tmp);
	str_cat(bottom_mesg,str2);
	b_mesg_timer=160;
	exit_func();
}

//Bit 1- +1
//Bit 2- -1
//Bit 4- +xsiz
//Bit 8- -xsiz
char cw_offs[8]={ 10,2,6,4,5,1,9,8 };
char ccw_offs[8]={ 10,8,9,1,5,4,6,2 };

//Rotate an area
void rotate(int x,int y,char dir) {
	char *offsp=cw_offs;
	int offs[8];
	int pos,t1,t2,t3,t4;
	pos=x+y*max_bxsiz;
	if((x==0)||(y==0)||(x==(board_xsiz-1))||
		(y==(board_ysiz-1))) return;
	if(dir) offsp=ccw_offs;
	//Fix offsets
	for(t1=0;t1<8;t1++) {
		t2=offsp[t1];
		offs[t1]=(t2&1?1:0)+(t2&2?-1:0)+(t2&4?max_bxsiz:0)+(t2&8?-max_bxsiz:0);
		}
	for(t1=0;t1<8;t1++) {
		if((flags[level_id[pos+offs[t1]]]&A_UNDER)&&(level_id[pos+offs[t1]]!=34))
			break;
		}
	if(t1==8) {
		for(t1=0;t1<8;t1++) {
			if(!((flags[level_id[pos+offs[t1]]]&A_PUSHABLE)||
				(flags[level_id[pos+offs[t1]]]&A_SPEC_PUSH))) break;
			if(level_id[pos+offs[t1]]==47) break;//Transport NOT pushable
			}
		if(t1!=8) return;
		t2=level_id[pos+offs[0]];
		t3=level_color[pos+offs[0]];
		t4=level_param[pos+offs[0]];
		for(t1=0;t1<7;t1++) {
			level_id[pos+offs[t1]]=level_id[pos+offs[t1+1]];
			level_color[pos+offs[t1]]=level_color[pos+offs[t1+1]];
			level_param[pos+offs[t1]]=level_param[pos+offs[t1+1]];
			}
		level_id[pos+offs[7]]=t2;
		level_color[pos+offs[7]]=t3;
		level_param[pos+offs[7]]=t4;
		return;
		}
	t2=t1;
	t2--;
	if(t2==-1) t2=7;
	do {
		t3=t1+1;
		if(t3==8) t3=0;
		if((flags[level_id[pos+offs[t3]]]&A_PUSHABLE)||
			(flags[level_id[pos+offs[t3]]]&A_SPEC_PUSH)) {
				if(level_id[pos+offs[t3]]==47) goto solid;
				if(update_done[pos+offs[t3]]&2) goto solid;
				offs_place_id(pos+offs[t1],0,
					level_id[pos+offs[t3]],
					level_color[pos+offs[t3]],
					level_param[pos+offs[t3]]);
				offs_remove_id(pos+offs[t3],0);
				update_done[pos+offs[t1]]|=2;
				t1=t3;
				continue;
				}
	solid:
		t1=t3;
		while(t1!=t2) {
			if((flags[level_id[pos+offs[t1]]]&A_UNDER)&&(level_id[pos+offs[t1]]!=34))
				break;
			t1++;
			if(t1==8) t1=0;
		};
	} while(t1!=t2);
}

void calculate_xytop(int &x,int &y) {
	enter_func("calculate_xytop");
	//Calculate xy top from player position and scroll view pos, or
	//as static position if set.
	if(locked_y!=65535) {
		x=locked_x+scroll_x;
		y=locked_y+scroll_y;
		}
	else {
		//Calculate from player position
		//Center screen around player, add scroll factor
		x=player_x-(viewport_xsiz/2);
		y=player_y-(viewport_ysiz/2);
		if(x<0) x=0;
		if(y<0) y=0;
		if(x>(board_xsiz-viewport_xsiz)) x=board_xsiz-viewport_xsiz;
		if(y>(board_ysiz-viewport_ysiz)) y=board_ysiz-viewport_ysiz;
		x+=scroll_x;
		y+=scroll_y;
		}
	//Prevent from going offscreen
	if(x<0) x=0;
	if(y<0) y=0;
	if(x>(board_xsiz-viewport_xsiz)) x=board_xsiz-viewport_xsiz;
	if(y>(board_ysiz-viewport_ysiz)) y=board_ysiz-viewport_ysiz;
	//Done!
	exit_func();
}

void update_player(void) {
	enter_func("update_player");
	int t1=level_under_id[player_x+player_y*max_bxsiz];
	//t1=ID stood on
	if(!(flags[t1]&A_AFFECT_IF_STOOD)) {
		exit_func();
		return;//Nothing special
		}
	switch(t1) {
		case 25://Ice
			if((player_last_dir&15)>0)
				if(move_player((player_last_dir&15)-1))
					player_last_dir&=240;
			break;
		case 26://Lava
			if(firewalker_dur>0) break;
			play_sfx(22);
			set_mesg("Augh!");
			dec_counter("HEALTH",id_dmg[26]);
			exit_func();
			return;
		case 63://Fire
			if(firewalker_dur>0) break;
			play_sfx(43);
			set_mesg("Ouch!");
			dec_counter("HEALTH",id_dmg[63]);
			exit_func();
			return;
		default://Water
			move_player(t1-21);
			break;
		}
	find_player();
	//Done
	exit_func();
}

//Settings dialog-

//------------------------
//
//  Speed- 123456789
//
//   ( ) Music on
//   ( ) Music off
//
//   ( ) SFX on
//   ( ) SFX off
//
//    OK        Cancel
//
//------------------------

//----------------------------
//
//  Speed- 123456789
//
//   ( ) Digitized music on
//   ( ) Digitized music off
//
//   ( ) PC speaker SFX on
//   ( ) PC speaker SFX off
//
//  Sound card volumes-
//  Overall volume- 12345678
//  SoundFX volume- 12345678
//
//    OK        Cancel
//
//----------------------------

int spd_tmp,music,sfx,mgvol,sgvol;
char stdi_types[8]={ DE_NUMBER,DE_RADIO,DE_RADIO,DE_TEXT,
	DE_NUMBER,DE_NUMBER,DE_BUTTON,DE_BUTTON };
char stdi_xs[8]={ 3,4,4,3,3,3,4,14 };
char stdi_ys[8]={ 2,4,7,10,11,12,14,14 };
char far *stdi_strs[8]={ "Speed- ","Digitized music on\nDigitized music off",
	"PC speaker SFX on\nPC speaker SFX off","Sound card volumes-",
	"Overall volume- ","SoundFX volume- ","OK","Cancel" };
int stdi_p1s[8]={ 1,2,2,0,1,1,0,1 };
int stdi_p2s[6]={ 9,9,7,0,8,8 };
void far *stdi_storage[6]={ &spd_tmp,&music,&sfx,NULL,&mgvol,&sgvol };
dialog stdi={
	25,4,54,20,"Game settings",8,
	stdi_types,stdi_xs,stdi_ys,
	stdi_strs,stdi_p1s,stdi_p2s,stdi_storage,0 };

void game_settings(void) {
	char temp[FILENAME_SIZE];
	set_context(92);
	spd_tmp=overall_speed;
	music=music_on^1;
	sfx=sfx_on^1;
	mgvol=music_gvol;
	sgvol=sound_gvol;
	if(run_dialog(&stdi,current_pg_seg)) {
		pop_context();
		return;
		}
	pop_context();
	if((sound_gvol!=sgvol)||(music_gvol!=mgvol)) {
		sound_gvol=sgvol;
		music_gvol=mgvol;
		fix_global_volumes();
		}
	//Check- turn off sound?
	if(sfx==sfx_on) {
		sfx_on=sfx^1;
		//clear_queue();
		}
	else sfx_on=sfx^1;
	//Check- turn music on/off?
	if(music==music_on) {
		char temp2[FILENAME_SIZE];
		if(music_on==1) {
			music_on=0;
			//Turn off music.
			str_cpy(temp,real_mod_playing);
			str_cpy(temp2,mod_playing);
			end_mod();
			str_cpy(real_mod_playing,temp);
			str_cpy(mod_playing,temp2);
		}
		else if(music_device>0) {
			music_on=1;
			//Turn on music.
			str_cpy(temp2,mod_playing);
			str_cpy(temp,real_mod_playing);
			load_mod(temp);
			str_cpy(real_mod_playing,temp);
			str_cpy(mod_playing,temp2);
		}
	}
/*	if (get_counter("MZXAVERSION") = 1)
	{
		Set_counter("MZX_SPEED", spd_tmp);
	}
*/	overall_speed=spd_tmp;
}

//Number of cycles to make player idle before repeating a directional move
#define REPEAT_WAIT	2

void play_game(char fadein) {
	//We have the world loaded, on the proper scene.
	//We are faded out. Commence playing!
	int t1,key;
	FILE *fp;
	char temp[FILENAME_SIZE];
	char temp2[FILENAME_SIZE];
	char keylbl[5]="KEY?";
	enter_func("play_game");

	set_context(91);

	switch_keyb_table(0);
	dead=0;

	//Set page (mouse is hidden)
	current_page=0;
	current_pg_seg=VIDEO_SEG;
	page_flip(0);
	m_vidseg(current_pg_seg);

	//Main game loop
	//Mouse remains hidden unless menu/etc. is invoked
	//Making the menu functions on by default -Koji
	set_counter("ENTER_MENU",1);
	set_counter("HELP_MENU",1);
	set_counter("F2_MENU",1);
	do {
		//Update
		if(update(1,fadein)) continue;
		//Keycheck

    key_get = 0;
		if(keywaiting()) {
			//Get key...
			key = getkey();
      key_get = key;
			if((key>='a')&&(key<='z')) key-=32;
			//...and process
		process_key:
			//all Keys accessable -Koji
      //"KeyEnter" exception - Exophase
			//if(key<'1') break;
			//if((key>'9')&&(key<'A')) break;
			//if(key>'Z') break;
			keylbl[3]=last_key=key;
      if(key == 13)
      {
        send_robot("ALL", "KeyEnter");
      }
			send_robot("ALL",keylbl);
			switch(key) {
				case ']'://Screen .PCX dump
					dump_screen();
					break;
				default://moved.
					//all Keys accessable -Koji
					//if(key<'1') break;
					//if((key>'9')&&(key<'A')) break;
					//if(key>'Z') break;
					//if(key == 13) send_robot("ALL", "EnterKey");
					//keylbl[3]=last_key=key;
					//send_robot("ALL",keylbl);
					break;
				case -60://F2
					//Settings
					if(get_counter("F2_MENU"))
					{
						m_show();
						switch_keyb_table(1);
						t1=is_faded();
						if(t1) {
							clear_screen(1824,current_pg_seg);
							insta_fadein();
							}
						game_settings();
						if(t1) insta_fadeout();
						switch_keyb_table(0);
						if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
					}
					break;
				case 13://Enter
					//Menu
					//19x9
					if(get_counter ("ENTER_MENU") != 0)
					{
						save_screen(current_pg_seg);
						t1=is_faded();
						if(t1) {
							clear_screen(1824,current_pg_seg);
							insta_fadein();
							}
						draw_window_box(8,4,35,18,current_pg_seg,25,16,24);
						write_string(game_menu,10,5,31,current_pg_seg);
						write_string(" Game Menu ",14,4,30,current_pg_seg);
						show_status();//Status screen too
						m_show();
						do {
							key=getkey();
						} while(!key);
						if(t1) insta_fadeout();
						if(key==27) key=0;
						if(key==13) key=0;
						if(key==MOUSE_EVENT) {
							if((mouse_event.cx<11)||(mouse_event.cx>31)||
								(mouse_event.cy<6)||(mouse_event.cy>18)) key=0;
							else key=game_menu_keys[mouse_event.cy-6];
							}
						if(get_counter("CURSORSTATE") == 0) m_hide();
						restore_screen(current_pg_seg);
						if(t1) insta_fadeout();
						goto process_key;
					}
					break;

				   case 27://ESC
					//Quit
					m_show();
					switch_keyb_table(1);
					t1=is_faded();
					if(t1) {
						clear_screen(1824,current_pg_seg);
						insta_fadein();
						}
					if(confirm("Quit playing- Are you sure?")) key=0;
					if(t1) insta_fadeout();
					switch_keyb_table(0);
					if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
					break;
				case -61://F3
					//Save game
					if(!dead) {
						//Can we?
						if(cheats_active>1) break;
						if(save_mode==CANT_SAVE) break;
						if(save_mode==CAN_SAVE_ON_SENSOR)
							if(level_under_id[player_x+max_bxsiz*player_y]!=122) break;
						m_show();
						switch_keyb_table(1);
						//If faded...
						t1=is_faded();
						if(t1) {
							clear_screen(1824,current_pg_seg);
							insta_fadein();
							}
						if(!save_game_dialog()) {
							//Name in curr_sav....
							if(curr_sav[0]==0) {
								if(t1) insta_fadeout();
								switch_keyb_table(0);
								if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
								break;
								}
							add_ext(curr_sav,".SAV");
							//Check for an overwrite
							if((fp=fopen(curr_sav,"rb"))!=NULL) {
								fclose(fp);
								if(confirm("File exists- Overwrite?")) {
									if(t1) insta_fadeout();
									switch_keyb_table(0);
									if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
									break;
									}
								}
							//Store current
							store_current();
							//Save entire game
							save_world(curr_sav,1,t1);
							//Reload current
							select_current(curr_board);
							}
						if(t1) insta_fadeout();
						switch_keyb_table(0);
						if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
						}
					break;
				case -62://F4
					//Restore
					if(cheats_active>1) break;
					m_show();
					switch_keyb_table(1);
					t1=is_faded();
					if(t1) {
						clear_screen(1824,current_pg_seg);
						insta_fadein();
						}
					if(!choose_file("*.SAV",temp,"Choose game to restore",1)) {
						//Swap out current board...
						store_current();
						clear_current();
						//Load game
						fadein=0;
						if(load_world(temp,0,1,&fadein)) {
							vquick_fadeout();
							if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
							switch_keyb_table(1);
							clear_world();
							return;
							}
						//Swap in starting board
						if(board_where[curr_board]!=W_NOWHERE)
							select_current(curr_board);
						else select_current(0);
					str_cpy(temp2,mod_playing);
					load_mod(temp2);

						send_robot_def(0,10);
						//Copy filename
						str_cpy(curr_sav,temp);
						dead=0;
						fadein^=1;
						}
					if(t1) insta_fadeout();
					switch_keyb_table(0);
					if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
					break;
				case -63://F5
				case -82://Ins
				case '0'://Ins w/NumLock
					//Change bomb type
					if(!dead) {
						bomb_type^=1;
						if((!player_attack_locked)&&(can_bomb)) {
							play_sfx(35);
							if(bomb_type) set_mesg("You switch to high strength bombs.");
							else set_mesg("You switch to low strength bombs.");
							}
						}
					break;
				case -64://F6
					//Debug menu
					debug_mode^=1;
					if(error_mode) {//If error_mode==0 don't change!
						if(debug_mode) error_mode=1;
						else error_mode=2;
						}
					break;
				case -65://F7
					//Cheat #1- Give all
					if(cheats_active) {
						set_counter("GEMS",32767);
						set_counter("AMMO",32767);
						set_counter("HEALTH",32767);
						set_counter("COINS",32767);
						set_counter("LIVES",32767);
						set_counter("TIME",time_limit);
						set_counter("LOBOMBS",32767);
						set_counter("HIBOMBS",32767);
						score=0;
						for(t1=0;t1<16;t1++) keys[t1]=t1;
						dead=0;// :)
						player_ns_locked=player_ew_locked=
							player_attack_locked=0;
						}
					break;
				case -66://F8
					//Cheat #2- Zap surrounding
					if((cheats_active)&&(!dead)) {
						if(player_x>0) {
							id_clear(player_x-1,player_y);
							if(player_y>0) id_clear(player_x-1,player_y-1);
							if(player_y<(board_ysiz-1))
								id_clear(player_x-1,player_y+1);
							}
						if(player_x<(board_xsiz-1)) {
							id_clear(player_x+1,player_y);
							if(player_y>0) id_clear(player_x+1,player_y-1);
							if(player_y<(board_ysiz-1))
								id_clear(player_x+1,player_y+1);
							}
						if(player_y>0) id_clear(player_x,player_y-1);
						if(player_y<(board_ysiz-1))
							id_clear(player_x,player_y+1);
						}
					break;
				case -67://F9
					//Quicksave
					if(cheats_active>1) break;
					if(curr_sav[0]==0) break;
					if(!dead) {
						//Can we?
						if(save_mode==CANT_SAVE) break;
						if(save_mode==CAN_SAVE_ON_SENSOR)
							if(level_under_id[player_x+max_bxsiz*player_y]!=122) break;
						m_show();
						switch_keyb_table(1);
						//If faded...
						t1=is_faded();
						if(t1) {
							clear_screen(1824,current_pg_seg);
							insta_fadein();
							}
						//Store current
						store_current();
						//Save entire game
						save_world(curr_sav,1,t1);
						//Reload current
						select_current(curr_board);
						if(t1) insta_fadeout();
						switch_keyb_table(0);
						if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
						}
					break;
				case -69://F11
					//SMZX Mode
          set_counter("SMZX_MODE", smzx_mode ^ 1, 0);
					break;
				case '='://AltW
					//Re-init screen
					vga_16p_mode();
					ega_14p_mode();
					cursor_off();
					blink_off();
					ec_update_set();
					update_palette(0);
					break;
				case -68://F10
					//Quickload
					if(cheats_active>1) break;
					if(curr_sav[0]==0) break;
					m_show();
					switch_keyb_table(1);
					//Swap out current board...
					store_current();
					clear_current();
					//Load game
					fadein=0;
					if(load_world(curr_sav,0,1,&fadein)) {
						vquick_fadeout();
						if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
						switch_keyb_table(1);
						clear_world();
						return;
						}
					//Swap in starting board
					if(board_where[curr_board]!=W_NOWHERE)
						select_current(curr_board);
					else select_current(0);

					str_cpy(temp2,mod_playing);
					load_mod(temp2);

					send_robot_def(0,10);
					dead=0;
					vquick_fadeout();
					fadein^=1;
					switch_keyb_table(0);
					if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
					break;
				}
			}
	} while(key!=27);

	pop_context();
	vquick_fadeout();
	switch_keyb_table(1);
	clear_sfx_queue();
	exit_func();
}

//Returns 1 if didn't move
char move_player(char dir) {//Dir is from 0 to 3
	int t1=0,t2,t3,t4,t5,t6,t7,t8,old_x=player_x,old_y=player_y;
	int new_x=player_x,new_y=player_y,offs;
	enter_func("move_player");
	switch(dir) {
		case 0:
			if(--new_y<0) t1=1;
			break;
		case 1:
			if(++new_y>=board_ysiz) t1=1;
			break;
		case 2:
			if(++new_x>=board_xsiz) t1=1;
			break;
		case 3:
			if(--new_x<0) t1=1;
			break;
		}
	if(t1) {
		//Edge
		if(board_dir[dir]==NO_BOARD) return 1;//No exit
		if(board_where[board_dir[dir]]==W_NOWHERE) return 1;//Nonexistant
		target_board=board_dir[dir];
		target_where=0;
		target_x=player_x;
		target_y=player_y;
		switch(dir) {
			case 0://North- Enter south side
				target_y=199;
				break;
			case 1://South- Enter north side
				target_y=0;
				break;
			case 2://East- Enter west side
				target_x=0;
				break;
			case 3://West- Enter east side
				target_x=399;
				break;
			}
		player_last_dir=(player_last_dir&240)+dir+1;
		exit_func();
		return 0;
		}
	else {
		//Not edge
		offs=new_x+new_y*max_bxsiz;
		t1=level_id[offs];
		t2=flags[t1];
		if(t2&A_SPEC_STOOD) {
			//Sensor
			//Activate label and then move player
			t1=level_param[offs];
			send_robot(sensors[t1].robot_to_mesg,"SENSORON");
			goto move_player;
			}
		else if(t2&A_ENTRANCE) {
			//Entrance
			play_sfx(37);
			t1=level_param[offs];
			if(t1==curr_board) goto move_player;//Same board
			if(board_where[t1]==W_NOWHERE) goto move_player;//Nonexistant
			//Go to board t1 AFTER showing update
			target_board=t1;
			target_where=1;
			target_x=level_color[offs];
			target_y=level_id[offs];
			goto move_player;
			}
		else if(t2&A_ITEM) {
			//Item
			t3=under_player_id;
			t4=under_player_color;
			t5=under_player_param;
			t2=grab_item(new_x,new_y,dir);
			if(t1==123) goto pushy;//Pushable robot
			else if((t2)&&(t1==49)) {
				//Teleporter
				t6=under_player_id;
				t7=under_player_color;
				t8=under_player_param;
				under_player_id=t3;
				under_player_color=t4;
				under_player_param=t5;
				id_remove_top(old_x,old_y);
				under_player_id=t6;
				under_player_color=t7;
				under_player_param=t8;
				player_last_dir=(player_last_dir&240)+dir+1;
				//New player x/y will be found after update !!! maybe fix.
				}
			else if(t2) goto move_player;
			}
		else if(t2&A_UNDER) {//Under
		move_player:
			id_remove_top(old_x,old_y);
			id_place(new_x,new_y,127,0,0);
			player_x=new_x;
			player_y=new_y;
			player_last_dir=(player_last_dir&240)+dir+1;
			exit_func();
			return 0;
			}
		else if((t2&A_ENEMY)||(t2&A_HURTS)) {
			if(t1==61) //Bullet
				if((level_param[offs]>>2)==PLAYER_BULLET) {
					//Player bullet no hurty
					id_remove_top(new_x,new_y);
					goto move_player;
					}
			//Enemy or hurtsy
			dec_counter("HEALTH",id_dmg[t1]);
			play_sfx(21);
			set_mesg("Ouch!");
			if(t2&A_ENEMY) {
				//Kill/move
				id_remove_top(new_x,new_y);
				if(level_id[new_x+new_y*max_bxsiz]!=34)//Not onto goop..
					if(!restart_if_zapped) goto move_player;
				}
			}
		else {
			//Check for push
			if(dir>1) t3=t2&A_PUSHEW;
			else t3=t2&A_PUSHNS;
			if(t3) {//Push
			pushy:
				if(!_push(old_x,old_y,dir,0)) goto move_player;
				}
			}
		//Nothing.
		}
	exit_func();
	return 1;
}

char door_first_movement[8]={ 0,3,0,2,1,3,1,2 };

char grab_item(int x,int y,char dir) {//Dir is for transporter
	int id=level_id[x+y*max_bxsiz];
	int param=level_param[x+y*max_bxsiz];
	int color=level_color[x+y*max_bxsiz];
	int t1,t2,tx,ty,fad=is_faded();
	char old_keyb=curr_table;
	char tmp[81];
	enter_func("grab_item");
	switch(id) {
		case 27://Chest
			if(!(param&15)) {
				play_sfx(40);
				break;
				}
			//Act upon contents
			play_sfx(41);
			t1=((param&240)>>4)*5;//Amount for most things
			switch(param&15) {
				case 1://Key
					t1/=5;//Key color
					if(give_key(t1)) {
						set_mesg("Inside the chest is a key, but you can't carry any more keys!");
						exit_func();
						return 0;
						}
					set_mesg("Inside the chest you find a key.");
					break;
				case 2://Coins
					set_3_mesg("Inside the chest you find ",t1," coins.");
					inc_counter("COINS",t1);
					inc_counter("SCORE",t1);
					break;
				case 3://Life
					t1/=5;
					itoa(t1,tmp,10);//Put as a string
					if(t1>1) set_3_mesg("Inside the chest you find ",t1," free lives.");
					else set_3_mesg("Inside the chest you find ",t1," free life.");
					inc_counter("LIVES",t1);
					break;
				case 4://Ammo
					set_3_mesg("Inside the chest you find ",t1," rounds of ammo.");
					inc_counter("AMMO",t1);
					break;
				case 5://Gems
					set_3_mesg("Inside the chest you find ",t1," gems.");
					inc_counter("GEMS",t1);
					inc_counter("SCORE",t1);
					break;
				case 6://Health
					set_3_mesg("Inside the chest you find ",t1," health.");
					inc_counter("HEALTH",t1);
					break;
				case 7://Potion
					m_show();
					switch_keyb_table(1);
					if(fad) {
						clear_screen(1824,current_pg_seg);
						insta_fadein();
						}
					t2=confirm("Inside the chest you find a potion. Drink it?");
					if(fad) insta_fadeout();
					switch_keyb_table(old_keyb);
					if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
					if(t2) {
						exit_func();
						return 0;
						}
					level_param[x+y*max_bxsiz]=0;
					param=t1/5;
					goto potion_effect;
				case 8://Ring
					m_show();
					switch_keyb_table(1);
					if(fad) {
						clear_screen(1824,current_pg_seg);
						insta_fadein();
						}
					t2=confirm("Inside the chest you find a ring. Wear it?");
					if(fad) insta_fadeout();
					switch_keyb_table(old_keyb);
					if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
					if(t2) {
						exit_func();
						return 0;
						}
					switch_keyb_table(0);
					level_param[x+y*max_bxsiz]=0;
					param=t1/5;
					goto potion_effect;
				case 9://Lobombs
					set_3_mesg("Inside the chest you find ",t1," low strength bombs.");
					inc_counter("LOBOMBS",t1);
					break;
				case 10://Hibombs
					set_3_mesg("Inside the chest you find ",t1," high strength bombs.");
					inc_counter("HIBOMBS",t1);
					break;
				}
			//Empty chest
			level_param[x+y*max_bxsiz]=0;
			break;
		case 28://Gem
		case 29://Magic gem
			play_sfx(id-28);
			inc_counter("GEMS");
			if(id==29) inc_counter("HEALTH");
			inc_counter("SCORE");
			goto remove_item;
		case 30://Health
			play_sfx(2);
			inc_counter("HEALTH",param);
			goto remove_item;
		case 31://Ring
		case 32://Potion
		potion_effect:
			play_sfx(39);
			inc_counter("SCORE",5);
			switch(param) {
				case 0://Dud
					set_mesg("* No effect *");
					break;
				case 1://Invinco
					set_mesg("* Invinco *");
					goto invinco;
				case 2://Blast
					for(t1=0;t1<10000;t1++) {
						if((t2=flags[level_id[t1]])&A_UNDER) {
							if(t2&A_ENTRANCE) continue;
							if(random_num()%50==0) {
								tx=t1%max_bxsiz;
								ty=t1/max_bxsiz;
								if(tx>=board_xsiz) continue;
								if(ty>=board_ysiz) continue;
								id_place(tx,ty,38,0,16*(random_num()%5+2));
								}
							}
						}
					set_mesg("* Blast *");
					break;
				case 3://Health +10
					inc_counter("HEALTH",10);
				case 4://Health +50
					if(param==4) inc_counter("HEALTH",50);
					set_mesg("* Healing *");
					break;
				case 5://Poison
					dec_counter("HEALTH",10);
					set_mesg("* Poison *");
					if(restart_if_zapped) {//Don't move there if could die & move
						id_remove_top(x,y);
						return 0;
						}
					break;
				case 6://Blind
					blind_dur=200;
					set_mesg("* Blind *");
					break;
				case 7://Kill
					for(t1=0;t1<10000;t1++) {
						t2=level_id[t1];
						if((t2>79)&&(t2<96)&&(t2!=92)&&(t2!=93)) //Enemy
							id_remove_top(t1%max_bxsiz,t1/max_bxsiz);
						}
					set_mesg("* Kill enemies *");
					break;
				case 8://Firewalk
					firewalker_dur=200;
					set_mesg("* Firewalking *");
					break;
				case 9://Detonate
					for(t1=0;t1<10000;t1++) {
						if(level_id[t1]==36) {
							level_id[t1]=38;
							if(level_param[t1]==0) level_param[t1]=32;
							else level_param[t1]=64;
							}
						}
					set_mesg("* Detonate *");
					break;
				case 10://Banish
					for(t1=0;t1<10000;t1++) {
						if(level_id[t1]==86) {
							level_id[t1]=85;
							level_param[t1]=51;
							}
						}
					set_mesg("* Banish dragons *");
					break;
				case 11://Summon
					for(t1=0;t1<10000;t1++) {
						t2=level_id[t1];
						if((t2>79)&&(t2<96)&&(t2!=92)&&(t2!=93)) {//Enemy
							level_id[t1]=86;
							level_color[t1]=4;
							level_param[t1]=102;
							}
						}
					set_mesg("* Summon dragons *");
					break;
				case 12://Avalanche
					for(t1=0;t1<10000;t1++) {
						if((t2=flags[level_id[t1]])&A_UNDER) {
							if(t2&A_ENTRANCE) continue;
							if(random_num()%18==0) {
								tx=t1%max_bxsiz;
								ty=t1/max_bxsiz;
								if(tx>=board_xsiz) continue;
								if(ty>=board_ysiz) continue;
								id_place(tx,ty,8,7,0);
								}
							}
						}
					set_mesg("* Avalanche *");
					break;
				case 13://Freeze
					freeze_time_dur=200;
					set_mesg("* Freeze time *");
					break;
				case 14://Wind
					wind_dur=200;
					set_mesg("* Wind *");
					break;
				case 15://Slow
					slow_time_dur=200;
					set_mesg("* Slow time *");
					break;
				}
			if(id==27) {
				exit_func();
				return 0;
				}
			else goto remove_item;
		case 34://Goop
			play_sfx(48);
			set_mesg("There is goop in your way!");
			send_robot_def(0,12);
			break;
		case 33://Energizer
			play_sfx(16);
			set_mesg("Energize!");
		invinco:
			send_robot_def(0,2);
			set_counter("INVINCO",113);
			if(id==27) {
				exit_func();
				return 0;
				}
			goto remove_item;
		case 35://Ammo
			play_sfx(3);
			inc_counter("AMMO",param);
			goto remove_item;
		case 36://Bomb
			if(collect_bombs) {
				if(param) {
					play_sfx(7);
					inc_counter("HIBOMBS");
					goto remove_item;
					}
				play_sfx(6);
				inc_counter("LOBOMBS");
				goto remove_item;
				}
			//Light bomb
			play_sfx(33);
			level_id[x+y*max_bxsiz]=37;
			level_param[x+y*max_bxsiz]=param<<7;
			break;
		case 39://Key
			if(give_key(color)) {
				play_sfx(9);
				set_mesg("You can't carry any more keys!");
				break;
				}
			play_sfx(8);
			set_mesg("You grab the key.");
			goto remove_item;
		case 40://Lock
			if(take_key(color)) {
				play_sfx(11);
				set_mesg("You need an appropriate key!");
				break;
				}
			play_sfx(10);
			set_mesg("You open the lock.");
			goto remove_item;
		case 41://Door
			if(param&8) {//Locked
				if(take_key(color)) {
					//Need key
					play_sfx(19);
					set_mesg("The door is locked!");
					break;
					}
				//Unlocked
				set_mesg("You unlock and open the door.");
				}
			else set_mesg("You open the door.");
			level_id[x+y*max_bxsiz]=42;
			level_param[x+y*max_bxsiz]=(param&7);
			if(_move(x,y,door_first_movement[param&7],1|4|8|16)) {
				set_mesg("The door is blocked from opening!");
				play_sfx(19);
				level_id[x+y*max_bxsiz]=41;
				level_param[x+y*max_bxsiz]=param&7;
				}
			else play_sfx(20);
			break;
		case 47://Gate
			if(param) {//Locked
				if(take_key(color)) {
					//Need key
					play_sfx(14);
					set_mesg("The gate is locked!");
					break;
					}
				//Unlocked
				set_mesg("You unlock and open the gate.");
				}
			else set_mesg("You open the gate.");
			level_id[x+y*max_bxsiz]=48;
			level_param[x+y*max_bxsiz]=22;
			play_sfx(15);
			break;
		case 49://Transport
			if(_transport(x,y,dir,127,0,0,1)) break;
			exit_func();
			return 1;
		case 50://Coin
			play_sfx(4);
			inc_counter("COINS");
			inc_counter("SCORE");
			goto remove_item;
		case 55://Pouch
			play_sfx(38);
			inc_counter("GEMS",(param&15)*5);
			inc_counter("COINS",(param>>4)*5);
			inc_counter("SCORE",((param&15)+(param>>4))*5);
			str_cpy(tmp,"The pouch contains ");
			itoa((param&15)*5,&tmp[19],10);
			str_cat(tmp," gems and ");
			itoa((param>>4)*5,&tmp[str_len(tmp)],10);
			str_cat(tmp," coins.");
			set_mesg(tmp);
			goto remove_item;
		case 65://Forest
			play_sfx(13);
			if(forest_becomes==FOREST_TO_EMPTY) goto remove_item;
			level_id[x+y*max_bxsiz]=15;
			exit_func();
			return 1;
		case 66://Life
			play_sfx(5);
			inc_counter("LIVES");
			set_mesg("You find a free life!");
			goto remove_item;
		case 71://Invis. wall
			level_id[x+y*max_bxsiz]=1;
			set_mesg("Oof! You ran into an invisible wall.");
			play_sfx(12);
			break;
		case 74://Mine
			level_id[x+y*max_bxsiz]=38;
			level_param[x+y*max_bxsiz]=param&240;
			play_sfx(36);
			break;
		case 81://Eye
			level_id[x+y*max_bxsiz]=38;
			level_param[x+y*max_bxsiz]=(param<<1)&112;
			break;
		case 82://Thief
			dec_counter("GEMS",(param&128)>>7);
			play_sfx(44);
			break;
		case 83://Slime
			if(param&64) {
				//Hurtsy
				dec_counter("HEALTH",id_dmg[83]);
				play_sfx(21);
				set_mesg("Ouch!");
				}
			if(param&128) break;//Invinco
			level_id[x+y*max_bxsiz]=6;
			break;
		case 85://Ghost
			//Hurtsy
			dec_counter("HEALTH",id_dmg[85]);
			play_sfx(21);
			set_mesg("Ouch!");
			//Die !?
			if(param&8) break;
			goto remove_item;
		case 86://Dragon
			//Hurtsy
			dec_counter("HEALTH",id_dmg[86]);
			play_sfx(21);
			set_mesg("Ouch!");
			break;
		case 87://Fish
			if(!(param&64)) break;
			//Hurtsy
			dec_counter("HEALTH",id_dmg[87]);
			play_sfx(21);
			set_mesg("Ouch!");
			goto remove_item;
		case 123://Pushable robot
		case 124://Robot
			send_robot_def(param,0);
			break;
		case 125://Sign
		case 126://Scroll
			play_sfx(47);
			switch_keyb_table(1);
			m_show();
			scroll_edit(param,id&1);
			if(get_counter("CURSORSTATE",0) == 0) { m_hide();}
			switch_keyb_table(old_keyb);
			if(id==126) goto remove_item;
			break;
		}
	exit_func();
	return 0;//Not grabbed

remove_item://Code to "pick up" item
	exit_func();
	id_remove_top(x,y);
	return 1;
}

//Show status screen
void show_status(void) {
	int t1;
	char temp[11];
	draw_window_box(37,4,67,21,current_pg_seg,25,16,24);
	show_counter("Gems",39,5);
	show_counter("Ammo",39,6);
	show_counter("Health",39,7);
	show_counter("Lives",39,8);
	show_counter("Lobombs",39,9);
	show_counter("Hibombs",39,10);
	show_counter("Coins",39,11);
	for(t1=0;t1<NUM_STATUS_CNTRS;t1++) //Show custom status counters
		if(status_shown_counters[t1*COUNTER_NAME_SIZE])
			show_counter(&status_shown_counters[t1*COUNTER_NAME_SIZE],39,t1+15,1);
	write_string("Score",39,12,27,current_pg_seg);
	ltoa(score,temp,10);
	write_string(temp,55,12,31,current_pg_seg);//Show score
	write_string("Keys",39,13,27,current_pg_seg);
	for(t1=0;t1<8;t1++)//Show keys
		if(keys[t1]!=NO_KEY) draw_char('',16+keys[t1],55+t1,13,current_pg_seg);
	for(t1=8;t1<16;t1++)//Show keys, 2nd row
		if(keys[t1]!=NO_KEY) draw_char('',16+keys[t1],47+t1,14,current_pg_seg);
	//Show hi/lo bomb selection
	write_string("(cur.)",48,9+bomb_type,25,current_pg_seg);
	//Show effects
	if(firewalker_dur>0) write_string("-W-",43,21,28,current_pg_seg);
	if(freeze_time_dur>0) write_string("-F-",47,21,27,current_pg_seg);
	if(slow_time_dur>0) write_string("-S-",51,21,30,current_pg_seg);
	if(wind_dur>0) write_string("-W-",55,21,31,current_pg_seg);
	if(blind_dur>0) write_string("-B-",59,21,25,current_pg_seg);
}

void show_counter(char far *str,char x,char y,char skip_if_zero) {
	unsigned int t1=get_counter(str);
	if((skip_if_zero)&&(!t1)) return;
	write_string(str,x,y,27,current_pg_seg);//Name
	write_number(t1,31,x+16,y,current_pg_seg,1);//Value
}

//Returns non-0 to skip all keys this cycle
char update(char game,char &fadein) {
	int t1,t2,t3,t4,sv;
	char r,g,b;
	unsigned int time;
	static reload=0;
	static slowed=0;//Flips between 0 and 1 during slow_time
#ifdef BETA
	static unsigned char cyc=0;//Debugging
#endif
	int tmp_x[5];
	int tmp_y[5];
	char tmp_str[10];
	enter_func("update");
	pal_update=0;
	tcycle=0;//For speed setting
	if(fadein) {
		clear_screen(1824,current_pg_seg);
		insta_fadein();
		}
	//Fade mod
	if(volume_inc) {
		t1=volume;
		t1+=volume_inc;
		if(volume_inc<0) {
			if(t1<=volume_target) {
				t1=volume_target;
				volume_inc=0;
				}
			}
		else {
			if(t1>=volume_target) {
				t1=volume_target;
				volume_inc=0;
				}
			}
		volume_mod(volume=t1);
		}
	//Slow_time- flip slowed
	if(freeze_time_dur) slowed=1;
	else if(slow_time_dur) slowed^=1;
	else slowed=0;
	//Update
	update_variables(slowed);
	update_player();//Ice, fire, water, lava
	if(wind_dur>0) {//Wind
		t1=random_num()%9;
		if(t1<4) {/* No wind this turn if above 3*/
			player_last_dir=(player_last_dir&240)+t1;
			move_player(t1);
			}
		}
	//No mouse support on title screen
	if (game) m_snapshot();
	//The following is during gameplay ONLY
	if((game)&&(!dead)) {
		//Shoot
		if(sp_pressed) {
			if(reload) goto no_shoot;
			if(player_attack_locked) goto no_shoot;
			if(up_pressed) t3=0;
			else if(dn_pressed) t3=1;
			else if(rt_pressed) t3=2;
			else if(lf_pressed) t3=3;
			else goto no_shoot;
			if(!can_shoot) set_mesg("Can't shoot here!");
			else if(!get_counter("AMMO")) {
				set_mesg("You are out of ammo!");
				play_sfx(30);
				}
			else {
				dec_counter("AMMO");
				play_sfx(28);
				_shoot_c(player_x,player_y,t3,PLAYER_BULLET);
				reload=2;
				player_last_dir=(player_last_dir&15)|(t3<<4);
				}
			}
		//Move
		else if((up_pressed)&&(!player_ns_locked)) {
			if(up_pressed>0)
				move_player(0);
			if(up_pressed==1) up_pressed=-1;
			else if((up_pressed>-REPEAT_WAIT)&&(up_pressed<0)) up_pressed--;
			else if(up_pressed) up_pressed=2;//In case up was released
			player_last_dir=(player_last_dir&15);
			}
		else if((dn_pressed)&&(!player_ns_locked)) {
			if(dn_pressed>0)
				move_player(1);
			if(dn_pressed==1) dn_pressed=-1;
			else if((dn_pressed>-REPEAT_WAIT)&&(dn_pressed<0)) dn_pressed--;
			else if(dn_pressed) dn_pressed=2;
			player_last_dir=(player_last_dir&15)+16;
			}
		else if((lf_pressed)&&(!player_ew_locked)) {
			if(lf_pressed>0)
				move_player(3);
			if(lf_pressed==1) lf_pressed=-1;
			else if((lf_pressed>-REPEAT_WAIT)&&(lf_pressed<0)) lf_pressed--;
			else if(lf_pressed) lf_pressed=2;
			player_last_dir=(player_last_dir&15)+48;
			}
		else if((rt_pressed)&&(!player_ew_locked)) {
			if(rt_pressed>0)
				move_player(2);
			if(rt_pressed==1) rt_pressed=-1;
			else if((rt_pressed>-REPEAT_WAIT)&&(rt_pressed<0)) rt_pressed--;
			else if(rt_pressed) rt_pressed=2;
			player_last_dir=(player_last_dir&15)+32;
			}
	no_shoot:
		//Bomb
		if(del_pressed) {
			if(player_attack_locked) goto no_bomb;
			t1=player_x+player_y*max_bxsiz;
			if(level_under_id[t1]!=37) {
				//Okay.
				if(!can_bomb) set_mesg("Can't bomb here!");
				else if((bomb_type==0)&&(!get_counter("LOBOMBS"))) {
					set_mesg("You are out of low strength bombs!");
					play_sfx(32);
					}
				else if((bomb_type==1)&&(!get_counter("HIBOMBS"))) {
					set_mesg("You are out of high strength bombs!");
					play_sfx(32);
					}
				else if(!(flags[level_under_id[t1]]&A_ENTRANCE)) {
					//Bomb!
					under_player_id=level_under_id[t1];
					under_player_color=level_under_color[t1];
					under_player_param=level_under_param[t1];
					level_under_id[t1]=37;
					level_under_color[t1]=8;
					level_under_param[t1]=bomb_type<<7;
					play_sfx(33+bomb_type);
					if(bomb_type) dec_counter("HIBOMBS");
					else dec_counter("LOBOMBS");
					}
				}
			}
	no_bomb:
		if(reload) reload--;
		}
	//Global robot BEFORE other stuff
    //  mynewx= scroll_x;
    //  mynewy= scroll_y;

	run_robot(NUM_ROBOTS,-1,-1);
	if(!slowed) {
		if(flags[level_under_id[player_x+player_y*max_bxsiz]]&A_ENTRANCE) t1=0;
		else t1=1;//Not on an entrance yet...
		was_zapped=0;
		enter_funcn("update_screen",2);
		update_screen();
		exit_func();
		//Pushed onto an entrance?
		if((t1)&&(flags[level_under_id[(t2=player_x+player_y*max_bxsiz)]]&A_ENTRANCE)
		 &&(!was_zapped)) {
			clear_sfx_queue();//Since there is often a push sound
			play_sfx(37);
			t1=level_under_param[t2];
			//Same board or nonexistant?
			if((t1!=curr_board)&&(board_where[t1]!=W_NOWHERE)) {
				//Nope.
				//Go to board t1
				target_board=t1;
				target_where=1;
				target_x=level_under_color[t2];
				target_y=level_under_id[t2];
				}
			}
		was_zapped=0;
		}
	//Death and game over
	if(get_counter("LIVES")==0) {//Game over
		if(endgame_board!=NO_ENDGAME_BOARD) {
			//Jump to given board
			if(curr_board==endgame_board) {
				id_remove_top(player_x,player_y);
				id_place(endgame_x,endgame_y,127,0,0);
				player_x=endgame_x;
				player_y=endgame_y;
				}
			else {
				target_board=endgame_board;
				target_where=0;
				target_x=endgame_x;
				target_y=endgame_y;
				}
			//Clear "endgame" part
			endgame_board=NO_ENDGAME_BOARD;
			//Give one more life with minimal health
			set_counter("LIVES",1);
			set_counter("HEALTH",1);
			}
		else {
			set_mesg("Game over");
			b_mesg_row=24;
			b_mesg_col=255;
			if(game_over_sfx) play_sfx(24);
			dead=1;
			}
		}
	else if(get_counter("HEALTH")==0) {/* Death */
		set_counter("HEALTH",starting_health);
		dec_counter("Lives");
		set_mesg("You have died...");
		clear_sfx_queue();
		play_sfx(23);
		//Go somewhere else?
		if(death_board==DEATH_SAME_POS) ;
		else if(death_board==NO_DEATH_BOARD) {
			//Return to entry x/y
			id_remove_top(player_x,player_y);
			id_place(player_restart_x,player_restart_y,127,0,0);
			player_x=player_restart_x;
			player_y=player_restart_y;
			}
		else {
			//Jump to given board
			if(curr_board==death_board) {
				id_remove_top(player_x,player_y);
				id_place(death_x,death_y,127,0,0);
				player_x=death_x;
				player_y=death_y;
				}
			else {
				target_board=death_board;
				target_where=0;
				target_x=death_x;
				target_y=death_y;
				}
			}
		}
	//Select OTHER page for seg
	if(target_where!=-1) {
		current_pg_seg=VIDEO_NUM2SEG(!current_page);
		if(fadein) insta_fadeout();
		//Draw border
		draw_viewport();
		//Draw screen
		if(!game) {
			*player_color=0;
			player_char[0]=player_char[1]=player_char[2]=player_char[3]=32;
			}
		//Figure out x/y of top
		calculate_xytop(t1,t2);
		if(blind_dur>0) {
			for(t3=viewport_y;t3<viewport_y+viewport_ysiz;t3++)
				fill_line(viewport_xsiz,viewport_x,t3,2224,current_pg_seg);
			//Find where player would be and draw.
			if(game) id_put(player_x-t1+viewport_x,player_y-t2+viewport_y,
				player_x,player_y,player_x,player_y,current_pg_seg);
			}
		else {
			enter_funcn("draw_game_window",3);
			draw_game_window(t1,t2,current_pg_seg);
			exit_func();
			}
#ifdef BETA
		//Debugging
		write_hex_byte(++cyc,15,0,0,current_pg_seg);
#endif
		//Add time limit
		time=get_counter("TIME");
		if(time) {
			t1=edge_color*16+15;
			if(edge_color==15) t1=240;//Prevent white on white for timer
			itoa(time/60,tmp_str,10);//Minutes
			write_string(tmp_str,1,24,t1,current_pg_seg);
			t2=1+str_len(tmp_str);
			draw_char(':',t1,t2,24,current_pg_seg);
			itoa(time%60,tmp_str,10);//Seconds
			if(str_len(tmp_str)==2) write_string(tmp_str,t2+1,24,t1,current_pg_seg);
			else {//If only one digit in seconds, display an extra '0'
				t2++;
				draw_char('0',t1,t2,24,current_pg_seg);
				write_string(tmp_str,t2+1,24,t1,current_pg_seg);
				}
			//Border with spaces
			draw_char(' ',t1,t2+1+str_len(tmp_str),24,current_pg_seg);
			draw_char(' ',t1,0,24,current_pg_seg);
			}
		//Add message
		if(b_mesg_timer>0) {
			if(b_mesg_col==255) t1=40-(str_len_color(bottom_mesg)>>1);
			else t1=b_mesg_col;
			color_string(bottom_mesg,t1,b_mesg_row,scroll_color,current_pg_seg);
			if((t1>0)&&(mesg_edges)) draw_char(' ',scroll_color,t1-1,b_mesg_row,current_pg_seg);
			t1+=str_len_color(bottom_mesg);
			if((t1<80)&&(mesg_edges)) draw_char(' ',scroll_color,t1,b_mesg_row,current_pg_seg);
			}
		//Add debug box
		if(debug_mode) {
			debug_x=65;
			draw_debug_box(16);
			//X/Y
			write_number(player_x,EC_DEBUG_NUMBER,72,17,current_pg_seg,3);
			write_number(player_y,EC_DEBUG_NUMBER,76,17,current_pg_seg,3);
			}
    // Add SPRITES! - Exo
    draw_sprites();
		//Page flip
		current_page=1-current_page;
		//Page flip automatically doesn't occur until NEXT retrace.
		//But we wait until then so we don't start writing on the
		//visible page!
		enter_funcn("page_flip",4);
		page_flip(current_page);
		ec_update_set_if_needed();
		exit_func();
		m_vidseg(current_pg_seg);
		}
	//Retrace/speed wait
	if(overall_speed>1) {
		enter_funcn("wait_retrace",5);
		wait_retrace();
		exit_func();
		if(pal_update) update_palette(0);
		enter_funcn("(update delay)",6);
		while(tcycle<(overall_speed-1)*8);
		exit_func();
	} else {
		if(pal_update) update_palette();
		ec_update_set_if_needed();
	}
	if(fadein) {
		vquick_fadein();
		fadein=0;
	}
	if(target_board>=0) {
		int faded=is_faded();
		//Aha.. TELEPORT or ENTRANCE.
		//Destroy message, bullets, spitfire?
		if(clear_on_exit) {
			b_mesg_timer=0;
			for(t1=0;t1<(board_xsiz+board_ysiz*max_bxsiz);t1++) {
				t2=level_id[t1];
				if((t2==78)||(t2==61)) offs_remove_id(t1,0);
				}
			}
		//Save last dir
		sv=player_last_dir;
		//Fade out.
		if(target_where>=0) {
			if(!faded) vquick_fadeout();
			//Prepare screen for any meters
			//Clear screen
			clear_screen(1824,current_pg_seg);
			//Palette (set black to black to mask the fact that we AREN'T faded
			//in)
			get_rgb(0,r,g,b);
			set_rgb(0,0,0,0);
			insta_fadein();
			}
		//Load board
		under_player_id=under_player_param=0;
		under_player_color=7;
		swap_with(target_board);
		set_counter("TIME",time_limit);
		//Find target x/y
		if(target_where==1) {
			//Entrance
			//First, set searched for id to the first in the whirlpool
			//series if it is part of the whirlpool series
			if((target_y>67)&&(target_y<71)) target_y=67;
			//Now scan. Order- (type==target_y, color==target_x)
			//	1) Same type & color
			// 2) Same color
			// 3) Same type & foreground
			// 4) Same foreground
			// 5) Same type
			//Search for first of all 5 at once
			for(t1=0;t1<5;t1++) tmp_x[t1]=-1;//None found
			//t3==Offset
			if(level_id[player_x+player_y*max_bxsiz]==127)
				id_remove_top(player_x,player_y);//So can find beneath player
			for(t2=0;t2<board_ysiz;t2++) {//Y pos
				t3=t2*max_bxsiz;
				for(t1=0;t1<board_xsiz;t1++,t3++) {//X pos
					t4=level_id[t3];
					if((t4>67)&&(t4<71)) t4=67;
					if(t4==target_y) {
						//Same type
						//Color match?
						if(level_color[t3]==target_x) {
							//Yep
							tmp_x[0]=t1; tmp_y[0]=t2;
							}
						//Fg?
						if((level_color[t3]&15)==(target_x&15)) {
							//Yep
							tmp_x[2]=t1; tmp_y[2]=t2;
							}
						else {//Just same type
							tmp_x[4]=t1; tmp_y[4]=t2;
							}
						}
					else if(flags[t4]&A_ENTRANCE) {
						//Not same type, but an entrance
						//Color match?
						if(level_color[t3]==target_x) {
							//Yep
							tmp_x[1]=t1; tmp_y[1]=t2;
							}
						//Fg?
						else if((level_color[t3]&15)==(target_x&15)) {
							//Yep
							tmp_x[3]=t1; tmp_y[3]=t2;
							}
						}
					//Done with this x/y
					}
				}
			//We've got it... maybe.
			for(t1=0;t1<5;t1++) if(tmp_x[t1]>=0) break;
			if(t1<5) {
				//Goto tmp_x[t1],tmp_y[t1]
				player_x=tmp_x[t1];
				player_y=tmp_y[t1];
				}
			//Else we stay at default player position
			id_place(player_x,player_y,127,0,0);
			}
		else {
			//Specified x/y
			if(target_x<0) target_x=0;
			if(target_y<0) target_y=0;
			if(target_x>=board_xsiz) target_x=board_xsiz-1;
			if(target_y>=board_ysiz) target_y=board_ysiz-1;
			if((target_x!=player_x)||(target_y!=player_y)) {
				if(level_id[player_x+player_y*max_bxsiz]==127)
					id_remove_top(player_x,player_y);
				player_x=target_x;
				player_y=target_y;
				t1=player_x+player_y*max_bxsiz;
				if((level_id[t1]==123)||(level_id[t1]==124))
					clear_robot(level_param[t1]);
				if((level_id[t1]==125)||(level_id[t1]==126))
					clear_scroll(level_param[t1]);
				if(level_id[t1]==122) step_sensor(level_param[t1]);
				id_place(player_x,player_y,127,0,0);
				}
			}
		send_robot_def(0,11);
		player_restart_x=player_x;
		player_restart_y=player_y;
		//Now... Set player_last_dir for direction FACED
		player_last_dir=(player_last_dir&15)+(sv&240);
		//...and if player ended up on ICE, set last dir pressed as well
		if(level_under_id[player_x+player_y*max_bxsiz]==25)
			player_last_dir=sv;
		//Fix palette
		if(target_where>=0) {
			insta_fadeout();
			set_rgb(0,r,g,b);
			//Prepare for fadein
			if(!faded) fadein=1;
			}
		target_x=target_y=target_board=-1;
		target_where=-2;
		//Disallow any keypresses this cycle
		exit_func();
		return 1;
		}
	exit_func();
	return 0;
}

void find_player(void) {
	int t1;
	enter_func("find_player");
	//Find player
	asm {										//Find NEW player pos
		mov bx,SEG level_id           //Load global data seg
		mov ds,bx
		les di,[ds:level_id]         	//Id loc
		mov al,127                		//What to find
		mov cx,10001              	 	//# of locs to search
		cld
		repne scasb
		jcxz oopsy
		mov ax,10000
		sub ax,cx                  	//DI is 0-9999 for offset
		mov bx,[max_bxsiz]           	//What to div by
		xor dx,dx
		div bx                     	//AL=y AH=x
		mov [player_x],dx
		mov [player_y],ax          	//Save new x/y
		}
	exit_func();
	return;
oopsy:
	player_x=player_y=0;
	t1=level_id[0];
	if((t1==123)||(t1==124)) //(robot)
		clear_robot(level_param[0]);
	else if((t1==125)||(t1==126)) //(scroll)
		clear_scroll(level_param[0]);
	//Place.
	id_place(0,0,127,0,0);
	exit_func();
}
