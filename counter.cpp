/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
 * Copyright (C) 1999 Charles Goetzman
 * Copyright (C) 2002 B.D.A - Koji_takeo@worldmailer.com
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


#include <time.h>
#include <dos.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>

#include "counter.h"

#include "bwsb.h"
#include "egacode.h"
#include "admath.h"
#include "blink.h"
#include "const.h"
#include "cursor.h"
#include "data.h"
#include "game.h"
#include "game2.h"
#include "graphics.h"
#include "idarray.h"
#include "idput.h"
#include "mouse.h"
#include "palette.h"
#include "runrobot.h"
#include "sfx.h"
#include "string.h"
#include "struct.h"
#include "conio.h"

long offset;
char fileio;

// I took away the mzxakversion stuff. So you no longer
// have to set "mzxakversion" to 1. -Koji

//Temp vars for new funcitons
int dir,dirx,diry,loc;

// Date and Time data
struct time t;
struct date d;

void set_built_in_messages(int param);

int player_restart_x=0,player_restart_y=0,gridxsize=1,gridysize=1;
int myscrolledx=0,myscrolledy=0;
//int MZXAKWENDEVERSION = 0;
//Some added variables to speed up pixel editing -Koji
unsigned char pixel_x = 0, pixel_y = 0;
//if (input_file != NULL)
//	fclose(input_file);
//if (output_file != NULL)
//	fclose(output_file);

char was_zapped=0;
extern char mid_prefix;
void prefix_xy(int&fx,int&fy,int&mx,int&my,int&lx,int&ly,int robotx,
	int roboty);

//Get a counter. Include robot id if a robot is running.
int get_counter(char far *name,unsigned char id) {
	int t1;

	//a few counters convert to var's for speed.
	if(!str_cmp(name,"CHAR_X")) return pixel_x;
	if(!str_cmp(name,"CHAR_Y")) return pixel_y;
	if(!str_cmp(name,"PLAYERDIST")) return abs((robots[id].xpos-player_x)
						  +(robots[id].ypos-player_y));
	//REAL distance -Koji
	// Can only handle real distances up to 129.
	// Can't seem to help that.
	if(!str_cmp(name,"R_PLAYERDIST"))
	{
		long d;
		d= (long)((robots[id].xpos-player_x)
			 *(robots[id].xpos-player_x));
		d+=(long)((robots[id].ypos-player_y)
			 *(robots[id].ypos-player_y));
		if (d < 16384)
			return sqrt(d);
		else return 129;
	}

	//Reads a single byte of a char -Koji
	if(!str_cmp(name,"CHAR_BYTE"))
		return *(curr_set + (get_counter("CHAR") * 14) + (get_counter("BYTE") % 14));

	//Reads a single pixel - Koji
	// I was really tired the night I was trying to make this.
	// and Exophase practically gave me the source
	// I would've made one myself the next day, but it's already done.
	if(!str_cmp(name,"PIXEL"))
	{
		int pixel_mask;
		char sub_x, sub_y, current_byte;
		unsigned char current_char;
		sub_x = pixel_x % 8;
		sub_y = pixel_y % 14;
		current_char = ((pixel_y / 14) * 32) + (pixel_x / 8);
		current_byte = *(curr_set+(current_char*14)+sub_y);
		pixel_mask = (128 >> sub_x);
		return (current_byte & pixel_mask) >> (7 - sub_x);
	}
	//Read binary positions of "INT" ("bit_place" = 0-15) - Koji
	if(!str_cmp(name,"INT2BIN"))
	{
		int place = (get_counter("BIT_PLACE") & 15);
		return (get_counter("INT") & (1 << place)) >> place;
	}

	//Open input_file! -Koji
	if(!str_cmp(name,"FREAD_OPEN"))
	{
		fileio = 1;
		return 0;
	}

	//opens output_file as normal. -Koji
	if(!str_cmp(name,"FWRITE_OPEN"))
	{
		fileio = 2;
		return 0;
	}

	//Opens output_file as APPEND. -Koji
	if(!str_cmp(name,"FWRITE_APPEND"))
	{
		fileio = 3;
		return 0;
	}

	//Reads from input_file. -Koji
	if(!str_cmp(name,"FREAD"))
	{
		if(input_file == NULL) return -1;
		return fgetc(input_file);
	}

	//Ok, ok. I did steal the "pos" within "page" idea from Akwende.
	//It is a great idea to be able to read more than 32767 bytes.
	//	-Koji
	if(!str_cmp(name,"FREAD_POS"))
	{
		if(input_file == NULL) return -1;
		return (ftell(input_file) % 32767);
	}

	if(!str_cmp(name,"FREAD_PAGE"))
	{
		if(input_file == NULL) return -1;
		return (ftell(input_file) / 32767);
	}

	if(!str_cmp(name,"FWRITE_POS"))
	{
		if(output_file == NULL) return -1;
		return (ftell(output_file) % 32767);
	}

	if(!str_cmp(name,"FWRITE_PAGE"))
	{
		if(output_file == NULL) return -1;
		return (ftell(output_file) / 32767);
	}
	if(!str_cmp(name,"FREAD_LENGTH"))
	{
		if(input_file == NULL) return -1;
		return filelength(fileno(input_file));
	}

	if(!str_cmp(name,"FWRITE_LENGTH"))
	{
		if(output_file == NULL) return -1;
		return filelength(fileno(output_file));
	}


//	}if(MZXAKWENDEVERSION == 1)
//	{
		if(!str_cmp(name,"OVERLAY_MODE"))
			return overlay_mode;
//		if(!str_cmp(name,"SMZX_MODE"))
//			return smzx_mode;
		if(!str_cmp(name,"DATE_DAY"))
		{
			getdate(&d);
			return d.da_day;
		}
		if(!str_cmp(name,"DATE_YEAR"))
		{
			getdate(&d);
			return d.da_year;
		}
		if(!str_cmp(name,"DATE_MONTH"))
		{
			getdate(&d);
			return d.da_mon;
		}
		if(!str_cmp(name,"TIME_SECONDS"))
		{
			gettime(&t);
			return t.ti_sec;
		}
		if(!str_cmp(name,"TIME_MINUTES"))
		{
			gettime(&t);
			return t.ti_min;
		}
		if(!str_cmp(name,"TIME_HOURS"))
		{
			gettime(&t);
			return t.ti_hour;
		}
//	}

	if(!str_cmp(name,"LOOPCOUNT"))
		return robots[id].loop_count;

	if(!str_cmp(name,"LOCAL"))
		return robots[id].blank;

	if(!str_cmp(name,"MZX_SPEED"))
		return overall_speed;

	//First, check for robot specific counters
	if(!str_cmp(name,"BULLETTYPE"))
		return robots[id].bullet_type;

	if(!str_cmp(name,"HORIZPLD"))
		return abs(robots[id].xpos-player_x);

	if(!str_cmp(name,"VERTPLD"))
		return abs(robots[id].ypos-player_y);

	if(!str_cmp(name,"THISX"))
	{
		if(mid_prefix<2) return robots[id].xpos;
		else if(mid_prefix==2) return robots[id].xpos-player_x;
		else return robots[id].xpos-get_counter("XPOS");
	}
	if(!str_cmp(name,"THISY"))
	{
		if(mid_prefix<2) return robots[id].ypos;
		else if(mid_prefix==2) return robots[id].ypos-player_y;
		else return robots[id].ypos-get_counter("YPOS");
	}
	//Next, check for global, non-standard counters
	if(!str_cmp(name,"INPUT")) 	 	  return num_input;
	if(!str_cmp(name,"INPUTSIZE")) 	  return input_size;
	if(!str_cmp(name,"KEY")) 		 	  return last_key;
	if(!str_cmp(name,"SCORE")) 	 	  return (unsigned int)score;
	if(!str_cmp(name,"TIMERESET")) 	  return time_limit;
	if(!str_cmp(name,"PLAYERFACEDIR")) return player_last_dir>>4;
	if(!str_cmp(name,"PLAYERLASTDIR")) return player_last_dir&15;
	// Mouse info Spid
	if(!str_cmp(name,"MOUSEX")) return saved_mouse_x;
	if(!str_cmp(name,"MOUSEY")) return saved_mouse_y;
	if(!str_cmp(name,"BUTTONS")) return saved_mouse_buttons;
		// scroll_x and scroll_y never work, always return 0. Spid
	if(!str_cmp(name,"SCROLLEDX"))
	{
		calculate_xytop(myscrolledx,myscrolledy);
		return myscrolledx;
	}

	if(!str_cmp(name,"SCROLLEDY"))
	{
		calculate_xytop(myscrolledx,myscrolledy);
		return myscrolledy;
	}
	// Player position Spid
	if(!str_cmp(name,"PLAYERX")) return player_x;
	if(!str_cmp(name,"PLAYERY")) return player_y;

	if(!str_cmp(name,"MBOARDX"))
	{
		calculate_xytop(myscrolledx,myscrolledy);
		return (mousex + myscrolledx - viewport_x);
	}
	if(!str_cmp(name,"MBOARDY"))
	{
		  calculate_xytop(myscrolledx,myscrolledy);
		  return (mousey + myscrolledy - viewport_y);
	}
	// Functions added :by Akwende
	// Adds Time Counters, and access to internal Robot ID

//	if(MZXAKWENDEVERSION == 1)
//	{
//		if(!str_cmp(name,"ROBOT_ID"))
//			return id;

		if(!str_cmp(name,"OVERLAY_CHAR"))
		{
			if (overlay_mode == 0) return 0;
			loc = get_counter("OVERLAY_X",id);
			if(loc > board_xsiz)
			  return 0;
			loc += (get_counter("OVERLAY_Y",id) * board_xsiz);
			if(loc > (board_xsiz * board_ysiz))
			  return 0;

			return overlay[loc];
		}
		if(!str_cmp(name,"OVERLAY_COLOR"))
		{
			if (overlay_mode == 0) return 0;
			loc = get_counter("OVERLAY_X",id);
			if(loc > board_xsiz)
			  return 0;

			loc += (get_counter("OVERLAY_Y",id) * board_xsiz);
			if(loc > (board_xsiz * board_ysiz))
			  return 0;
			return overlay_color[loc];
		}

		if(!str_cmp(name,"BOARD_CHAR"))
		{
			loc = get_counter("BOARD_X",id);
			if(loc > board_xsiz)
				return 0;

			loc += (get_counter("BOARD_Y",id) * board_xsiz);

			if(loc > (board_xsiz * board_ysiz))
			  return 0;

			return get_id_char(loc);
		}
		if(!str_cmp(name,"BOARD_COLOR"))
		{
			loc = get_counter("BOARD_X",id);

			if(loc > board_xsiz)
				return 0;
			loc += (get_counter("BOARD_Y",id) * board_xsiz);

			if(loc > (board_xsiz * board_ysiz))
			  return 0;
			return level_color[loc];
		}

		if(!str_cmp(name,"RED_VALUE"))
		{
			return get_Color_Aspect(get_counter("CURRENT_COLOR",id),0);
		}
		if(!str_cmp(name,"GREEN_VALUE"))
		{
			return get_Color_Aspect(get_counter("CURRENT_COLOR",id),1);
		}
		if(!str_cmp(name,"BLUE_VALUE"))
		{
			return get_Color_Aspect(get_counter("CURRENT_COLOR",id),2);
		}
		if(!str_cmp(name,"BULLET_TYPE"))
		{
			return robots[id].bullet_type;
		}
		//My_target messed with the can_lavawalk local
		//I'll just make it into a new local with that name -Koji
		if(!str_cmp(name,"LAVA_WALK"))
		{
			return robots[id].can_lavawalk;
		}
		if(!str_cmp(name,"THIS_CHAR"))
			return robots[id].robot_char;

		if(!str_cmp(name,"THIS_COLOR"))
			return level_color[robots[id].ypos * board_xsiz + robots[id].xpos];

    if(!str_cmp(name, "MOD_ORDER"))
      return MusicOrder(0xFF);

/*
		if(!str_cmp(name,"GET_TARGET_ID"))
		{
			loc = get_counter("BOARD_X",id);

			if(mid_prefix==1)
			{
				loc+= robots[id].xpos;
				loc += ((get_counter("BOARD_Y",id)+ robots[id].ypos) * board_xsiz);
			}
			else
			{
				if(mid_prefix==2)
				{
					loc+= player_x;
					loc += ((get_counter("BOARD_Y",id)+ player_y) * board_xsiz);
				}
				else
				{
					int temp = get_counter("YPOS");
					loc+= temp;
					loc+= ((get_counter("BOARD_Y",id)+ temp) * board_xsiz);
				}
			}

			if ((level_id[loc] == 123) || (level_id[loc] ==124))
				return level_param[loc];
			else
			return 0;
		}
		if(!str_cmp(name,"TARGET_X"))
		{
			return robots[robots[id].can_lavawalk].xpos;
		}

		if(!str_cmp(name,"TARGET_Y"))
			return robots[robots[id].can_lavawalk].ypos;

		if(!str_cmp(name,"TARGET_XDIR"))
		{
			dir =	robots[robots[id].can_lavawalk].xpos - robots[id].xpos;
			if (dir == 0) return 0;
			return dir/abs(dir);
		}

		if(!str_cmp(name,"TARGET_YDIR"))
		{
			dir =	robots[robots[id].can_lavawalk].ypos - robots[id].ypos;
			if (dir == 0) return 0;
			return dir/abs(dir);
		}

		if(!str_cmp(name,"TARGET_DIST"))
		{
			diry =	robots[robots[id].can_lavawalk].ypos;
			diry -= robots[id].ypos;

			dirx =	robots[robots[id].can_lavawalk].xpos;
			dirx = robots[id].xpos;

			return abs(dirx) + abs(diry);
		}*/
//	}
	//Now search counter structs
	for(t1=0;t1<NUM_COUNTERS;t1++)
		if(!str_cmp(name,counters[t1].counter_name)) break;
	if(t1<NUM_COUNTERS) return counters[t1].counter_value;
	return 0;//Not found
}


//Sets the value of a counter. Include robot id if a robot is running.
void set_counter(char far *name,int value,unsigned char id) {
	int t1;
	//File protocal -Koji
	if(fileio > 0)
	{       //This can be good thing or a bad thing...
		//I'd say it'd be wise to seclude a new game
		//inside a it's own folder to fight against
		//a posible viral threat. -Koji
		if(fileio == 1)
		{
			//read
			if(input_file != NULL)
				fclose(input_file);
			input_file = fopen(name,"rb");
		}

		if(fileio == 2)
		{
			//write
			if(output_file != NULL)
				fclose(output_file);
			output_file = fopen(name,"wb");
		}

		if(fileio == 3)
		{
			//Append
			if(output_file != NULL)
				fclose(output_file);
			output_file = fopen(name,"ab");
		}

		fileio = 0;
		return;
	}

	//Ok, ok. I did steal the "pos" within "page" idea from Akwende.
	//It is a great idea to be able to read more than 32767 bytes.
	// The pages are limited to 256, so The max size of a file is
	// about 8MB
	//	-Koji
	if(!str_cmp(name,"FREAD_POS"))
	{
		if(input_file == NULL)
			return;
		offset = 32767 * get_counter("FREAD_PAGE") + value;
		fseek(input_file,offset,0);
	}

	if(!str_cmp(name,"FREAD_PAGE"))
	{
		value %= 256;
		if(input_file == NULL)
			return;
		offset = 32767 * value + get_counter("FREAD_POS");
		fseek(input_file,offset,0);
	}

	if(!str_cmp(name,"FWRITE_POS"))
	{
		if(output_file == NULL)
			return;
		offset = 32767 * get_counter("FWRITE_PAGE") + value;
		fseek(output_file,offset,0);
	}

	if(!str_cmp(name,"FWRITE_PAGE"))
	{
		value %= 256;
		if(output_file == NULL)
			return;
		offset = 32767 * value + get_counter("FWRITE_POS");
		fseek(output_file,offset,0);
	}

	//Writes to the output_file. -Koji
	if(!str_cmp(name,"FWRITE"))
	{
		if(output_file == NULL)
			return;
		fputc(value % 256,output_file);
		return;
	}

	if(!str_cmp(name,"CHAR_X")) {
			pixel_x = value;
			return;
			}
	if(!str_cmp(name,"CHAR_Y")) {
			pixel_y = value;
			return;
			}
	//Writes to a single byte of a char -koji
	if(!str_cmp(name,"CHAR_BYTE")) {
			*(curr_set + (get_counter("CHAR") * 14) + (get_counter("BYTE") % 14)) = value;
			return;
			}
		//Writes to a pixel -Koji
		if(!str_cmp(name,"PIXEL"))
		{
			char sub_x, sub_y, current_byte;
			unsigned char current_char;
			pixel_x = abs(pixel_x % 256);
			pixel_y = abs(pixel_y % 112);
			sub_x = pixel_x % 8;
			sub_y = pixel_y % 14;
			current_char = ((pixel_y / 14) * 32) + (pixel_x / 8);
			current_byte = ec_read_byte(current_char, sub_y);
			if(value == 0)
			{
				//clear
				current_byte &= ~(128 >> sub_x);
				*(curr_set + (current_char * 14) + (sub_y % 14)) = (current_byte % 256);
			}
			else if(value == 1)
			{
				//set
				current_byte |= (128 >> sub_x);
				*(curr_set + (current_char * 14) + (pixel_y % 14)) = (current_byte % 256);
			}
			else
			{
				//toggle
				current_byte ^= (128 >> sub_x);
				*(curr_set + (current_char * 14) + (pixel_y % 14)) = (current_byte % 256);
			}
			return;
		}

		//Writes a single bit to a given bit place of "INT" -Koji
		//(bit place = 0-15)
		if(!str_cmp(name,"INT2BIN"))
		{
			unsigned int integer;
			int place;
			place = ((get_counter("BIT_PLACE") % 16) * (-1)) + 15;
			integer = get_counter("INT") + 32768;
			if(value % 3 == 0)
			{
				//clear
				integer &= ~(32768 >> place);
				set_counter("INT", integer - 32768);
			}
			else if(value % 3 == 1)
			{
				//set
				integer |= (32768 >> place);
				set_counter("INT", integer - 32768);
			}
			else
			{
				//toggle
				integer ^= (32768 >> place);
				set_counter("INT", integer - 32768);
			}
		}

	//First, check for robot specific counters
/*	if(!str_cmp(name,"MZXAKVERSION"))
	{
		MZXAKWENDEVERSION = value % 2;
		return;
	}
*/
	//Functions added by Akwende
	//ABS Value Function
//	if(MZXAKWENDEVERSION == 1)
//	{

		if(!str_cmp(name,"ABS_VALUE"))
		{
			if (value < 0 ) value = value * -1;
			set_counter("VALUE",value,id);
			return;
		}
		if(!str_cmp(name,"SQRT_VALUE"))
		{
		  set_counter("VALUE",sqrt(value),id);
		  return;
		}
		//Akwende tried to make another overlay... Tried! -koji
	/*	if(!str_cmp(name,"OVERLAY_MODE_4"))
		{
			overlay_mode = 4;
		}
	*/
/*		if(!str_cmp(name,"SMZX_MODE"))
		{
			smzx_mode = (char)value % 3;

			if(smzx_mode==1)
			{
			smzx_14p_mode();
			cursor_off();
			blink_off();
			ec_update_set();
			//init_palette();
			smzx_update_palette(0);
			}
			else
			{
				if(smzx_mode==1)
				{
				ega_14p_mode();
				cursor_off();
				blink_off();
				ec_update_set();
				update_palette(0);
				}
				else
				{
				ega_14p_mode();
				cursor_off();
				blink_off();
				ec_update_set();
				update_palette(0);
				}
			}
		}*/


		if(!str_cmp(name,"RED_VALUE"))
		{
			value = value % 64;
			set_Color_Aspect(get_counter("CURRENT_COLOR",id),0,value);
			return;
		}
		if(!str_cmp(name,"GREEN_VALUE"))
		{
			value = value % 64;
			set_Color_Aspect(get_counter("CURRENT_COLOR",id),1,value);
			return;
		}
		if(!str_cmp(name,"BLUE_VALUE"))
		{
			value = value % 64;
			set_Color_Aspect(get_counter("CURRENT_COLOR",id),2,value);
			return;
		}
/*              //Silly Akwende we already have a variable that does
		//This. It's called "bullettype" -Koji
		if(!str_cmp(name,"BULLET_TYPE"))
		{
			robots[id].bullet_type=value;
			return;
		}
*/              //New Lava_walk local counter. -Koji
		if(!str_cmp(name,"LAVA_WALK"))
		{
			robots[id].can_lavawalk=value;
			return;
		}
//	}

		if(!str_cmp(name,"LOOPCOUNT")) {
			robots[id].loop_count=value;
			return;
			}
		if(!str_cmp(name,"LOCAL")) {
			robots[id].blank=value;
			return;
			}

		//I'm suprised Akwende didn't do this...
		//MZX_SPEED can now be changed by robots.
		//Meaning the game speed can now be set
		//by robots. -Koji
		if(!str_cmp(name,"MZX_SPEED")){
			overall_speed=value;
			return;
			}
	//Took out the Number constraints. -Koji
	if(!str_cmp(name,"BULLETTYPE")) {
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
		if(!str_cmp(name,"BIMESG"))
		{
			set_built_in_messages(abs(value % 2));
		}
		//Don't return! Spid
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
			saved_pl_color=*player_color;
		else if(value==0) {
			clear_sfx_queue();
			play_sfx(18);
			*player_color=saved_pl_color;
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
	if(t1<NUM_COUNTERS)
		{//Space found
		str_cpy(counters[t1].counter_name,name);
		counters[t1].counter_value=value;
		}
	return;
}

//Take a key. Returns non-0 if none of proper color exist.
char take_key(char color)
{
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
	if(!str_cmp(name,"CHAR_X")) {
			pixel_x = (pixel_x + value) % 256;
			return;
			}
	if(!str_cmp(name,"CHAR_Y")) {
			pixel_y = (pixel_y + value) % 256;
			return;
			}
	if(!str_cmp(name,"SCORE")) {//Special score inc. code
		if((score+value)<score) score=4294967295ul;
		else score+=value;
		return;
		}
	if(!str_cmp(name,"WRAP"))
	{
		int current,max;
		current = get_counter("WRAP",id) + value;
		max = get_counter("WRAPVAL",id);
		if (current > max) current = 0;
		set_counter("WRAP",current,id);
	}
	t=((long)get_counter(name,id))+value;
	if(t>32767L) t=-32767L;
	if(t<-32768L) t=32768L;
	set_counter(name,(int)t,id);
}

void dec_counter(char far *name,int value,unsigned char id) {
	long t;
	if(!str_cmp(name,"CHAR_X")) {
			pixel_x = (pixel_x - value + 256) % 256;
			return;
			}
	if(!str_cmp(name,"CHAR_Y")) {
			pixel_y = (pixel_y - value + 256) % 256;
			return;
			}
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
	if(!str_cmp(name,"WRAP"))
		{
			int current,max;
			current = get_counter("WRAP",id);
			current -= value;
			max = get_counter("WRAPVAL",id);
			if (current < 0) current = max;
			set_counter("WRAP",current,id);
		}

	t=((long)get_counter(name,id))-value;
	if(t>32767L) t=-32767L;
	if(t<-32768L) t=32768L;
	set_counter(name,(int)t,id);
}

