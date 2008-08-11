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
// Error code

#include "helpsys.h"
#include "error.h"
#include "window.h"
#include "graphics.h"
#include "getkey.h"
#include "string.h"
#include <setjmp.h>
#include "error.h"
#include <stdlib.h>
#include "palette.h"
#include <stdio.h>
#include "ems.h"
#include <alloc.h>
#include <conio.h>

//Defined and set in MAIN.CPP
extern jmp_buf exit_jump;//Used in error function and nowhere else

//Call for an error OR a warning. Type=0 for a warning, 1 for a recoverable
//error, 2 for a fatal error. Options are (bits set in options and returned
//as action) FAIL=1, RETRY=2, EXIT TO DOS=4, OK=8, HELP=16 (OK is for usually
//only for warnings) Type = 3 for a critical error
int error(char far *string,char type,char options,int segment,
 unsigned int code) {
	int t1=9,ret=0,fad=is_faded();;
	char temp[5];
	char old_help=allow_help;
	//Window
	set_context(code>>8);
	m_hide();
	save_screen(segment);
	if(fad) {
		clear_screen(1824,segment);
		insta_fadein();
		}
	draw_window_box(1,10,78,14,segment,76,64,72);
	//Add title and error name
	if(type==0) write_string(" WARNING: ",35,10,78,segment);
	else if(type==1) write_string(" ERROR: ",36,10,78,segment);
	else if(type==2) write_string(" FATAL ERROR: ",33,10,78,segment);
	else if(type==3) write_string(" CRITICAL ERROR: ",32,10,78,segment);
	write_string(string,40-(str_len(string)>>1),11,79,segment);
	//Add options
	write_string("Press",4,13,78,segment);
	if(options&1) {
		write_string(", F for Fail",t1,13,78,segment);
		t1+=12;
		}
	if(options&2) {
		write_string(", R to Retry",t1,13,78,segment);
		t1+=12;
		}
	if(options&4) {
		write_string(", E to Exit to Dos",t1,13,78,segment);
		t1+=18;
		}
	if(options&8) {
		write_string(", O for OK",t1,13,78,segment);
		t1+=10;
		}
	if((options&16)&&(allow_help==1)) {
		write_string(", F1 for Help",t1,13,78,segment);
		t1+=13;
		}
	else allow_help=0;
	draw_char('.',78,t1,13,segment);
	draw_char(':',78,9,13,segment);
	//Add code if not 0
	if(code!=0) {
		write_string(" Debug code:0000h ",30,14,64,segment);
		itoa(code,temp,16);
		write_string(temp,46-str_len(temp),14,64,segment);
		}
	//Get key
	do {
		t1=getkey();
		if((t1>='a')&&(t1<='z')) t1-=32;
		//Process
		switch(t1) {
			case 'F':
			fail:
				if(!(options&1)) break;
				ret=1;
				break;
			case 'R':
			retry:
				if(!(options&2)) break;
				ret=2;
				break;
			case 'E':
			exit:
				if(!(options&4)) break;
				ret=4;
				break;
			case 'O':
			ok:
				if(!(options&8)) break;
				ret=8;
				break;
			case 'H':
				if(!(options&16)) break;
				//Call help
				break;
			case 27:
				//Escape. Order of options this applies to-
				//Fail, Ok, Retry, Exit.
				if(options&1) goto fail;
				if(options&8) goto ok;
				if(options&2) goto retry;
				goto exit;
			case 13:
				//Enter. Order of options this applies to-
				//OK, Retry, Fail, Exit.
				if(options&8) goto ok;
				if(options&2) goto retry;
				if(options&1) goto fail;
				goto exit;
			}
	} while(ret==0);
	pop_context();
	//Restore screen and exit appropriately
	if(fad) insta_fadeout();
	restore_screen(segment);
	m_show();
	if(ret==4) //Exit to DOS
		longjmp(exit_jump,1);
	allow_help=old_help;
	return ret;
}

//For SFX data integrity checks
#define NUM_SAM_CACHE	16
extern char far *SamStorage[NUM_SAM_CACHE];
//Filenames
extern char SamNames[NUM_SAM_CACHE][13];
//Number of times played (0=NOT LOADED)
extern int SamPlayed[NUM_SAM_CACHE];
//Channel currently playing on plus one (0=none)
extern char SamChannel[NUM_SAM_CACHE];
extern int SCDoing;
extern char far *SCLoadingName;

//For CEH interface
int error2(char far *string,char type,char options,int segment,
 unsigned int code) {
	//For data integrity errors in SFX code
	if((type==3)&&(string[1]=='a')) {
		clrscr();
		printf("\nData integrity error in MegaZeux Sound cache system!\n");
		printf("Please report the following data to Software Visions:\n");
		printf("Action: ");
		if(SCDoing&512) printf("load_mod(%s) ",SCLoadingName);
		if(SCDoing&1) printf("play_sample ");
		if(SCDoing&16) printf("spot_sample ");
		if(SCDoing&2) printf("end_sample ");
		if(SCDoing&4) printf("jump_mod ");
		if(SCDoing&8) printf("volume_mod ");
		if(SCDoing&256) printf("_load_sam(%s) ",SCLoadingName);
		if(SCDoing&64) printf("_free_sam_cache(0) ");
		if(SCDoing&128) printf("_free_sam_cache(1) ");
		if(SCDoing&32) printf("_free_sam ");
		printf("(%d)\nSFX data:\n",SCDoing);
		for(int tmp=0;tmp<NUM_SAM_CACHE;tmp++) {
			printf("#%2.2d: %12.12s played %3.3d times. Channel %2.2d. Mem %s\n",
				tmp,SamNames[tmp],SamPlayed[tmp],SamChannel[tmp],
				SamStorage[tmp]?"ALLOC":"NULL");
			}
		printf("Memory: %ld\nEMS: %d\n",farcoreleft(),free_mem_EMS());
		printf("We will try to work out this problem as soon as possible.\n");
		for(;;) ;
		}
	return error(string,type,options,segment,code);
}
