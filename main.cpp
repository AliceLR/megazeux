/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
 * Copyright (C) 1999 Charles Goetzman
 * Copyright (C) 2002 B.D.A. (Koji) - Koji_Takeo@worldmailer.com
 * Copyright (C) 2002 Gilead Kutnick - exophase@adelphia.net
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

//This source code should be 8086 compatible
#pragma option -2-
#pragma option -1-

#include "beep.h"
#include "profile.h"
#include "ceh.h"
#include "helpsys.h"
#include "sfx.h"
#include "random.h"
#include "edit.h"
#include "ezboard.h"
#include "roballoc.h"
#include <setjmp.h>
#include <dir.h>
#include "blink.h"
#include "timer.h"
#include "egacode.h"
#include "meminter.h"
#include "string.h"
#include "arrowkey.h"
#include "comp_chk.h"
#include "dt_data.h"
#include "graphics.h"
#include "cursor.h"
#include "window.h"
#include "palette.h"
#include "getkey.h"
#include "ems.h"
#include <stdlib.h>
#include "main.h"
#include <dos.h>
#include <stdio.h>
#include "detect.h"
#include "data.h"
#include "mod.h"
#include "game.h"
#include "error.h"
#include <time.h>
#define SAVE_INDIVIDUAL


//new_settings=1 if Addr IRQ or DMA was updated via CMD line so we
//should keep them even if config is entered
char force_ega=0,no_mouse=0,no_ems=0,new_settings=0;
jmp_buf exit_jump;//Used in error function and nowhere else
extern unsigned int Addr,IRQ,DMA;//Sound card parameters (-1=detect)

#ifdef UNREG
char *unreg_exit_mesg=
"This message should not be displayed, it is a bug, if it is.";


#else
char *reg_exit_mesg=
"Thank you for playing MegaZeux.\n\r"
"Read the files help.txt, megazeux.doc and readme.1st if you need help.\n\n\r"
"MegaZeux, by Greg Janson\n\r"
"Additional contributors:\n\r"
"\tSpider124, CapnKev, MenTaLguY, JZig, Akwende, Koji, Exophase\n\n\r"
"Check the whatsnew file for new additions.\n\n\r"
"Check http://www.digitalmzx.net/ for MZX source and binary distributions.\n\r$";

#endif

#pragma warn -par
int main(int argc,char **argv) {
	//Temp var for number->string conversion
	char temp[7];
	//Set to 1 if any errors occur during startup
	char errors=0;
	//Used to save result of early ec_init() call
	char ec_i=0;
	//Temporary variables
	int t1;
	//Return code
	int ret=0;
	//Set if we need first time help
	char first_time=0;
  // Determines if ATI fix is present
  FILE *f_ati_fix;
	//Jump destination for exiting to DOS
	if(setjmp(exit_jump)) {//Exiting to DOS
		mod_exit();
		ret=3;
		goto escape;
		}
	//First thing- check for card and processor
	if(computer_check()) return 1;
	//Scan command line options, exit if need be
	if(scan_options()) return 2;
	if(force_ega) vga_avail=0;
	//Now- Page 0
	page_flip(0);
	//Init palette system and fade out
	init_palette();
	vquick_fadeout();
	//Setup directory strings
	//Get megazeux directory
	str_cpy(help_file,argv[0]);
	//Chop at last backslash
	for(t1=str_len(help_file);t1>=0;t1--)
		if(help_file[t1]=='\\') break;
	help_file[t1+1]=0;
	//Copy to config_file and MSE_file and blank_mod_file and convert_mod_file
	str_cpy(config_file,help_file);
	str_cpy(MSE_file,help_file);
	str_cpy(mzx_blank_mod_file,help_file);
	str_cpy(mzx_convert_mod_file,help_file);
	//Caton filenames
	str_cat(config_file,"megazeux.cfg");
	str_cat(help_file,"mzx_help.fil");
	//Get current directory and drive (form- C:\DIR\SUBDIR)
	getcwd(current_dir,PATHNAME_SIZE);
	str_cpy(megazeux_dir,current_dir);
	megazeux_drive=current_drive=getdisk();
	//Disable smzx mode
	//Switch to EGA 14 point mode and turn off cursor
	ega_14p_mode();
	cursor_off();
	//Init ec code and load up default set NOW to avoid seeing the
	//line characters shift. Save status of call for later.
	ec_i=ec_init();
	//These lines prevent a white flash onscreen
	init_palette();
	insta_fadeout();
	//Display Megazeux startup screen
	draw_window_box(0,0,79,24,0xB800,127,120,113,0);
	draw_window_box(2,1,77,3,0xB800,120,127,113,0);
	draw_window_box(2,4,77,16,0xB800,120,127,113,0);
	draw_window_box(2,17,77,23,0xB800,120,127,113,0);
  write_string("MegaZeux version 2.69b",27,2,127,0xB800);
// #ifdef BETA
	write_string("Beta; please distribute",27,17,127,0xB800);
// #endif
#ifdef GAMMA
	write_string("GAMMA- MAY CONTAIN BUGz!",27,17,127,0xB800);
#endif
#ifdef UNREG
	write_string("Unregistered Evaluation Copy",25,0,122,0xB800);
#endif
	write_string("Graphics card:",4,18,122,0xB800);

	write_string("EMS available:",4,20,122,0xB800);
	write_string("Core mem free:",4,21,122,0xB800);
	write_string("Memory allocs:",4,22,122,0xB800);
	write_string("Keyboard handler:",41,18,122,0xB800);
	write_string("Mouse handler:",44,19,122,0xB800);
	write_string("Sound card port:",42,20,122,0xB800);
	write_string("Sound card IRQ:",43,21,122,0xB800);
	write_string("Sound card DMA:",43,22,122,0xB800);

  // I restored the original startup palette... - Exo
	set_rgb(1,31,31,31);
	set_rgb(6,63,0,0);
	set_rgb(7,21,21,21);
	set_rgb(8,8,8,8);
	set_rgb(9,42,42,63);
	set_rgb(10,42,63,42);
	set_rgb(11,42,63,63);
	set_rgb(12,63,42,42);
	set_rgb(13,63,42,63);
  // Do the ATI fix thing
  f_ati_fix = fopen("ati_fix", "rb");
  if(f_ati_fix != NULL)
  {
    ati_fix = 1;
    fclose(f_ati_fix);
  }
	//Fade in
	vquick_fadein();
	//Initialize systems and display progess at bottom
	//First, show graphics card and processor
	if(force_ega) write_string("EGA (command line)",19,18,125,0xB800);
	else write_string(gcard_strs[card-3],19,18,121,0xB800);
	//Initialize EMS systems
	if(no_ems) write_string("Forced off",19,20,125,0xB800);
	else if(setup_EMS()) write_string("None",19,20,124,0xB800);
	else {
		itoa(free_mem_EMS()*16,temp,10);
		temp[str_len(temp)+1]=0;
		temp[str_len(temp)]='k';
		if(free_mem_EMS()>=64) write_string(temp,19,20,121,0xB800);
		else write_string(temp,19,20,124,0xB800);
		}
	//Write up keyboard mode
	if(keyb_mode) write_string("Alternate mode",59,18,125,0xB800);
	else write_string("Default mode",59,18,121,0xB800);
	//Initialize windowing code
	if(window_cpp_entry()) {
		write_string("OUT OF MEMORY",19,22,124,0xB800);
		errors=1;
		}
	//Initialize mouse handler (from now on must hide/show it)
	if(!no_mouse) {
		m_init();
		m_hide();
		}
	if(no_mouse) write_string("Forced off",59,19,125,0xB800);
	else if(driver_activated) write_string("Ok",59,19,121,0xB800);
	else write_string("No mouse found",59,19,124,0xB800);
	//Initialize character edit code and character set
	if(ec_i) {
		write_string("OUT OF MEMORY",19,22,124,0xB800);
		errors=1;
		}
	//Install new timer ISR
	install_timer();
	//Allocate misc. memory
	t1=0;//Set to 1 for error
	board_list=(char far *)farmalloc(NUM_BOARDS*BOARD_NAME_SIZE);
	if(board_list==NULL) t1=1;
	board_offsets=(bOffset far *)farmalloc(NUM_BOARDS*sizeof(bOffset));
	if(board_offsets==NULL) t1=1;
	board_sizes=(unsigned long far *)farmalloc(NUM_BOARDS*4);
	if(board_sizes==NULL) t1=1;
	board_filenames=(char far *)farmalloc(NUM_BOARDS*FILENAME_SIZE);
	if(board_filenames==NULL) t1=1;
	level_id=(unsigned char far *)farmalloc(MAX_ARRAY_X*MAX_ARRAY_Y);
	if(level_id==NULL) t1=1;
	level_color=(unsigned char far *)farmalloc(MAX_ARRAY_X*MAX_ARRAY_Y);
	if(level_color==NULL) t1=1;
	level_param=(unsigned char far *)farmalloc(MAX_ARRAY_X*MAX_ARRAY_Y);
	if(level_param==NULL) t1=1;
	level_under_id=(unsigned char far *)farmalloc(MAX_ARRAY_X*MAX_ARRAY_Y);
	if(level_under_id==NULL) t1=1;
	level_under_color=(unsigned char far *)farmalloc(MAX_ARRAY_X*MAX_ARRAY_Y);
	if(level_under_color==NULL) t1=1;
	level_under_param=(unsigned char far *)farmalloc(MAX_ARRAY_X*MAX_ARRAY_Y);
	if(level_under_param==NULL) t1=1;
	overlay=(unsigned char far *)farmalloc(MAX_ARRAY_X*MAX_ARRAY_Y);
	if(overlay==NULL) t1=1;
	overlay_color=(unsigned char far *)farmalloc(MAX_ARRAY_X*MAX_ARRAY_Y);
	if(overlay_color==NULL) t1=1;
	update_done=(unsigned char far *)farmalloc(MAX_ARRAY_X*MAX_ARRAY_Y);
	if(update_done==NULL) t1=1;
	//Allocate one more for global robot
	robots=(Robot far *)farmalloc((NUM_ROBOTS+1)*sizeof(Robot));
	if(robots==NULL) t1=1;
	scrolls=(Scroll far *)farmalloc(NUM_SCROLLS*sizeof(Scroll));
	if(scrolls==NULL) t1=1;
	counters=(Counter far *)farmalloc(NUM_COUNTERS*sizeof(Counter));
	if(counters==NULL) t1=1;
	sensors=(Sensor far *)farmalloc(NUM_SENSORS*sizeof(Sensor));
	if(sensors==NULL) t1=1;
	if(init_robot_mem()) t1=1;
	board_setup();
	if(sfx_init()) t1=1;
	if(t1) {
		write_string("OUT OF MEMORY",19,22,124,0xB800);
		errors=1;
		}
	//Initialize blink code, mod code, random number generator, and keyboard
	blink_off();
	random_seed();
	install_i09();
	installceh();

	//Write up free memory (below 100 shown as bad)
	itoa(t1=(int)(farcoreleft()>>10),temp,10);
	temp[str_len(temp)+1]=0;
	temp[str_len(temp)]='k';
	if(t1>=100) write_string(temp,19,21,121,0xB800);
	else write_string(temp,19,21,124,0xB800);
	//Give error message if appropriate (including if free Kb below 20)
	if((errors)||(t1<20)) {
		draw_window_box(3,9,76,11,0xB800,76,64,70,0);
		write_string("Out of memory error- Please increase available core/EMS memory",
			9,10,79,0xB800);
		getkey();
		goto escape;
		}
	else {
		write_string("Ok",19,22,121,0xB800);
		//Configuration time
		//Note- music_device is from 0 to 6 at all times.
		//Mixing rate is 0 1 or 2, and converted to kHz before game is
		//started.
		//Search for .CFG file
		if(!load_config_file()) {
			//Loaded- display and verify
			save_config_file();//Update Port/IRQ/DMA
			write_string("Current configuration-",3,5,127,0xB800);
			write_string("Device for digitized music and sound:",5,6,122,0xB800);
			if((music_device>0)&&(music_device<6)) {
				write_string("Sound quality for digitized output:",7,7,122,0xB800);
				write_string(music_quality[mixing_rate],43,7,123,0xB800);
				}
			if((music_device>0)&&(music_device<6)) {
				write_string("Number of digitized sfx channels:",9,8,122,0xB800);
				draw_char('0'+sfx_channels,123,43,8,0xB800);
				}
			write_string("PC speaker sound effects:",17,9,122,0xB800);
			if(music_device==6) write_string("Gravis Ultra-Sound (no sfx)",43,6,123,0xB800);
			else write_string(music_devices[music_device],43,6,123,0xB800);
			if(sfx_on) write_string("On",43,9,123,0xB800);
			else write_string("Off",43,9,123,0xB800);
			//Port/IRQ/DMA-
			if(Addr==0xFFFF) write_string("Autodetect",59,20,121,0xB800);
			else {
				itoa(Addr,temp,16);
				temp[str_len(temp)+1]=0;
				temp[str_len(temp)]='h';
				write_string(temp,59,20,125,0xB800);
				}
			if(IRQ==0xFF) write_string("Autodetect",59,21,121,0xB800);
			else {
				itoa(IRQ,temp,10);
				write_string(temp,59,21,125,0xB800);
				}
			if(DMA==0xFF) write_string("Autodetect",59,22,121,0xB800);
			else {
				itoa(DMA,temp,10);
				write_string(temp,59,22,125,0xB800);
				}
			write_string("Press C to configure, ESC to exit, or any other key to continue.",
				3,10,127,0xB800);
		rekey:
			t1=getkey();
			if(t1==27) goto escape;
			if(t1==0) goto rekey;
			if((t1!='c')&&(t1!='C')) goto maingame;
			if(!new_settings) {
				Addr=0xFFFF;
				IRQ=DMA=0xFF;
				write_string("Autodetect",59,20,121,0xB800);
				write_string("Autodetect",59,21,121,0xB800);
				write_string("Autodetect",59,22,121,0xB800);
				}
			}
		else {
			first_time=1;
			write_string("Autodetect",59,20,121,0xB800);
			write_string("Autodetect",59,21,121,0xB800);
			write_string("Autodetect",59,22,121,0xB800);
			}
		//Configure
		//Digitized output device
		draw_window_box(2,4,77,16,0xB800,120,127,113,0);
		write_string("Devices for digitized music and sound-",3,5,127,0xB800);
		for(t1=0;t1<(NUM_DEVICES/2+1);t1++) {
			draw_char('A'+t1,127,5,6+t1,0xB800);
			draw_char(')',127,6,6+t1,0xB800);
			write_string(music_devices[t1],8,6+t1,125,0xB800);
			}
		for(;t1<NUM_DEVICES;t1++) {
			draw_char('A'+t1,127,40,6+t1-NUM_DEVICES/2-1,0xB800);
			draw_char(')',127,41,6+t1-NUM_DEVICES/2-1,0xB800);
			write_string(music_devices[t1],43,6+t1-NUM_DEVICES/2-1,125,0xB800);
			}
		write_string(music_devices[NUM_DEVICES],43,9,125,0xB800);
		write_string("Choose an output device or press ESC to exit.",3,13,127,
			0xB800);
		do {
			t1=getkey();
			if((t1>='a')&&(t1<='z')) t1-=32;
		} while((t1!=27)&&((t1<'A')||(t1>'G')));
		if(t1==27) goto escape;
		music_device=t1-'A';
		//Mixing rate
		if(music_device==0) mixing_rate=0;
		else if(music_device==6) mixing_rate=2;
		else {
			draw_window_box(2,4,77,16,0xB800,120,127,113,0);
			write_string("Choose music quality (higher quality requires more processor power)",3,5,127,0xB800);
			for(t1=0;t1<3;t1++) {
				draw_char('A'+t1,127,5,6+t1,0xB800);
				draw_char(')',127,6,6+t1,0xB800);
				write_string(music_quality[t1],8,6+t1,125,0xB800);
				}
			write_string("Choose a music quality or press ESC to exit.",3,15,127,
				0xB800);
			do {
				t1=getkey();
				if((t1>='a')&&(t1<='z')) t1-=32;
			} while((t1!=27)&&((t1<'A')||(t1>'C')));
			if(t1==27) goto escape;
			mixing_rate=t1-'A';
			}
		//Sfx channels
		if((music_device==0)||(music_device==6)) sfx_channels=0;
		else {
			draw_window_box(2,4,77,16,0xB800,120,127,113,0);
			write_string("Choose number of digitized sound effect channels-",3,5,
				127,0xB800);
			write_string("1, 2, 3, or 4, or ESC to exit.",3,7,127,0xB800);
			write_string("(higher requires more memory and is slower)",3,8,125,0xB800);
			do {
				t1=getkey();
			} while((t1!=27)&&((t1<'1')||(t1>'4')));
			if(t1==27) goto escape;
			sfx_channels=t1-'0';
			}
		//Sound effects
		draw_window_box(2,4,77,16,0xB800,120,127,113,0);
		write_string("PC speaker sound effects-",3,5,127,0xB800);
		write_string("Press 1 for On, 2 for Off, or ESC to exit.",3,7,127,
			0xB800);
		do {
			t1=getkey();
		} while((t1!=27)&&(t1!='1')&&(t1!='2'));
		if(t1==27) goto escape;
		sfx_on='2'-t1;
		//Done
		save_config_file();
		}
maingame:
	mixing_rate=mixing_rates[music_device][mixing_rate];
	if(music_device==0) music_on=0;
	else music_on=1;
	vquick_fadeout();
	clear_screen(1824,0xB800);
	//Init mod code
	mod_init();
	//First time help
	if(first_time) {
		default_palette();
		insta_fadein();
		context=71;
		help_system();
		insta_fadeout();
		}
	context=72;
	//Run main game (mouse is hidden and palette is faded)
	title_screen();
	update_config_file();//Save speed setting
	mod_exit();
escape:
	music_device=0;//To prevent errors in accessing end_mod()
	vquick_fadeout();
	//Deallocate miscellaneous memory items
	sfx_exit();
	clear_world();
	exit_robot_mem();
	if(board_list!=NULL) farfree(board_list);
	if(board_offsets!=NULL) farfree(board_offsets);
	if(board_sizes!=NULL) farfree(board_sizes);
	if(board_filenames!=NULL) farfree(board_filenames);
	if(level_id!=NULL) farfree(level_id);
	if(level_color!=NULL) farfree(level_color);
	if(level_param!=NULL) farfree(level_param);
	if(level_under_id!=NULL) farfree(level_under_id);
	if(level_under_color!=NULL) farfree(level_under_color);
	if(level_under_param!=NULL) farfree(level_under_param);
	if(overlay!=NULL) farfree(overlay);
	if(overlay_color!=NULL) farfree(overlay_color);
	if(update_done!=NULL) farfree(update_done);
	if(robots!=NULL) farfree(robots);
	if(scrolls!=NULL) farfree(scrolls);
	if(counters!=NULL) farfree(counters);
	if(sensors!=NULL) farfree(sensors);
	//Uninstall systems in reverse order
	uninstall_i09();
	blink_on();
	clear_sfx_queue();
	uninstall_timer();
	m_deinit();
	window_cpp_exit();
	ec_exit();
	clear_screen(1824,0xB800);
	page_flip(0);
	init_palette();
	if((vga_avail)||((force_ega)&&(card>=VGAm))) vga_16p_mode();
	cursor_underline();

	asm push ds
#ifdef UNREG
	asm mov dx,SEG unreg_exit_mesg
	asm mov ds,dx
	asm lds dx,ds:unreg_exit_mesg
#else
	asm mov dx,SEG reg_exit_mesg
	asm mov ds,dx
	asm lds dx,ds:reg_exit_mesg
#endif
	asm mov ah,9
	asm int 21h
	asm pop ds

	profiling_summary();

	//ERRORLEVEL
	return ret;
}
#pragma warn +par

char scan_options(void) {
	int t1,t2;
	char help=0;//Set to 1 for beep+help, 2 for help
	//Scan the command line options and adjust global variables accordingly.
	//Returns non-0 if need to exit.
	if(_argc>1) {
		for(t1=1;t1<_argc;t1++) {
			if((_argv[t1][0]!='-')&&(_argv[t1][0]!='+')&&
				(_argv[t1][0]!='/')) {
				help=1;
				break;
				}
			str_lwr(_argv[t1]);
			//Figure it out...
			if(!str_cmp(&_argv[t1][1],"?")) help=2;
			else if(!str_cmp(&_argv[t1][1],"nomouse")) no_mouse=1;
			else if(!str_cmp(&_argv[t1][1],"noems")) no_ems=1;
			else if(!str_cmp(&_argv[t1][1],"ega")) force_ega=1;
			else if(!str_cmp(&_argv[t1][1],"keyb2")) keyb_mode=1;
			//Cheat mode- use a - followed by an ALT-254.
			else if(((unsigned char)_argv[t1][1])==254) cheats_active++;
			else if(_argv[t1][1]=='l') {
				//Set "current" file to string after 'l'
				if(str_len(&_argv[t1][2])<FILENAME_SIZE) {//Only if short enough
					str_cpy(curr_file,&_argv[t1][2]);
					add_ext(curr_file,".MZX");
					}
				}
			else {
				//Check for variable arguments for sound card port/irq/dma
				t2=_argv[t1][5];
				_argv[t1][5]=0;
				if(!str_cmp(&_argv[t1][1],"port")) {
					_argv[t1][5]=t2;
					//Set port
					Addr=strtol(&_argv[t1][5],NULL,16);
					new_settings=1;
					}
				else {
					_argv[t1][5]=t2;
					t2=_argv[t1][4];
					_argv[t1][4]=0;
					if(!str_cmp(&_argv[t1][1],"irq")) {
						_argv[t1][4]=t2;
						//Set IRQ
						IRQ=strtol(&_argv[t1][4],NULL,10);
						new_settings=1;
						}
					else if(!str_cmp(&_argv[t1][1],"dma")) {
						_argv[t1][4]=t2;
						//Set DMA
						DMA=strtol(&_argv[t1][4],NULL,10);
						new_settings=1;
						}
					else {
						help=1;
						break;
						}
					}
				}
			}
		}
	if(help) {
		if(help==1) puts("\a");
		else puts("");
		puts("MegaZeux version 2.69b\tCommand line parameters-\n");
		puts("      -?  Help with parameters.");
		puts("-nomouse  Don't use mouse, even if found.");
		puts("  -noems  Don't use EMS memory, even if available. (NOT RECOMMENDED)");
		puts("    -ega  Use EGA mode even if VGA or better found.");
		puts("  -keyb2  Use the alternate keyboard handler; Try this if the keyboard acts");
		puts("\t  strange within the game or keys come out as if you are holding the");
		puts("\t  Ctrl, Shift, or Alt key down, when you aren't.");
		puts("-l[file]  Loads the world [file] up when starting MegaZeux. Do not include the");
		puts("\t  [ or ]. You do not have to include the extension.\n");
		puts("Sound card settings- (see your sound card manual for details)");
		puts("These are only needed if autodetection fails. They are saved in the");
		puts("configuration file (MEGAZEUX.CFG) until you 'C'hange your configuration.\n");
		puts("-port230  Use base port 230h (substitute 240h, 250h, etc.)");
		puts("   -irq5  Use IRQ 5 (substitute 3, 4, etc.)");
		puts("   -dma3  Use DMA 3 (substitute 4, 5, etc.)");
		return 1;
		}
	return 0;
}

//Configuration file- MEGAZEUX.CFG.
//Simple file containing music device (byte), mixing rate (word), and
//whether sound is on. (byte)

char load_config_file(void) {//Returns non-0 if not found
	int t1;
	FILE *fp;
	fp=fopen(config_file,"rb");
	if(fp==NULL) return 1;
	t1=fgetc(fp);
	if((t1!=126)&&(t1!=127)) {
		//old file- load overall_speed and return "no config file"
		fgetc(fp);
		fgetc(fp);
		fgetc(fp);
		overall_speed=fgetc(fp);
		if((overall_speed>9)||(overall_speed<1)) overall_speed=4;
		fclose(fp);
		return 1;
		}
	music_device=fgetc(fp);
	fread(&mixing_rate,2,1,fp);
	sfx_on=fgetc(fp);
	overall_speed=fgetc(fp);
	sfx_channels=fgetc(fp);
	if((overall_speed>9)||(overall_speed<1)) overall_speed=4;
	if(Addr==0xFFFF) fread(&Addr,2,1,fp);
	else fseek(fp,2,SEEK_CUR);
	if(DMA==0xFF) fread(&DMA,2,1,fp);
	else fseek(fp,2,SEEK_CUR);
	if(IRQ==0xFF) fread(&IRQ,2,1,fp);
	if(t1==126) {
		music_gvol=fgetc(fp);
		sound_gvol=fgetc(fp);
		}
	fclose(fp);
	return 0;
}

void save_config_file(void) {//Saves the .CFG file
	FILE *fp;
	fp=fopen(config_file,"wb");
	if(fp==NULL) return;
	fputc(126,fp);//Code to signify version 2.50 config file
	fputc(music_device,fp);
	fwrite(&mixing_rate,2,1,fp);
	fputc(sfx_on,fp);
	fputc(overall_speed,fp);
	fputc(sfx_channels,fp);
	fwrite(&Addr,2,1,fp);
	fwrite(&DMA,2,1,fp);
	fwrite(&IRQ,2,1,fp);
	fputc(music_gvol,fp);
	fputc(sound_gvol,fp);
	fclose(fp);
}

void update_config_file(void) {//Updates the speed setting in the .CFG file
	FILE *fp;
	fp=fopen(config_file,"rb+");
	if(fp==NULL) return;
	fseek(fp,5,SEEK_SET);
	fputc(overall_speed,fp);
	fseek(fp,7,SEEK_CUR);
	fputc(music_gvol,fp);
	fputc(sound_gvol,fp);
	fclose(fp);
}