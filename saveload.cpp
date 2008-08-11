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

// Saving/loading worlds- dialogs and functions
// *** READ THIS!!!! ****
/* New magic strings
   .MZX files:
   MZX - Ver 1.x MegaZeux
   MZ2 - Ver 2.x MegaZeux
   MZA - Ver 2.51S1 Megazeux
   M\002\011 - 2.5.1spider2+, 2.9.x

   .SAV files:
   MZSV2 - Ver 2.x MegaZeux
   MZXSA - Ver 2.51S1 MegaZeux
   MZS\002\011 - 2.5.1spider2+, 2.9.x
 
 All others are unchanged.

*/






#include "helpsys.h"
#include "sfx.h"
#include "scrdisp.h"
#include "window.h"
#include "meter.h"
#include "error.h"
#include "saveload.h"
#include "window.h"
#include "ezboard.h"
#include "boardmem.h"
#include <stdio.h>
#include "const.h"
#include "data.h"
#include "string.h"
#include "password.h"
#include "egacode.h"
#include "palette.h"
#include "struct.h"
#include "roballoc.h"
#include "ems.h"
#include "counter.h"
#define SAVE_INDIVIDUAL
char sd_types[3]={ DE_INPUT,DE_BUTTON,DE_BUTTON };
char sd_xs[3]={ 5,15,37 };
char sd_ys[3]={ 2,4,4 };
char far *sd_strs[3]={ "Save world as: ","OK","Cancel" };
int sd_p1s[3]={ FILENAME_SIZE-1,0,1 };
int sd_p2s=193;
void far *cf_ptr=(void far *)(curr_file);
dialog s_di={ 10,8,69,14,"Save World",3,sd_types,sd_xs,sd_ys,sd_strs,sd_p1s,
	&sd_p2s,&cf_ptr,0 };

static int world_magic(const char magic_string[3]) {
	if ( magic_string[0] == 'M' ) {
		if ( magic_string[1] == 'Z' ) {
			switch (magic_string[2]) {
			  case 'X':
				return 0x0100;
			  case '2':
				return 0x0205;
			  case 'A':
				return 0x0209;
			  default:
				return 0;
			}
		} else {
			if ( ( magic_string[1] > 1 ) && ( magic_string[1] < 10 ) ) { // I hope to God that MZX doesn't last beyond 9.x
				return ( (int)magic_string[1] << 8 ) + (int)magic_string[2];
			} else {
				return 0;
			}
		}
	} else {
		return 0;
	}
}

static int save_magic(const char magic_string[5]) {
	if ( ( magic_string[0] == 'M' ) && ( magic_string[1] == 'Z' ) ) {
		switch (magic_string[2]) {
		  case 'S':
			if ( ( magic_string[3] == 'V' ) && ( magic_string[4] == '2' ) ) {
				return 0x0205;
			} else if ( ( magic_string[3] >= 2 ) && ( magic_string[3] <= 10 ) ) {
				return ( (int)magic_string[3] << 8 ) + magic_string[4];
			} else {
				return 0;
			}
		  case 'X':
			if ( ( magic_string[3] == 'S' ) && ( magic_string[4] == 'A' ) ) {
				return 0x0209;
			} else {
				return 0;
			}
		  default:
			return 0;
		}
	} else {
		return 0;
	}
}

char save_world_dialog(void) {
	set_context(76);
	char t1=run_dialog(&s_di,current_pg_seg);
	pop_context();
	return t1;
}

char save_game_dialog(void) {
	int t1;
	set_context(96);
	sd_strs[0]="Save game as: ";
	s_di.title="Save Game";
	cf_ptr=(void far *)(curr_sav);
	t1=run_dialog(&s_di,current_pg_seg);
	pop_context();
	sd_strs[0]="Save world as: ";
	s_di.title="Save World";
	cf_ptr=(void far *)(curr_file);
	return t1;
}

//Saves the world as an edit file (current board MUST be stored first)
//Set savegame to non-zero for a .SAV file and set faded to faded status
void save_world(char far *file,char savegame,char faded) {
	int t1,t2,t3,nmb;//nmb==number of boards
	unsigned char xor;
	unsigned long temp,temp2,temp3;
	char r,g,b;
	int meter_target=2,meter_curr=0;//Percent meter-
											  // 1 for global robot
											  // 1 per board
											  // 1 for other info
	//Count boards...
	nmb=1;
	for(t1=0;t1<NUM_BOARDS;t1++)
		if(board_where[t1]!=W_NOWHERE) nmb=t1+1;
	meter_target+=nmb;
	FILE *fp=fopen(file,"wb");
	if(fp==NULL) {
		error("Error saving world",1,24,current_pg_seg,0x0C01);
		return;
	}
	//Save it...
	save_screen(current_pg_seg);
	meter("Saving...",current_pg_seg,meter_curr,meter_target);
	if (savegame) {
		fwrite("MZS\002\011",1,5,fp);
		fputc(curr_board,fp);
		xor=0;
	} else {
		//Name of game-
		fwrite(board_list,1,BOARD_NAME_SIZE,fp);
		//Pw info-
		write_password(fp);
		//File type id-
		fwrite("M\002\011",1,3,fp);
		//Get xor code...
		xor=get_pw_xor_code();
	}
	//Rest of file is xor encoded. Write character set...
	if(xor) mem_xor((char far *)curr_set,3584,xor);
	fwrite(curr_set,1,3584,fp);
	if(xor) mem_xor((char far *)curr_set,3584,xor);
	//Idchars array...
	if(xor) mem_xor((char far *)id_chars,455,xor);
	fwrite(id_chars,1,455,fp);
	if(xor) mem_xor((char far *)id_chars,455,xor);
	//Status counters...
	if(xor) mem_xor(status_shown_counters,NUM_STATUS_CNTRS*COUNTER_NAME_SIZE,
		xor);
	fwrite(status_shown_counters,COUNTER_NAME_SIZE,NUM_STATUS_CNTRS,fp);
	if(xor) mem_xor(status_shown_counters,NUM_STATUS_CNTRS*COUNTER_NAME_SIZE,
		xor);
	//DATA.ASM info... (define SAVE_INDIVIDUAL to save individually...)
#ifndef SAVE_INDIVIDUAL
	if(savegame) {
		fwrite(keys,1,54+NUM_KEYS,fp);
		fputc(scroll_base_color,fp);
		fputc(scroll_corner_color,fp);
		fputc(scroll_pointer_color,fp);
		fputc(scroll_title_color,fp);
		fputc(scroll_arrow_color,fp);
		fwrite(real_mod_playing,1,FILENAME_SIZE,fp);
	}
	if(xor) mem_xor((char far *)&edge_color,24,xor);
	fwrite(&edge_color,1,24,fp);
	if(xor) mem_xor((char far *)&edge_color,24,xor);
#else
	if(savegame) {
		fwrite(keys,1,NUM_KEYS,fp);
		fwrite(&score,4,1,fp);
		fputc(blind_dur,fp);
		fputc(firewalker_dur,fp);
		fputc(freeze_time_dur,fp);
		fputc(slow_time_dur,fp);
		fputc(wind_dur,fp);
		fwrite(pl_saved_x,2,8,fp);
		fwrite(pl_saved_y,2,8,fp);
		fwrite(pl_saved_board,1,8,fp);
		fputc(saved_pl_color,fp);
		fputc(under_player_id,fp);
		fputc(under_player_color,fp);
		fputc(under_player_param,fp);
		fputc(mesg_edges,fp);
		fputc(scroll_base_color,fp);
		fputc(scroll_corner_color,fp);
		fputc(scroll_pointer_color,fp);
		fputc(scroll_title_color,fp);
		fputc(scroll_arrow_color,fp);
		fwrite(real_mod_playing,1,FILENAME_SIZE,fp);
	}
	fputc(edge_color^xor,fp);
	fputc(first_board^xor,fp);
	fputc(endgame_board^xor,fp);
	fputc(death_board^xor,fp);
	fputc((endgame_x&255)^xor,fp);
	fputc((endgame_x>>8)^xor,fp);
	fputc((endgame_y&255)^xor,fp);
	fputc((endgame_y>>8)^xor,fp);
	fputc(game_over_sfx^xor,fp);
	fputc((death_x&255)^xor,fp);
	fputc((death_x>>8)^xor,fp);
	fputc((death_y&255)^xor,fp);
	fputc((death_y>>8)^xor,fp);
	fputc((starting_lives&255)^xor,fp);
	fputc((starting_lives>>8)^xor,fp);
	fputc((lives_limit&255)^xor,fp);
	fputc((lives_limit>>8)^xor,fp);
	fputc((starting_health&255)^xor,fp);
	fputc((starting_health>>8)^xor,fp);
	fputc((health_limit&255)^xor,fp);
	fputc((health_limit>>8)^xor,fp);
	fputc(enemy_hurt_enemy^xor,fp);
	fputc(clear_on_exit^xor,fp);
	fputc(only_from_swap^xor,fp);
#endif
	//Palette...
	for(t1=0;t1<16;t1++) {
		get_rgb(t1,r,g,b);
		if(xor) {
			r^=xor;
			g^=xor;
			b^=xor;
			}
		fputc(r,fp);
		fputc(g,fp);
		fputc(b,fp);
		}
	if(savegame) {
		//Intensity palette
		for(t1=0;t1<16;t1++)
			fputc(get_color_intensity(t1),fp);
		fputc(faded,fp);
		fwrite(&player_restart_x,2,1,fp);
		fwrite(&player_restart_y,2,1,fp);
		fputc(under_player_id,fp);
		fputc(under_player_param,fp);
		fputc(under_player_color,fp);
		//Counters
      // Here I took out all the old stuff + just made megazeux save all
      // 17k of the 1000 counters, ha ha ha. Spid
      // Actually, the above is wrong :), I just fixed it so that the fputc(t2,fp) is now a
      // fwrite(), and it will write out the # of counters properly (thanx ment&Kev!) Spid

		t2=0;
		for(t1=0;t1<NUM_COUNTERS;t1++)
			if((counters[t1].counter_value)>0) t2=t1+1;
       //		fputc(t2,fp);
      //      fprintf(fp, "%d", t2);
	    fwrite(&t2, sizeof(int), 1, fp);  //THIS is the # of counters, as an int. Spid
		for(t1=0;t1<t2;t1++)
			fwrite(&counters[t1],1,sizeof(Counter),fp);



//            for (t1=0;t1<NUM_COUNTERS;t1++) //Write out every last counter Spid
//                  fwrite(&counters[t1],1,sizeof(Counter),fp);

		}
	//Save space for global robot pos.
	temp3=ftell(fp);
	fputc(0,fp);
	fputc(0,fp);
	fputc(0,fp);
	fputc(0,fp);
	//Sfx
	if(custom_sfx_on) {
		fputc(xor,fp);
		temp=ftell(fp);//Save pos to store word
		fputc(0,fp);
		fputc(0,fp);
		//Write ea. sfx
		for(t3=t1=0;t1<NUM_SFX;t1++) {
			fputc((t2=(str_len(&custom_sfx[t1*69])+1))^xor,fp);
			if(xor) mem_xor(&custom_sfx[t1*69],t2,xor);
			fwrite(&custom_sfx[t1*69],1,t2,fp);
			if(xor) mem_xor(&custom_sfx[t1*69],t2,xor);
			t3+=t2+1;
			}
		//Go back and write word
		temp2=ftell(fp);
		fseek(fp,temp,SEEK_SET);
		fputc((t3&255)^xor,fp);
		fputc((t3>>8)^xor,fp);
		fseek(fp,temp2,SEEK_SET);
		}
	//Write number of boards
	fputc(nmb^xor,fp);
	//Write position of global robot... (right after board position info)
	temp=ftell(fp);
	temp2=temp+(BOARD_NAME_SIZE+8)*nmb;
	if(xor) mem_xor((char far *)&temp2,4,xor);
	fseek(fp,temp3,SEEK_SET);
	fwrite(&temp2,4,1,fp);
	fseek(fp,temp,SEEK_SET);
	//Write board list
	if(xor) mem_xor(board_list,nmb*BOARD_NAME_SIZE,xor);
	fwrite(board_list,BOARD_NAME_SIZE,nmb,fp);
	if(xor) mem_xor(board_list,nmb*BOARD_NAME_SIZE,xor);
	//Write board offsets/lengths (temp keeps track of where to put info)
	//First we need to reserve room for the global robot's struct and program,
	//then place them after it...
	temp=ftell(fp);
	temp+=nmb*8;
	temp+=sizeof(Robot)+robots[GLOBAL_ROBOT].program_length;
	//Cycle through boards...
	for(t1=0;t1<nmb;t1++) {
		if(board_where[t1]==W_NOWHERE) {
			temp2=0;
			if(xor) mem_xor((char far *)&temp2,4,xor);
			fwrite(&temp2,4,1,fp);
			fwrite(&temp2,4,1,fp);
			}
		else {
			//Write board size...
			temp2=board_sizes[t1];
			if(xor) mem_xor((char far *)&temp2,4,xor);
			fwrite(&temp2,4,1,fp);
			//Write current offset...
			temp2=temp;
			if(xor) mem_xor((char far *)&temp2,4,xor);
			fwrite(&temp2,4,1,fp);
			//Increase offest.
			temp+=board_sizes[t1];
			}
		}
	meter_interior(current_pg_seg,++meter_curr,meter_target);
	//Write global robot- struct...
	robots[GLOBAL_ROBOT].used=1;
	if(xor) mem_xor((char far *)&robots[GLOBAL_ROBOT],sizeof(Robot),xor);
	fwrite(&robots[GLOBAL_ROBOT],sizeof(Robot),1,fp);
	if(xor) mem_xor((char far *)&robots[GLOBAL_ROBOT],sizeof(Robot),xor);
	//...then program.
	prepare_robot_mem(1);
	if(xor) mem_xor((char far *)&robot_mem[robots[GLOBAL_ROBOT].program_location],
		robots[GLOBAL_ROBOT].program_length,xor);
	fwrite(&robot_mem[robots[GLOBAL_ROBOT].program_location],1,
		robots[GLOBAL_ROBOT].program_length,fp);
	if(xor) mem_xor((char far *)&robot_mem[robots[GLOBAL_ROBOT].program_location],
		robots[GLOBAL_ROBOT].program_length,xor);
	meter_interior(current_pg_seg,++meter_curr,meter_target);
	//Now write boards.
	for(t1=0;t1<nmb;t1++) {
		if(board_where[t1]!=W_NOWHERE)
			disk_board(t1,fp,0,xor);
		meter_interior(current_pg_seg,++meter_curr,meter_target);
		}
	//...All done!
	fclose(fp);
	restore_screen(current_pg_seg);
}

//Loads the world from an edit file (current board should be stored first)
//Clears all prev. board mem and global robot for storing.
//Set edit to 1 to mean this is in the editor (IE stricter pw controls)
//Set edit to 2 or higher to load the file no matter what.
//Returns non-0 if error
//Set savegame to 1 for a .SAV file, then modifys *faded
//set edit to -1 to mean swap world; ie for a game but ignore password if
//it is the same as the current.
char load_world(char far *file,char edit,char savegame,char *faded) {
	int version;
	int t1,t2,nmb;
	unsigned char xor;
	unsigned long temp,temp2,gl_rob;
	char tempstr[16];
	int p_m;
	char r,g,b;
	int meter_target=2,meter_curr=0;//Percent meter-
											  // 1 for global robot
											  // 1 per board
											  // 1 for other info
	FILE *fp=fopen(file,"rb");
	if(fp==NULL) {
		error("Error loading world",1,24,current_pg_seg,0x0D01);
		return 1;
		}
	//Load it...
	save_screen(current_pg_seg);
	meter("Loading...",current_pg_seg,meter_curr,meter_target);
	if(savegame) {
                fread(tempstr,1,5,fp);
		version = save_magic(tempstr);
		if ( version != 0x0209 ) {
			restore_screen(current_pg_seg);
			if (!version) {
				error(".SAV files from other versions of MZX are not supported",1,24,current_pg_seg,0x2101);
			} else {
				error("Unrecognized magic: file may not be .SAV file",1,24,current_pg_seg,0x2101);
			}
			fclose(fp);
			return 1;
		}
		curr_board=fgetc(fp);
		xor=0;
		refresh_mod_playing = 1;
	} else {
		//Name of game- skip it.
		fseek(fp,BOARD_NAME_SIZE,SEEK_CUR);
		//Pw info- Save current...
		p_m=protection_method;
		str_cpy(tempstr,password);
		//...get new...
		read_password(fp);
		if((edit==-1)&&(!str_cmp(tempstr,password))) goto pw_okay;
		//...pw check if needed...
		if(edit<2) {
			if((protection_method==NO_PLAYING)||((protection_method==NO_EDITING)&&(edit==1))) {
				if(check_pw()) {
					//Invalid- restore old info...
					protection_method=p_m;
					str_cpy(password,tempstr);
					//...close file...
					fclose(fp);
					//...and exit.
					restore_screen(current_pg_seg);
					return 1;
					}
				//Correct password, continue. Forget about old pw info.
				}
			}
	pw_okay:
		{
			char magic[3];
			char error_string[80];
			fread(magic,1,3,fp);
			version = world_magic(magic);
			if ( version < 0x0205 ) {
				sprintf(error_string, "World is from old version (%d.%d); use converter", ( version & 0xff00 ) >> 8, version & 0xff);
				version = 0;
			} else if ( version > 0x0209 ) {
				sprintf(error_string, "World is from more recent version (%d.%d)", ( version & 0xff00 ) >> 8, version & 0xff);
				version = 0;
			} else if ( version == 0 ) {
				sprintf(error_string, "Unrecognized magic; file may not be MZX world");
			}
			if (!version) {
				restore_screen(current_pg_seg);
				error(error_string,1,24,current_pg_seg,0x0D02);
				protection_method=p_m;
				str_cpy(password,tempstr);
				fclose(fp);
				restore_screen(current_pg_seg);
				return 1;
			}
		}
		//Get xor code...
		xor=get_pw_xor_code();
	}
	//Version 2.00
	//Clear gl robot...
	clear_robot(GLOBAL_ROBOT);
	//Clear boards...
	for(t1=0;t1<NUM_BOARDS;t1++) {
		deallocate_board_space(t1);
		board_sizes[t1]=0;
		board_list[t1*BOARD_NAME_SIZE]=0;
		}
	//Rest of file is xor encoded. Read character set...
	fread(curr_set,1,3584,fp);
	if(xor) mem_xor((char far *)curr_set,3584,xor);
	ec_update_set();
	//Idchars array...
	fread(id_chars,1,455,fp);
	if(xor) mem_xor((char far *)id_chars,455,xor);
	//Status counters...
	fread(status_shown_counters,COUNTER_NAME_SIZE,NUM_STATUS_CNTRS,fp);
	if(xor) mem_xor(status_shown_counters,NUM_STATUS_CNTRS*COUNTER_NAME_SIZE,
		xor);
	//DATA.ASM info... (define SAVE_INDIVIDUAL to load individually...)
#ifndef SAVE_INDIVIDUAL
	if(savegame) {
		fread(keys,1,54+NUM_KEYS,fp);
		scroll_base_color=fgetc(fp);
		scroll_corner_color=fgetc(fp);
		scroll_pointer_color=fgetc(fp);
		scroll_title_color=fgetc(fp);
		scroll_arrow_color=fgetc(fp);
                fread(real_mod_playing,1,FILENAME_SIZE,fp);
	}
	fread(&edge_color,1,24,fp);
	if(xor) mem_xor((char far *)&edge_color,24,xor);
#else
	if(savegame) {
		fread(keys,1,NUM_KEYS,fp);
		fread(&score,4,1,fp);
		blind_dur=fgetc(fp);
		firewalker_dur=fgetc(fp);
		freeze_time_dur=fgetc(fp);
		slow_time_dur=fgetc(fp);
		wind_dur=fgetc(fp);
		fread(pl_saved_x,2,8,fp);
		fread(pl_saved_y,2,8,fp);
		fread(pl_saved_board,1,8,fp);
		saved_pl_color=fgetc(fp);
		under_player_id=fgetc(fp);
		under_player_color=fgetc(fp);
		under_player_param=fgetc(fp);
		mesg_edges=fgetc(fp);
		scroll_base_color=fgetc(fp);
		scroll_corner_color=fgetc(fp);
		scroll_pointer_color=fgetc(fp);
		scroll_title_color=fgetc(fp);
		scroll_arrow_color=fgetc(fp);
                fread(real_mod_playing,1,FILENAME_SIZE,fp);
	}
	edge_color=fgetc(fp)^xor;
	first_board=fgetc(fp)^xor;
	endgame_board=fgetc(fp)^xor;
	death_board=fgetc(fp)^xor;
	endgame_x=fgetc(fp)^xor;
	endgame_x+=(fgetc(fp)^xor)<<8;
	endgame_y=fgetc(fp)^xor;
	endgame_y+=(fgetc(fp)^xor)<<8;
	game_over_sfx=fgetc(fp)^xor;
	death_x=fgetc(fp)^xor;
	death_x+=(fgetc(fp)^xor)<<8;
	death_y=fgetc(fp)^xor;
	death_y+=(fgetc(fp)^xor)<<8;
	starting_lives=fgetc(fp)^xor;
	starting_lives+=(fgetc(fp)^xor)<<8;
	lives_limit=fgetc(fp)^xor;
	lives_limit+=fgetc(fp)^xor<<8;
	starting_health=fgetc(fp)^xor;
	starting_health+=fgetc(fp)^xor<<8;
	health_limit=fgetc(fp)^xor;
	health_limit+=fgetc(fp)^xor<<8;
	enemy_hurt_enemy=fgetc(fp)^xor;
	clear_on_exit=fgetc(fp)^xor;
	only_from_swap=fgetc(fp)^xor;
#endif
	//Palette...
	for(t1=0;t1<16;t1++) {
		r=fgetc(fp)^xor;
		g=fgetc(fp)^xor;
		b=fgetc(fp)^xor;
		set_rgb(t1,r,g,b);
		}
	if(savegame) {
		for(t1=0;t1<16;t1++)
			set_color_intensity(t1,fgetc(fp));
		*faded=fgetc(fp);
		fread(&player_restart_x,2,1,fp);
		fread(&player_restart_y,2,1,fp);
		under_player_id=fgetc(fp);
		under_player_color=fgetc(fp);
		under_player_param=fgetc(fp);
            //Again, it's fixed so that it will read the whole integer
           
       //		t2=fgetc(fp);
	    fread(&t2, sizeof(int), 1, fp);
		for(t1=0;t1<t2;t1++)
			fread(&counters[t1],1,sizeof(Counter),fp);
		if(t1<NUM_COUNTERS)
			for(;t1<NUM_COUNTERS;t1++)
				counters[t1].counter_value=0;
		}
	update_palette();
	//Get position of global robot...
	fread(&gl_rob,4,1,fp);
	if(xor) mem_xor((char far *)&gl_rob,4,xor);
	//Get number of boards
	nmb=fgetc(fp)^xor;
	if(nmb==0) {
		//Sfx
		custom_sfx_on=1;
		fgetc(fp);//Skip size word
		fgetc(fp);
		//Read sfx
		for(t1=0;t1<NUM_SFX;t1++) {
			t2=fgetc(fp)^xor;
			fread(&custom_sfx[t1*69],1,t2,fp);
			}
		if(xor) mem_xor(custom_sfx,NUM_SFX*69,xor);
		nmb=fgetc(fp)^xor;
		}
	else custom_sfx_on=0;
	meter_target+=nmb;
	meter_interior(current_pg_seg,++meter_curr,meter_target);
	//Read board list
	fread(board_list,BOARD_NAME_SIZE,nmb,fp);
	if(xor) mem_xor(board_list,nmb*BOARD_NAME_SIZE,xor);
	//Per board- Read size. Read offset, jump there, read board.
	//  Jump back, next board. temp remembers place within board
	//  size/loc list.
	//Cycle through boards...
	for(t1=0;t1<nmb;t1++) {
		//Read board size...
		fread(&temp2,4,1,fp);
		if(xor) mem_xor((char far *)&temp2,4,xor);
		board_sizes[t1]=temp2;
		//Read offset
		fread(&temp2,4,1,fp);
		if(xor) mem_xor((char far *)&temp2,4,xor);
		//Is there a board?
		if(board_sizes[t1]) {
			//First try to allocate room
			if(allocate_board_space(board_sizes[t1],t1))
				error("Out of memory and/or disk space",2,4,current_pg_seg,0x0204);
			//Remember current place
			temp=ftell(fp);
			//Jump to the board
			fseek(fp,temp2,SEEK_SET);
			//and undisk it
			disk_board(t1,fp,1,xor);
			//Now return to within list
			fseek(fp,temp,SEEK_SET);
			}
			//Nope. Do nothing (no board, already clear)
		//Loop for next board
		meter_interior(current_pg_seg,++meter_curr,meter_target);
		}
	//Read global robot
	fseek(fp,gl_rob,SEEK_SET);
	//Struct... (save mem offset/size)
	t1=robots[GLOBAL_ROBOT].program_location;
	t2=robots[GLOBAL_ROBOT].program_length;
	fread(&robots[GLOBAL_ROBOT],sizeof(Robot),1,fp);
	if(xor) mem_xor((char far *)&robots[GLOBAL_ROBOT],sizeof(Robot),xor);
	robots[GLOBAL_ROBOT].program_location=t1;
	t1=robots[GLOBAL_ROBOT].program_length;
	robots[GLOBAL_ROBOT].program_length=t2;
	robots[GLOBAL_ROBOT].used=1;
	//Reallocate to t2 long
	if(reallocate_robot_mem(T_ROBOT,GLOBAL_ROBOT,t1)) {
		error("Out of robot memory",1,21,current_pg_seg,0x0504);
		//They chose fail... no loady loady... :)
		}
	else {
		//...then program.
		prepare_robot_mem(1);
		fread(&robot_mem[robots[GLOBAL_ROBOT].program_location],1,
			robots[GLOBAL_ROBOT].program_length,fp);
		if(xor) mem_xor((char far *)&robot_mem[robots[GLOBAL_ROBOT].program_location],
			robots[GLOBAL_ROBOT].program_length,xor);
		meter_interior(current_pg_seg,++meter_curr,meter_target);
		}
	//...All done!
	fclose(fp);
	restore_screen(current_pg_seg);
	return 0;
}
