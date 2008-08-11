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

/* EZ-Board management code */

#include "timer.h"
#include "scrdisp.h"
#include "string.h"
#include "beep.h"
#include "string.h"
#include "mod.h"
#include "roballoc.h"
#include "egacode.h"
#include "error.h"
#include "boardmem.h"
#include "data.h"
#include "const.h"
#include "struct.h"
#include "ezboard.h"
#include "palette.h"
#include "game.h"
#include "counter.h"

// The way boards work-
// There is the current board, in memory, and all other boards, in memory.
// They are stored in conventional, EMS, or temp files. The current board
// has a code of W_CURRENT and NO allocated memory.

// On startup, before any other board-related code, call board_setup. Since
// nothing is allocated, it simply clears all the arrays for use and sets
// up the current board.
void board_setup(void) {
	int t1;
	//Clear all board-related arrays
	for(t1=0;t1<NUM_BOARDS;t1++) {
		board_where[t1]=W_NOWHERE;//Doesn't exist
		board_offsets[t1].offset=0;//No offset
		board_filenames[t1*FILENAME_SIZE]=0;//No filename
		board_list[t1*BOARD_NAME_SIZE]=0;//No name
		board_sizes[t1]=0;//No size
		}
	//Select first as active
	board_where[curr_board=0]=W_CURRENT;//Both methods of determining current
	//Still has no offest, filename, size, or name. Offset, filename, and
	//size only relate to storage in memory, and name is undefined yet.
	//Done.
}

// Now boards are in the required normalized setup format. :)
// Access the current board in any way normally allowed. To store the current
// board into memory, use this function. It will leave the board in current
// memory as well. It stores it in id curr_board. Note that changing to a new
// current board is important, since the board_where[] array no longer has
// W_CURRENT for the "current" board. Error message produced if run when
// a board is already stored there. Error message uses current_pg_seg.

void store_current(void) {
	//Get size of current board
	long size=size_of_current_board();
	//Are we already stored? (err- exit or help only)
	if((board_where[curr_board]!=W_CURRENT)&&
		(board_where[curr_board]!=W_NOWHERE))
		error("Attempt to overwrite board in storage",2,20,current_pg_seg,
			0x0101);
	//Ok. Allocate space for new board- check for error
	if(allocate_board_space(size,curr_board))
		error("Out of memory and/or disk space",2,4,
			current_pg_seg,0x0201);
	board_sizes[curr_board]=size;
	//Space allocated.
	//Now we must store!
	if(store_current(curr_board))
		error("Error storing current board",2,20,current_pg_seg,0x0301);
	//Stored!
}

// Now we might want to clear the current board... (not neccesary if the
// NEXT function is going to be used...) This also sets id to be the current
// board. If id has space allocated to it, the board is deleted. This is used
// usually to add a board. Rundown- Clear current board and select id as
// current, using the newly cleared board. If adopt_settings is on, the
// board_info settings from the prev. current board are kept. (viewport
// and board size and most other local settings)

void clear_current_and_select(unsigned char id,char adopt_settings) {
	//If board to become current exists, delete it (works fine if it
	//is already current);
	if(board_where[id]!=W_NOWHERE)
		deallocate_board_space(id);
	//Select as current and clear size
	board_where[curr_board=id]=W_CURRENT;
	board_sizes[id]=0;
	//Clear board and settings
	clear_current(adopt_settings);
	//Done!
}

// We also need to be able to load ANOTHER board into current. This will,
// of course, only work if the current board is already stored here. Since
// we can't tell for sure if the current board has been stored, this is not
// checked for. The current board is simply overwritten by the board
// specified by id. Errors include loading a nonexistant board and running
// out of robot memory. (due to current and global objects) This fully
// selects the board as current, including setting board_where[], curr_board,
// and deallocating any space allocated to the board. This function also
// searches for the player and sets player_x and player_y appropriately.

void select_current(unsigned char id) {
	int t1;
	char temp[FILENAME_SIZE];
	//Save current mod...
	str_cpy(temp,real_mod_playing);
	//Does board exist?
	if(board_where[id]==W_NOWHERE)
		error("Attempt to load nonexistent board",2,20,current_pg_seg,0x0401);
	//If board is current, we're done
	if(board_where[id]==W_CURRENT) return;
	//Otherwise, load it in...
	if((t1=grab_current(id))!=0) {
		//Out of robot memory or other error?
		if(t1==4) //Out of robot memory
			error("Out of robot memory",1,21,current_pg_seg,0x0501);
			//They chose FAIL. We'll continue with robots possibly missing...
		else //Out of work space
			error("Out of memory and/or disk space",2,4,
				current_pg_seg,0x0202);
		}
	//Loaded. Deallocate...
	deallocate_board_space(id);
	//...and select as current.
	board_where[curr_board=id]=W_CURRENT;
	board_sizes[id]=0;
	//Mods-
	if( str_cmp(mod_playing,"*") && str_cmp(mod_playing,temp) ) {
		//Different mod.
		if(mod_playing[0]==0) end_mod();
		else {
			str_cpy(temp,mod_playing);
			load_mod(temp);
			volume_mod(volume);
		}
	}
	find_player();
	//Done!
}

// Often we simply need to swap the current board with another. That is
// taken care of simply using the next function.

void swap_with(unsigned char id) {
	if(id==curr_board) return;//Why bother?
	store_current();
	select_current(id);
}

// This takes care of most important board functions. We also want to be
// able to delete a board... This function deallocates all space assigned
// to the noted board and marks it as empty. You cannot delete the title
// screen, and deleting the current board first swaps to the title screen.

void delete_board(unsigned char id) {
	//Simply deallocate board and clear name in board_list.
	if(id==0) return;//Can't delete title screen !
	if(id==curr_board) swap_with(0);
	if(board_where[id]==W_NOWHERE) return;//No board to clear
	//Deallocate...
	deallocate_board_space(id);
	//...set board_sizes...
	board_sizes[id]=0;
	//...and clear board_list.
	board_list[id*BOARD_NAME_SIZE]=0;
	//Done!
}

// It would also be nice to have a clear board function that just cleared
// the current board without marking up the board list.. IE clear the floor
// plan and settings, etc. The board_info settings are not cleared if
// adopt_settings is on.

void clear_current(char adopt_settings) {
	int t1;
	//Stop any mod IF not adopting settings
	if(!adopt_settings) {
		end_mod();
		mod_playing[0]=0;
		real_mod_playing[0]=0;
	}
	//Clear page
	for(t1=0;t1<10000;t1++) {
		level_id[t1]=level_under_id[t1]=level_param[t1]=level_under_param[t1]=0;
		level_color[t1]=level_under_color[t1]=overlay_color[t1]=7;
		overlay[t1]=32;
		}
	//Place player
	level_id[0]=127; level_color[0]=27; player_x=player_y=0;
	//Clear robots (NOT temp or global)
	for(t1=1;t1<NUM_ROBOTS;t1++) clear_robot(t1);
	//Clear scrolls (NOT temp)
	for(t1=1;t1<NUM_SCROLLS;t1++) clear_scroll(t1);
	//Clear sensors (NOT temp)
	for(t1=1;t1<NUM_SENSORS;t1++) clear_sensor(t1);
	//Clear settings
	if(!adopt_settings) {
		viewport_x=3; viewport_y=2;
		viewport_xsiz=74; viewport_ysiz=21;
		board_xsiz=100;
		board_ysiz=50;
		max_bsiz_mode=2;
		max_bxsiz=max_bysiz=100;
		can_shoot=can_bomb=fire_burn_space=fire_burn_fakes=fire_burn_trees=
			collect_bombs=1;
		fire_burn_brown=0;
		explosions_leave=EXPL_LEAVE_ASH;
		save_mode=CAN_SAVE;
		forest_becomes=FOREST_TO_FLOOR;
		fire_burns=FIRE_BURNS_LIMITED;
		}
	restart_if_zapped=time_limit=last_key=num_input=
		input_size=input_string[0]=bottom_mesg[0]=
		b_mesg_timer=scroll_x=scroll_y=0;
	player_last_dir=16;
	b_mesg_row=24;
	b_mesg_col=255;
	locked_x=locked_y=65535;
	overlay_mode=OVERLAY_OFF;
	//Clear board exits
	for(t1=0;t1<4;t1++) board_dir[t1]=NO_BOARD;
	//DON'T clear board title
	//Done!
}

// And of course, a clean sweep purging function is always helpful... IE one
// that clears EVERYTHING. This one will do the trick.

void clear_world(char clear_curr_file) {
	int t1;
	//Select board 0 as current
	deallocate_board_space(0);
	board_where[curr_board=0]=W_CURRENT;
	//Give it no name and no size
	board_list[0]=0;
	board_sizes[0]=0;
	//Deallocate all other boards
	for(t1=1;t1<NUM_BOARDS;t1++) delete_board(t1);
	//Clear current board
	clear_current(0);
	//Clear global robot
	clear_robot(GLOBAL_ROBOT);
	robots[GLOBAL_ROBOT].used=1;
	//Clear all other global parameters
	mem_cpy((char far *)id_chars,(char far *)def_id_chars,324);
	mem_cpy((char far *)bullet_color,(char far *)def_id_chars+324,3);
	mem_cpy((char far *)id_dmg,(char far *)(def_id_chars+327),128);
	missile_color = 8;

	first_board=clear_on_exit=endgame_x=endgame_y=death_x=
		death_y=only_from_swap=protection_method=password[0]=
		enemy_hurt_enemy=0;
	if(clear_curr_file) curr_file[0]=0;
	endgame_board=NO_ENDGAME_BOARD;
	death_board=NO_DEATH_BOARD;
	lives_limit=99;
	health_limit=200;
	starting_lives=7;
	starting_health=100;
	for(t1=0;t1<NUM_STATUS_CNTRS*COUNTER_NAME_SIZE;t1++)
		status_shown_counters[t1]=0;
	edge_color=8;
	//Load in default mzx character set and palette
	ec_load_mzx();
	default_palette();
	//Done!
}

// This next function clears all game-related counters and other parameters.
// Run this only in game-related situations- the affected parameters are only
// used and saved in game situations, not editing situations. The parameters
// are also not saved in any way in a MZX or MZB file, only SAV files.

void clear_game_params(void) {
	int t1;
	//Clear all global game parameters
	for(t1=0;t1<NUM_KEYS;t1++) keys[t1]=NO_KEY;
	score=quicksave_file[0]=volume_inc=volume_target=player_ns_locked=
		player_ew_locked=player_attack_locked=blind_dur=firewalker_dur=
		freeze_time_dur=slow_time_dur=wind_dur=under_player_id=
		under_player_param=0;
	volume=255;
	scroll_color=15;
	saved_pl_color=29;
	under_player_color=7;
	mesg_edges=1;
	for(t1=0;t1<8;t1++)
		pl_saved_x[t1]=pl_saved_y[t1]=pl_saved_board[t1]=0;
	//Clear counters
	for(t1=0;t1<NUM_COUNTERS;t1++)
		counters[t1].counter_name[0]=counters[t1].counter_value=0;
	//Set all nine built-in counters
	str_cpy(counters[0].counter_name,"GEMS");
	str_cpy(counters[1].counter_name,"AMMO");
	str_cpy(counters[2].counter_name,"LOBOMBS");
	str_cpy(counters[3].counter_name,"HIBOMBS");
	str_cpy(counters[4].counter_name,"COINS");
	str_cpy(counters[5].counter_name,"LIVES");
	str_cpy(counters[6].counter_name,"HEALTH");
	str_cpy(counters[7].counter_name,"TIME");
	str_cpy(counters[8].counter_name,"INVINCO");
	counters[5].counter_value=starting_lives;
	counters[6].counter_value=starting_health;
	counters[7].counter_value=time_limit;
	//Scroll colors
	scroll_base_color=143;
	scroll_corner_color=135;
	scroll_pointer_color=128;
	scroll_title_color=143;
	scroll_arrow_color=142;
	//Done!
	player_restart_x=player_x;
	player_restart_y=player_y;
	for(t1=0;t1<16;t1++)
		set_color_intensity(t1,100);
}

// We don't need a cleanup function- clear_world handles this nicely. It only
// leaves robot memory allocated, which is a seperate entity that is
// deallocated anyways. A nice function would be one to clear current
// objects, those numbered 0, for use within the editing fields.

void clear_zero_objects(void) {
	//Clear the three zero objects
	clear_robot(0);
	clear_scroll(0);
	clear_sensor(0);
}
