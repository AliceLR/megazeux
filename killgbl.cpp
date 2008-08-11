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

#include <stdio.h>

struct Robot {
	unsigned int program_length;
	unsigned int program_location;//Offsetted from start of global robot memory
	char robot_name[15];
	unsigned char robot_char;
	unsigned int cur_prog_line;//Location of start of line (pt to FF for none)
	unsigned char pos_within_line;//Countdown for GO and WAIT
	unsigned char robot_cycle;
	unsigned char cycle_count;
	unsigned char bullet_type;
	unsigned char is_locked;
	unsigned char can_lavawalk;//Can always travel on fire
	unsigned char walk_dir;//1-4, of course
	unsigned char last_touch_dir;//1-4, of course
	unsigned char last_shot_dir;//1-4, of course
	int xpos,ypos;//Used for IF ALIGNED "robot", THISX/THISY, PLAYERDIST,
						//HORIZPLD, VERTPLD, and others. Keep udpated at all
						//times. Set to -1/-1 for global robot.
	char status;//0=Un-run yet, 1=Was run but only END'd, WAIT'd, or was
					//inactive, 2=To be re-run on a second robot-run due to a
					//rec'd message
	int blank;//Always 0
	char used;//Set to 1 if used onscreen, 0 if not
	int loop_count;//Loop count. Loops go back to first seen LOOP
									//START, loop at first seen LOOP #, and an ABORT
									//LOOP jumps to right after first seen LOOP #.
} glorob;

//Take a .MZX file (UN-pw-protected) and delete any global robot

void main(int argc,char **argv) {
	int t1,t2;
	if(argc<2) {
		printf("\n\nUsage: KILLGBL FILE.MZX\nThis will totally eradicate the global robot from a .MZX file.\n\n\a");
		return;
		}
	printf("\n\nDeleting the global robot in %s... (WARNING!: No error checks performs!)\n\n",argv[1]);
	FILE *fp=fopen(argv[1],"rb+");
	//find location
	fseek(fp,4230,SEEK_SET);
	long where;
	fread(&where,1,4,fp);
	fseek(fp,where,SEEK_SET);
	fread(&glorob,1,sizeof(Robot),fp);
	t1=glorob.program_location;
	for(t2=0;t2<sizeof(Robot);t2++) {
		((char far *)(&glorob))[t2]=0;
		}
	glorob.program_length=2;
	glorob.robot_cycle=1;
	glorob.program_location=t1;
	glorob.cur_prog_line=1;
	glorob.robot_char=2;
	glorob.bullet_type=1;
	fseek(fp,where,SEEK_SET);
	fwrite(&glorob,1,sizeof(Robot),fp);
	fputc(0xFF,fp);
	fputc(0x00,fp);
	fclose(fp);
}