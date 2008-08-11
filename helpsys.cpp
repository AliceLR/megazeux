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

// internal help system

#include "mod.h"
#include "cursor.h"
#include <string.h>
#include "scrdisp.h"
#include <stdio.h>
#include "meminter.h"
#include "beep.h"
#include "helpsys.h"
#include "data.h"

int context=72;//72="No" context link
unsigned char far *help=NULL;
int contexts[20];
int curr_context=0;

void set_context(int con) {
	contexts[curr_context++]=context;
	context=con;
}

void pop_context(void) {
	context=contexts[--curr_context];
}

void help_system(void) {
	FILE *fp;
	unsigned int t1,t2;
	unsigned char old_cmode=cursor_mode;
	long where;
	long offs;
	long size;
	char file[13];
	char file2[13];
	char label[20];
	//Reserve memory (24k)
	help=(char far *)farmalloc(24576);
	if(help==NULL) {
		//Try to free up mem
		free_sam_cache(1);
		help=(char far *)farmalloc(24576);
		if(help==NULL) {
			beep();
			return;
			}
		}
	//Search context links
	fp=fopen(help_file,"rb");
	if(fp==NULL) {
		beep();
		farfree(help);
		return;
		}
	fread(&t1,1,2,fp);
	fseek(fp,t1*21+4+context*12,SEEK_SET);
	//At proper context info
	fread(&where,1,4,fp);//Where file to load is
	fread(&size,1,4,fp);//Size of file to load
	fread(&offs,1,4,fp);//Offset within file of link
	//Jump to file
	fseek(fp,where,SEEK_SET);
	//Read it in
	fread(help,1,size,fp);
	//Display it
	cursor_off();
labelled:
	help_display(help,offs,file,label);
	//File?
	if(file[0]) {
		//Yep. Search for file.
		fseek(fp,2,SEEK_SET);
		for(t2=0;t2<t1;t2++) {
			fread(file2,1,13,fp);
			if(!_fstricmp(file,file2)) break;
			fseek(fp,8,SEEK_CUR);
			}
		if(t2<t1) {
			//Found file.
			fread(&where,1,4,fp);
			fread(&size,1,4,fp);
			fseek(fp,where,SEEK_SET);
			fread(help,1,size,fp);
			//Search for label
			for(t2=0;t2<size;t2++) {
				if(help[t2]!=255) continue;
				if(help[t2+1]!=':') continue;
				if(!_fstricmp(&help[t2+3],label)) break;//Found label!
				}
			if(t2<size) {
				//Found label. t2 is offset.
				offs=t2;
				goto labelled;
				}
			}
		}
	//All done!
	fclose(fp);
	farfree(help);
	if(old_cmode==CURSOR_UNDERLINE) cursor_underline();
	else if(old_cmode==CURSOR_BLOCK) cursor_solid();
}