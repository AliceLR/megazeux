/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
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

//Code to find space to store boards, and push and pull them into and out
//of storage. Also deallocates board storage and helps simplify file
//access.

#include "window.h"
#include "meter.h"
#include "roballoc.h"
#include "boardmem.h"
#include "meminter.h"
#include <dir.h>
#include "ems.h"
#include "data.h"
#include "const.h"
#include <stdio.h>
#include "string.h"
#include "error.h"
#include <dos.h>
#include "mod.h"

#define SAVE_INDIVIDUAL

//Normalize an unsigned char far * ptr-
#define normalize(x) {\
	x=(unsigned char far *)MK_FP(FP_SEG(x)+(FP_OFF(x)>>4),FP_OFF(x)&15); }

//Find space for board of given size and allocates it to given id. Don't use
//if space is already allocated. If allocated, returns 0. Out of mem returns
//1. This is really rare- Checks for ems, then conventional, then disk.
//Doesn't affect board_sizes[]. If conv_mem_ok is 0, then only ems and disk
//are checked.
char allocate_board_space(long size,unsigned char id,char conv_mem_ok)
{
	int t1,ems_pages;
	FILE *fp;
	//Check for ems first...
	//Get number of 16k pages needed
	ems_pages=(int)(size>>14);
	if(size&16383) ems_pages++;
	t1=alloc_EMS(ems_pages);
	if(t1)
	{
		//Got it!
		board_where[id]=W_EMS;
		board_offsets[id].EMS.handle=t1;
		board_offsets[id].EMS.page=0;
		board_filenames[id*FILENAME_SIZE]=0;
		return 0;
	}
	//Nope.. try conventional
	// SPIDER'S NOTE: WILL NOT USE CONVENTIONAL, DISABLED
	if(conv_mem_ok)
	{
		board_offsets[id].mem=(unsigned char *)farmalloc(size);
		if(board_offsets[id].mem)
		{
			//Got it!
			board_where[id]=W_MEMORY;
			board_filenames[id*FILENAME_SIZE]=0;
			return 0;
		}
		//Retry after clearing sfx cache
		free_sam_cache(1);
		board_offsets[id].mem=(unsigned char *)farmalloc(size);
		if(board_offsets[id].mem)
		{
			//Got it!
			board_where[id]=W_MEMORY;
			board_filenames[id*FILENAME_SIZE]=0;
			return 0;
		}
	}
	//Nope.. try disk
	//Make temporary filename
	str_cpy(&board_filenames[id*FILENAME_SIZE],"~MZTMPXXXXXX");
	mktemp(&board_filenames[id*FILENAME_SIZE]);
	//Open..
	fp=fopen(&board_filenames[id*FILENAME_SIZE],"wb");
	if(fp==NULL)
	{
		//Error
		board_filenames[id*FILENAME_SIZE]=0;
		return 1;
	}
	//Try to solidify room
	fseek(fp,size-1,SEEK_SET);
	fputc(0,fp);
	fseek(fp,size,SEEK_SET);
	if(ftell(fp)<size)
	{
		//Not enough room on hard drive
		fclose(fp);
		unlink(&board_filenames[id*FILENAME_SIZE]);
		board_filenames[id*FILENAME_SIZE]=0;
		return 2;
	}
	//Room made!
	fclose(fp);
	board_where[id]=W_TEMPFILE;
	board_offsets[id].offset=0;
	return 0;
}

//Deallocates the space allocated to id given. Sets where to W_NOWHERE and
//clears all others except for board_sizes[].
void deallocate_board_space(unsigned char id)
{
	//According to type...
	switch(board_where[id])
	{
		case W_EMS:
			//EMS- deallocate.
			free_EMS(board_offsets[id].EMS.handle);
			break;
		case W_MEMORY:
			//Memory- deallocate.
			farfree(board_offsets[id].mem);
			break;
		case W_TEMPFILE:
			//Temp file- delete.
			unlink(&board_filenames[id*FILENAME_SIZE]);
			break;
	}
	//Reset variables for id
	board_where[id]=W_NOWHERE;
	board_offsets[id].offset=0;
	board_filenames[id*FILENAME_SIZE]=0;
}

//Returns size array would take up RLE2 encoded, using current x size and
//y size.
unsigned int RLE2_size(unsigned char far *plane)
{
	unsigned int size=4;//4 for x/y size
	int t1,t2;
	int runchar=-1,runsize=-1,chr;

	for(t1=0;t1<board_ysiz;t1++)
	{
		for(t2=0;t2<board_xsiz;t2++)
		{
			//Get char
			chr=plane[t1*max_bxsiz+t2];
			//Continue run?
			if(chr==runchar)
			{
				//Yep.
				runsize++;
				//Run size at max of 127?
				if(runsize==127)
				{
					//Store run and reset it.
					size+=2;
					runchar=runsize=-1;
				}
			}
			else
			{
				//Nope- Store current run...
				if((runchar<128)&&(runsize==1))	size++;
				else if(runchar>-1)	size+=2;
				//...and set up for a new run.
				runchar=chr;
				runsize=1;
			}
			//Loop.
		}
	}
	//Store last run, if any
	if((runchar<128)&&(runsize==1))	size++;
	else if(runchar>-1)	size+=2;
	//Done- return size.
	return size;
}

//Stores plane in where as RLE2 compressed, using current x/y sizes.
//Returns size.
unsigned int RLE2_store(unsigned char far *where,unsigned char far *plane)
{
	unsigned int size=4;//4 for x/y size
	int t1,t2;
	int runchar=-1,runsize=-1,chr;

  //board_xsiz = 100;
	//board_ysiz = 50;

	where[0]=board_xsiz&255; where[1]=board_xsiz>>8;
	where[2]=board_ysiz&255; where[3]=board_ysiz>>8;


	for(t1=0;t1<board_ysiz;t1++)
	{
		for(t2=0;t2<board_xsiz;t2++)
		{
			//Get char
			chr=plane[t1*max_bxsiz+t2];
			//Continue run?
			if(chr==runchar)
			{
				//Yep.
				runsize++;
				//Run size at max of 127?
				if(runsize==127)
				{
					//Store run and reset it.
					where[size++]=runsize|128;
					where[size++]=runchar;
					runchar=runsize=-1;
				}
			}
			else
			{
				//Nope- Store current run...
				if((runchar<128)&&(runsize==1))	where[size++]=runchar;
				else if(runchar>-1)
				{
					where[size++]=runsize|128;
					where[size++]=runchar;
				}
				//...and set up for a new run.
				runchar=chr;
				runsize=1;
			}
			//Loop.
		}
	}
	//Store last run, if any
	if((runchar<128)&&(runsize==1))	where[size++]=runchar;
	else if(runchar>-1)
	{
		where[size++]=runsize|128;
		where[size++]=runchar;
	}
	//Done.
	return size;
}

//Stores plane in file as RLE2 compressed, using current x/y sizes.
void RLE2_save(FILE *fp,unsigned char far *plane)
{
	int t1,t2;
	int runchar=-1,runsize=-1,chr;

	fwrite(&board_xsiz,2,1,fp);
	fwrite(&board_ysiz,2,1,fp);
	for(t1=0;t1<board_ysiz;t1++)
	{
		for(t2=0;t2<board_xsiz;t2++)
		{
			//Get char
			chr=plane[t1*max_bxsiz+t2];
			//Continue run?
			if(chr==runchar)
			{
				//Yep.
				runsize++;
				//Run size at max of 127?
				if(runsize==127)
				{
					//Store run and reset it.
					fputc(runsize|128,fp);
					fputc(runchar,fp);
					runchar=runsize=-1;
				}
			}
			else
			{
				//Nope- Store current run...
				if((runchar<128)&&(runsize==1))	fputc(runchar,fp);
				else if(runchar>-1)
				{
					fputc(runsize|128,fp);
					fputc(runchar,fp);
				}
				//...and set up for a new run.
				runchar=chr;
				runsize=1;
			}
			//Loop.
		}
	}
	//Store last run, if any
	if((runchar<128)&&(runsize==1))	fputc(runchar,fp);
	else if(runchar>-1)
	{
		fputc(runsize|128,fp);
		fputc(runchar,fp);
	}
	//Done.
}

//Reads RLE2-compressed plane from where and puts into plane.
//Also sets board_xsiz and board_ysiz unless specified otherwise.
//Returns byte count of RLE2 read.
unsigned int RLE2_read(unsigned char far *where,unsigned char far *plane,
											 char change_xysizes)
{
	unsigned int pos=4;//2 for x/y size
	int t1,t2,t3,xsiz=where[0]+(where[1]<<8),ysiz=where[2]+(where[3]<<8);

	int runsize,chr;
	if(change_xysizes)
	{
		board_xsiz=xsiz;
		board_ysiz=ysiz;
	}
	t1=t2=0;//y and x position
	do
	{
		chr=where[pos++];
		if(!(chr&128))
		{//Regular character
			plane[t1*max_bxsiz+t2]=chr;
			if((++t2)>=xsiz)
			{
				t2=0;
				if((++t1)>=ysiz) goto done;
			}
		}
		else
		{
			//A run
			runsize=chr&127;
			chr=where[pos++];
			for(t3=0;t3<runsize;t3++)
			{
				plane[t1*max_bxsiz+t2]=chr;
				if((++t2)>=xsiz)
				{
					t2=0;
					if((++t1)>=ysiz) goto done;
				}
			}
		}
	} while(1);
	done:
	//Done!
	return pos;
}

//Reads RLE2-compressed plane from file and puts into plane.
//Also sets board_xsiz and board_ysiz unless specified otherwise.
void RLE2_load(FILE *fp,unsigned char far *plane,char change_xysizes)
{
	int t1,t2,t3,xsiz,ysiz;
	int runsize,chr;
	fread(&xsiz,2,1,fp);
	fread(&ysiz,2,1,fp);

	//xsiz = 100;
	//ysiz = 50;

	if(change_xysizes)
	{
		board_xsiz=xsiz;
		board_ysiz=ysiz;
	}
	t1=t2=0;//y and x position
	do
	{
		chr=fgetc(fp);
		if(!(chr&128))
		{//Regular character
			plane[t1*max_bxsiz+t2]=chr;
			if((++t2)>=xsiz)
			{
				t2=0;
				if((++t1)>=ysiz) goto done;
			}
		}
		else
		{
			//A run
			runsize=chr&127;
			chr=fgetc(fp);
			for(t3=0;t3<runsize;t3++)
			{
				plane[t1*max_bxsiz+t2]=chr;
				if((++t2)>=xsiz)
				{
					t2=0;
					if((++t1)>=ysiz) goto done;
				}
			}
		}
	} while(1);
	done:
	//Done!
}

//Gets size current board would take up, were it stored.
long size_of_current_board(void)
{
	long size=224;//Size of info other than board and robots/etc
	int t1,count=0;
	//Count robots
	for(t1=1;t1<NUM_ROBOTS;t1++)
		if(robots[t1].used)	count=t1;
		//Add room for robots and all their programs plus the count itself
	size+=sizeof(Robot)*count;
	if(count)
		for(t1=0;t1<count;t1++)
			size+=robots[t1+1].program_length;
	//Count scrolls
	count=0;
	for(t1=1;t1<NUM_SCROLLS;t1++)
		if(scrolls[t1].used) count=t1;
		//Add room for scrolls and all their texts plus the count itself
	size+=sizeof(Scroll)*count;
	if(count)
		for(t1=0;t1<count;t1++)
			size+=scrolls[t1+1].mesg_size;
	//Count sensors
	count=0;
	for(t1=1;t1<NUM_SENSORS;t1++)
		if(sensors[t1].used) count=t1;
		//Add room for sensors plus the count itself
	size+=sizeof(Sensor)*count;
	//If overlay on, add room for 2 bytes (one for overlay mode, one
	//for indicator bit) and RLE2 of both planes
	if(overlay_mode)
	{
		size+=2;
		size+=RLE2_size(overlay);
		size+=RLE2_size(overlay_color);
	}
	//Finally, add room for all six planes, RLE2 encoded, and return
	size+=RLE2_size(level_id);
	size+=RLE2_size(level_color);
	size+=RLE2_size(level_param);
	size+=RLE2_size(level_under_id);
	size+=RLE2_size(level_under_color);
	return(size+RLE2_size(level_under_param));
}

//Stores current board in slot given. Space must already be allocated.
//board_sizes and other board variables MUST be accurate. Returns non-0
//if out of room in memory and on disk for working, or for misc. errors.
//Does not affect and is not affected by a current object or global robot.
char store_current(unsigned char id)
{
	unsigned char far *ptr;//Where to store for mem/ems
	unsigned char far *old_ptr;//Saves old ems allocated ptr
	FILE *fp;//Where to store for tempfile (or ems if pressed for space)
	int t1,count,tcpy;
	long copied;
	//Space of proper size already assumed to be allocated. This function
	//has two major parts- The code for memory/ems, and the code for temp
	//files.
	//Switch according to storage type.
	switch(board_where[id])
	{
		case W_MEMORY:
		case W_EMS:
			//If EMS, allocate a temporary area to allow storage in conventional
			//memory for now, then copy to EMS later. If not possible, we use a
			//temp file and then copy THAT to EMS later. If no room for a temp
			//file... return error.
			if(board_where[id]==W_EMS)
			{
				//Allocate a temporary area
				ptr=(unsigned char far *)farmalloc(board_sizes[id]);
				if(ptr==NULL)
				{
					//No room in mem- try a temp file
					//Make temporary filename
					str_cpy(&board_filenames[id*FILENAME_SIZE],"~MZTMPXXXXXX");
					mktemp(&board_filenames[id*FILENAME_SIZE]);
					//Open..
					fp=fopen(&board_filenames[id*FILENAME_SIZE],"wb+");
					if(fp==NULL)
					{
						//Error
						board_filenames[id*FILENAME_SIZE]=0;
						return 1;
					}
					//Try to make room
					fseek(fp,board_sizes[id],SEEK_SET);
					if(ftell(fp)!=(board_sizes[id]))
					{
						//Not enough room on hard drive
						fclose(fp);
						unlink(&board_filenames[id*FILENAME_SIZE]);
						board_filenames[id*FILENAME_SIZE]=0;
						return 2;
					}
					//Room made! Solidify.
					fseek(fp,-1,SEEK_CUR);
					fputc(0,fp);
					fclose(fp);
					//Go to tempfile storage
					goto store_tempfile;
				}
				else old_ptr=ptr;
			}
			else ptr=(unsigned char far *)board_offsets[id].mem;
			*(ptr++)=max_bsiz_mode;
			//Store RLE2 stuff
			if(overlay_mode)
			{
				*(ptr++)=0;
				*(ptr++)=overlay_mode;
				ptr+=RLE2_store((unsigned char far*)ptr,overlay);
				normalize(ptr);
				ptr+=RLE2_store((unsigned char far*)ptr,overlay_color);
				normalize(ptr);
			}

			ptr+=RLE2_store((unsigned char far *)ptr,level_id);
			normalize(ptr);
			ptr+=RLE2_store((unsigned char far *)ptr,level_color);
			normalize(ptr);
			ptr+=RLE2_store((unsigned char far *)ptr,level_param);
			normalize(ptr);
			ptr+=RLE2_store((unsigned char far *)ptr,level_under_id);
			normalize(ptr);
			ptr+=RLE2_store((unsigned char far *)ptr,level_under_color);
			normalize(ptr);
			ptr+=RLE2_store((unsigned char far *)ptr,level_under_param);
			normalize(ptr);
			//Store variables- Due to the way they are stored in DATA.ASM,
			//they can all be stored as a series of 210 bytes starting at
			//mod_playing. Appropriate code to save each individually is
			//also included in case this ever fails. To use the individual
			//code, define the symbol SAVE_INDIVIDUAL.
#ifndef SAVE_INDIVIDUAL

			mem_cpy((char far *)ptr,mod_playing,207+FILENAME_SIZE);
			ptr+=207+FILENAME_SIZE;
#else
			mem_cpy((char far *)ptr,mod_playing,FILENAME_SIZE);
			ptr+=FILENAME_SIZE;
			*(ptr++)=viewport_x;
			*(ptr++)=viewport_y;
			*(ptr++)=viewport_xsiz;
			*(ptr++)=viewport_ysiz;
			*(ptr++)=can_shoot;
			*(ptr++)=can_bomb;
			*(ptr++)=fire_burn_brown;
			*(ptr++)=fire_burn_space;
			*(ptr++)=fire_burn_fakes;
			*(ptr++)=fire_burn_trees;
			*(ptr++)=explosions_leave;
			*(ptr++)=save_mode;
			*(ptr++)=forest_becomes;
			*(ptr++)=collect_bombs;
			*(ptr++)=fire_burns;
			mem_cpy((char far *)ptr,(char far *)board_dir,4);
			ptr+=4;
			*(ptr++)=restart_if_zapped;
			mem_cpy((char far *)ptr,(char far *)&time_limit,2);
			ptr+=2;
			*(ptr++)=last_key;
			mem_cpy((char far *)ptr,(char far *)&num_input,2);
			ptr+=2;
			*(ptr++)=input_size;
			mem_cpy((char far *)ptr,input_string,81);
			ptr+=81;
			*(ptr++)=player_last_dir;
			mem_cpy((char far *)ptr,bottom_mesg,81);
			ptr+=81;
			*(ptr++)=b_mesg_timer;
			*(ptr++)=lazwall_start;
			*(ptr++)=b_mesg_row;
			*(ptr++)=b_mesg_col;
			mem_cpy((char far *)ptr,(char far *)&scroll_x,2);
			ptr+=2;
			mem_cpy((char far *)ptr,(char far *)&scroll_y,2);
			ptr+=2;
			mem_cpy((char far *)ptr,(char far *)&locked_x,2);
			ptr+=2;
			mem_cpy((char far *)ptr,(char far *)&locked_y,2);
			ptr+=2;
			*(ptr++)=player_ns_locked;
			*(ptr++)=player_ew_locked;
			*(ptr++)=player_attack_locked;
			*(ptr++)=volume;
			*(ptr++)=volume_inc;
			*(ptr++)=volume_target;
#endif
			//Robot count
			count=0;
			for(t1=1;t1<NUM_ROBOTS;t1++)
				if(robots[t1].used)	count=t1;
			*(ptr++)=count;
			//Robots themselves
			prepare_robot_mem();
			if(count)
			{
				for(t1=1;t1<=count;t1++)
				{
					//Copy robot t1
					mem_cpy((char far *)ptr,(char far *)&robots[t1],sizeof(Robot));
					ptr+=sizeof(Robot);
					normalize(ptr);
					mem_cpy((char far *)ptr,
									(char far *)&robot_mem[robots[t1].program_location],
									robots[t1].program_length);
					ptr+=robots[t1].program_length;
				}
			}
			//Scroll count
			count=0;
			for(t1=1;t1<NUM_SCROLLS;t1++)
				if(scrolls[t1].used) count=t1;
			*(ptr++)=count;
			//Scrolls themselves
			if(count)
			{
				for(t1=1;t1<=count;t1++)
				{
					//Copy scroll t1
					mem_cpy((char far *)ptr,(char far *)&scrolls[t1],sizeof(Scroll));
					ptr+=sizeof(Scroll);
					normalize(ptr);
					mem_cpy((char far *)ptr,
									(char far *)&robot_mem[scrolls[t1].mesg_location],
									scrolls[t1].mesg_size);
					ptr+=scrolls[t1].mesg_size;
				}
			}
			//Sensor count
			count=0;
			for(t1=1;t1<NUM_SENSORS;t1++)
				if(sensors[t1].used) count=t1;
			*(ptr++)=count;
			//Sensors themselves
			if(count)
			{
				for(t1=1;t1<=count;t1++)
				{
					//Copy sensor t1
					mem_cpy((char far *)ptr,(char far *)&sensors[t1],sizeof(Sensor));
					ptr+=sizeof(Sensor);
				}
			}
			//All copied! Return if memory...
			if(board_where[id]==W_MEMORY)	return 0;
			//...otherwise copy to EMS.
			//Copy a page at a time...
			t1=board_offsets[id].EMS.page;//Current page
			copied=0;//Copied nothing so far.
			ptr=old_ptr;//Use original pointer
			do
			{
				//Map page
				map_page_EMS(board_offsets[id].EMS.handle,0,t1);
				//Copy a page, or part of a page if that's all that's left.
				tcpy=16384;
				if((copied+tcpy)>board_sizes[id])	//Too much...
					tcpy=(int)(board_sizes[id]-(unsigned long)copied);
				//Now copy it...
				mem_cpy(page_frame_EMS,(char far *)ptr,tcpy);
				//...increment status...
				copied+=tcpy;
				ptr+=tcpy;
				normalize(ptr);
				t1++;//Page
				//...loop.
			} while(copied<board_sizes[id]);
			//All copied! Free memory and exit
			farfree(old_ptr);
			return 0;
		case W_TEMPFILE:
			store_tempfile:
			//This is storage in a tempfile, sometimes a subfuction of storage
			//in EMS.
			//Open file...
			fp=fopen(&board_filenames[id*FILENAME_SIZE],"rb+");
			if(fp==NULL)
			{
				if(board_where[id]==W_EMS) board_filenames[id*FILENAME_SIZE]=0;
				return 1;//Error...
			}
			//...and begin output!
			fputc(max_bsiz_mode,fp);
			//Store RLE2 stuff
			if(overlay_mode)
			{
				fputc(0,fp);
				fputc(overlay_mode,fp);
				RLE2_save(fp,overlay);
				RLE2_save(fp,overlay_color);
			}
			RLE2_save(fp,level_id);
			RLE2_save(fp,level_color);
			RLE2_save(fp,level_param);
			RLE2_save(fp,level_under_id);
			RLE2_save(fp,level_under_color);
			RLE2_save(fp,level_under_param);
			//Store variables- Due to the way they are stored in DATA.ASM,
			//they can all be stored as a series of 210 bytes starting at
			//mod_playing. Appropriate code to save each individually is
			//also included in case this ever fails. To use the individual
			//code, define the symbol SAVE_INDIVIDUAL.
#ifndef SAVE_INDIVIDUAL
			fwrite(mod_playing,1,207+FILENAME_SIZE,fp);
#else
			fwrite(mod_playing,1,FILENAME_SIZE,fp);
			fputc(viewport_x,fp);
			fputc(viewport_y,fp);
			fputc(viewport_xsiz,fp);
			fputc(viewport_ysiz,fp);
			fputc(can_shoot,fp);
			fputc(can_bomb,fp);
			fputc(fire_burn_brown,fp);
			fputc(fire_burn_space,fp);
			fputc(fire_burn_fakes,fp);
			fputc(fire_burn_trees,fp);
			fputc(explosions_leave,fp);
			fputc(save_mode,fp);
			fputc(forest_becomes,fp);
			fputc(collect_bombs,fp);
			fputc(fire_burns,fp);
			fwrite(board_dir,1,4,fp);
			fputc(restart_if_zapped,fp);
			fwrite(&time_limit,2,1,fp);
			fputc(last_key,fp);
			fwrite(&num_input,2,1,fp);
			fputc(input_size,fp);
			fwrite(input_string,1,81,fp);
			fputc(player_last_dir,fp);
			fwrite(bottom_mesg,1,81,fp);
			fputc(b_mesg_timer,fp);
			fputc(lazwall_start,fp);
			fputc(b_mesg_row,fp);
			fputc(b_mesg_col,fp);
			fwrite(&scroll_x,2,1,fp);
			fwrite(&scroll_y,2,1,fp);
			fwrite(&locked_x,2,1,fp);
			fwrite(&locked_y,2,1,fp);
			fputc(player_ns_locked,fp);
			fputc(player_ew_locked,fp);
			fputc(player_attack_locked,fp);
			fputc(volume,fp);
			fputc(volume_inc,fp);
			fputc(volume_target,fp);
#endif
			//Robot count
			count=0;
			for(t1=1;t1<NUM_ROBOTS;t1++)
				if(robots[t1].used)	count=t1;
			fputc(count,fp);
			//Robots themselves
			prepare_robot_mem();
			if(count)
			{
				for(t1=1;t1<=count;t1++)
				{
					//Copy robot t1
					fwrite(&robots[t1],sizeof(Robot),1,fp);
					fwrite(&robot_mem[robots[t1].program_location],1,
								 robots[t1].program_length,fp);
				}
			}
			//Scroll count
			count=0;
			for(t1=1;t1<NUM_SCROLLS;t1++)
				if(scrolls[t1].used) count=t1;
			fputc(count,fp);
			//Scrolls themselves
			if(count)
			{
				for(t1=1;t1<=count;t1++)
				{
					//Copy scroll t1
					fwrite(&scrolls[t1],sizeof(Scroll),1,fp);
					fwrite(&robot_mem[scrolls[t1].mesg_location],1,
								 scrolls[t1].mesg_size,fp);
				}
			}
			//Sensor count
			count=0;
			for(t1=1;t1<NUM_SENSORS;t1++)
				if(sensors[t1].used) count=t1;
			fputc(count,fp);
			//Sensors themselves
			if(count)
			{
				for(t1=1;t1<=count;t1++) //Copy sensor t1
					fwrite(&sensors[t1],sizeof(Sensor),1,fp);
			}
			//All saved! Return if tempfile...
			if(board_where[id]==W_TEMPFILE)
			{
				fclose(fp);
				return 0;
			}
			//...otherwise copy to EMS.
			//Copy a page at a time...
			t1=board_offsets[id].EMS.page;//Current page
			copied=0;//Copied nothing so far.
			fseek(fp,0,SEEK_SET);//Return to start of file
			do
			{
				//Map page
				map_page_EMS(board_offsets[id].EMS.handle,0,t1);
				//Copy a page, or part of a page if that's all that's left.
				tcpy=16384;
				if((copied+tcpy)>board_sizes[id])	//Too much...
					tcpy=(int)(board_sizes[id]-(unsigned long)copied);
				//Now copy it...
				fread(page_frame_EMS,tcpy,1,fp);
				//...increment status...
				copied+=tcpy;
				t1++;//Page
				//...loop.
			} while(copied<board_sizes[id]);
			//All copied! Clear file and exit
			fclose(fp);
			unlink(&board_filenames[id*FILENAME_SIZE]);
			board_filenames[id*FILENAME_SIZE]=0;
			return 0;
	}
	return 3;
}

//Loads current board from slot given. Space must already be filled.
//board_sizes and other board variables MUST be accurate. Returns non-0
//if out of room in memory and on disk for working, or for misc. errors.
//Returns 4 if out of robot memory. (This is usually due to a large current
//robot/scroll or a large global robot) Does not affect current object or
//global robot.
char grab_current(unsigned char id)
{
	unsigned char far *ptr;//Where to grab from for mem/ems
	unsigned char far *old_ptr;//Saves old ems allocated ptr
	FILE *fp;//Where to grab from for tempfile (or ems if pressed for space)
	int t1,count,tcpy;
	unsigned int oldsize,newsize,oldloc;//For robot/scroll allocation
	long copied;
	//Space of proper size already assumed to be allocated. This function
	//has two major parts- The code for memory/ems, and the code for temp
	//files.
	//Switch according to storage type.

	switch(board_where[id])
	{
		case W_EMS:
			//If EMS, allocate a temporary area to allow storage in conventional
			//memory for now, then copy to robots/etc. later. If not possible,
			//we use a temp file and then copy THAT to mem. later. If no room
			//for a temp file... return error.
		 //Allocate a temporary area
		 ptr=(unsigned char far *)farmalloc(board_sizes[id]);
		 if(ptr==NULL)
		 {
		 	//No room in mem- try a temp file
		 	//Make temporary filename
		 	str_cpy(&board_filenames[id*FILENAME_SIZE],"~MZTMPXXXXXX");
		 	mktemp(&board_filenames[id*FILENAME_SIZE]);
		 	//Open..
		 	fp=fopen(&board_filenames[id*FILENAME_SIZE],"wb+");
		 	if(fp==NULL)
		 	{
		 		//Error
		 		board_filenames[id*FILENAME_SIZE]=0;
		 		return 1;
		 	}
		 	//Try to make room
		 	fseek(fp,board_sizes[id],SEEK_SET);
		 	if(ftell(fp)!=(board_sizes[id]))
		 	{
		 		//Not enough room on hard drive
		 		fclose(fp);
		 		unlink(&board_filenames[id*FILENAME_SIZE]);
		 		board_filenames[id*FILENAME_SIZE]=0;
		 		return 2;
		 	}
		 	//Room made! Solidify.
		 	fseek(fp,-1,SEEK_CUR);
		 	fputc(0,fp);
		 	//Load from EMS
		 	//Copy a page at a time...
		 	t1=board_offsets[id].EMS.page;//Current page
		 	copied=0;//Copied nothing so far.
		 	fseek(fp,0,SEEK_SET);//Return to start of file
		 	do
		 	{
		 		//Map page
		 		map_page_EMS(board_offsets[id].EMS.handle,0,t1);
		 		//Copy a page, or part of a page if that's all that's left.
		 		tcpy=16384;
		 		if((copied+tcpy)>board_sizes[id])	//Too much...
		 			tcpy=(int)(board_sizes[id]-(unsigned long)copied);
		 		//Now copy it...
		 		fwrite(page_frame_EMS,tcpy,1,fp);
		 		//...increment status...
		 		copied+=tcpy;
		 		t1++;//Page
		 		//...loop.
		 	} while(copied<board_sizes[id]);
		 	//All copied! Close file and jump to tempfile code
		 	fclose(fp);
		 	goto grab_tempfile;
		 }
		 //Allocated. Copy from EMS to memory first...
		 old_ptr=ptr;
		 //Copy a page at a time...
		 t1=board_offsets[id].EMS.page;//Current page
		 copied=0;//Copied nothing so far.
		 do
		 {
		 	//Map page
		 	map_page_EMS(board_offsets[id].EMS.handle,0,t1);
		 	//Copy a page, or part of a page if that's all that's left.
		 	tcpy=16384;
		 	if((copied+tcpy)>board_sizes[id])	//Too much...
		 		tcpy=(int)(board_sizes[id]-(unsigned long)copied);
		 	//Now copy it...
		 	mem_cpy((char far *)ptr,page_frame_EMS,tcpy);
		 	//...increment status...
		 	copied+=tcpy;
		 	ptr+=tcpy;
		 	normalize(ptr);
		 	t1++;//Page
		 	//...loop.
		 } while(copied<board_sizes[id]);
		 //All copied! Now do regular memory code.
		 ptr=old_ptr;//Restore pointer
		 goto grab_memory;
		
		case W_MEMORY:
			ptr=(unsigned char far *)board_offsets[id].mem;

			grab_memory:

			//Clear overlay
			overlay_mode=0;
			for(t1=0;t1<10000;t1++)
			{
				level_id[t1]=level_under_id[t1]=level_param[t1]=level_under_param[t1]=0;
				level_color[t1]=level_under_color[t1]=overlay_color[t1]=7;
				overlay[t1]=32;
			}
			max_bsiz_mode=*(ptr++);
			convert_max_bsiz_mode();
			//Load RLE2 stuff
		
			// Oh Greg NO. According to the board specs, the first byte being 0 here
			// means that there's no overlay. Otherwise, this byte is the first byte of
			// the board width for the level stuff. But what Greg didn't realize is that
			// the first byte can be 0 for a board without overlay if the width is 256.
			// So if the width is 256, it will try to load an overlay that's not there and
			// totally eat the game. Solution? Um... well you can't just check the entire
			// word because the latter 8bits won't necessarily be 0. So 0 1 bytes mean
			// either 256 width no overlay, or overlay mode 1, you can't tell the
			// difference. You can guess by looking at the word following this one to see
			// if it's less than 26 (which it must be for the board to be 256 wide) but
			// this value in overlay mode may be less than 256.
			if(*ptr == 0)
			{
				//Overlay
				ptr++;
				overlay_mode=*(ptr++);
				ptr+=RLE2_read((unsigned char far *)ptr,overlay);
				normalize(ptr);
				ptr+=RLE2_read((unsigned char far *)ptr,overlay_color);
				normalize(ptr);
			}

			ptr+=RLE2_read((unsigned char far *)ptr,level_id);
			normalize(ptr);
			ptr+=RLE2_read((unsigned char far *)ptr,level_color);
			normalize(ptr);
			ptr+=RLE2_read((unsigned char far *)ptr,level_param);
			normalize(ptr);
			ptr+=RLE2_read((unsigned char far *)ptr,level_under_id);
			normalize(ptr);
			ptr+=RLE2_read((unsigned char far *)ptr,level_under_color);
			normalize(ptr);
			ptr+=RLE2_read((unsigned char far *)ptr,level_under_param);
			normalize(ptr);

			//Read variables- Due to the way they are stored in DATA.ASM,
			//they can all be read as a series of 210 bytes starting at
			//mod_playing. Appropriate code to read each individually is
			//also included in case this ever fails. To use the individual
			//code, define the symbol SAVE_INDIVIDUAL.

#ifndef SAVE_INDIVIDUAL
			mem_cpy(mod_playing,(char far *)ptr,207+FILENAME_SIZE);
			ptr+=207+FILENAME_SIZE;
#else
			mem_cpy(mod_playing,(char far *)ptr,FILENAME_SIZE);
			ptr+=FILENAME_SIZE;
			viewport_x=*(ptr++);

			viewport_y=*(ptr++);
			viewport_xsiz=*(ptr++);
			viewport_ysiz=*(ptr++);
			can_shoot=*(ptr++);
			can_bomb=*(ptr++);
			fire_burn_brown=*(ptr++);
			fire_burn_space=*(ptr++);
			fire_burn_fakes=*(ptr++);
			fire_burn_trees=*(ptr++);
			explosions_leave=*(ptr++);
			save_mode=*(ptr++);
			forest_becomes=*(ptr++);
			collect_bombs=*(ptr++);
			fire_burns=*(ptr++);
			mem_cpy((char far *)board_dir,(char far *)ptr,4);
			ptr+=4;
			restart_if_zapped=*(ptr++);
			mem_cpy((char far *)&time_limit,(char far *)ptr,2);
			ptr+=2;
			last_key=*(ptr++);
			mem_cpy((char far *)&num_input,(char far *)ptr,2);
			ptr+=2;
			input_size=*(ptr++);
			mem_cpy(input_string,(char far *)ptr,81);
			ptr+=81;
			player_last_dir=*(ptr++);
			mem_cpy(bottom_mesg,(char far *)ptr,81);
			ptr+=81;
			b_mesg_timer=*(ptr++);
			lazwall_start=*(ptr++);
			b_mesg_row=*(ptr++);
			b_mesg_col=*(ptr++);
			mem_cpy((char far *)&scroll_x,(char far *)ptr,2);
			ptr+=2;
			mem_cpy((char far *)&scroll_y,(char far *)ptr,2);
			ptr+=2;
			mem_cpy((char far *)&locked_x,(char far *)ptr,2);
			ptr+=2;
			mem_cpy((char far *)&locked_y,(char far *)ptr,2);
			ptr+=2;
			player_ns_locked=*(ptr++);
			player_ew_locked=*(ptr++);
			player_attack_locked=*(ptr++);
			volume=*(ptr++);
			volume_inc=*(ptr++);
			volume_target=*(ptr++);
#endif

			//Before doing robots/scrolls, make sure all but #0 and global
			//are allocated to the minimum. Also clear all sensors.
			prepare_robot_mem();

			for(t1=1;t1<NUM_ROBOTS;t1++)
				clear_robot(t1);
 
			for(t1=1;t1<NUM_SCROLLS;t1++)
				clear_scroll(t1);

			for(t1=1;t1<NUM_SENSORS;t1++)
				clear_sensor(t1);

			//Robots- get count, then load 'em up. All must be allocated
			//first. A mis-allocation frees any ems memory and returns error
			//code #4.

			count=*(ptr++);
			if(count)
			{
				for(t1=1;t1<=count;t1++)
				{
					//Read robot t1
					oldsize=robots[t1].program_length;
					oldloc=robots[t1].program_location;
					mem_cpy((char far *)&robots[t1],(char far *)ptr,sizeof(Robot));
					ptr+=sizeof(Robot);
					normalize(ptr);
					newsize=robots[t1].program_length;
					robots[t1].program_length=oldsize;
					robots[t1].program_location=oldloc;
					if(reallocate_robot_mem(T_ROBOT,t1,newsize))
					{
						//Error in allocation.
						//EMS- delete temp area
						if(board_where[id]==W_EMS) farfree(ptr);
						return 4;
					}
					//Load in robot
					mem_cpy((char far *)&robot_mem[robots[t1].program_location],
									(char far *)ptr,robots[t1].program_length);
					ptr+=robots[t1].program_length;
				}
			}

			//Scroll count
			count=*(ptr++);
			if(count)
			{
				for(t1=1;t1<=count;t1++)
				{
					//Read scroll t1
					oldsize=scrolls[t1].mesg_size;
					oldloc=scrolls[t1].mesg_location;
					mem_cpy((char far *)&scrolls[t1],(char far *)ptr,sizeof(Scroll));
					ptr+=sizeof(Scroll);
					normalize(ptr);
					newsize=scrolls[t1].mesg_size;
					scrolls[t1].mesg_size=oldsize;
					scrolls[t1].mesg_location=oldloc;
					if(reallocate_robot_mem(T_SCROLL,t1,newsize))
					{
						//Error in allocation.
						//EMS- delete temp area
						if(board_where[id]==W_EMS) farfree(ptr);
						return 4;
					}
					mem_cpy((char far *)&robot_mem[scrolls[t1].mesg_location],
									(char far *)ptr,scrolls[t1].mesg_size);
					ptr+=scrolls[t1].mesg_size;
				}
			}

			//Sensor count
			count=*(ptr++);
			if(count)
			{
				for(t1=1;t1<=count;t1++)
				{
					//Read sensor t1
					mem_cpy((char far *)&sensors[t1],(char far *)ptr,sizeof(Sensor));
					ptr+=sizeof(Sensor);
				}
			}
			//All grabbed! Return if memory...
			if(board_where[id]==W_MEMORY)	return 0;
			//...otherwise free EMS temp memory.
			farfree(old_ptr);
			return 0;
		case W_TEMPFILE:
			grab_tempfile:
			//This is loading from a tempfile, sometimes a subfuction of loading
			//from EMS.
			//Open file...
			fp=fopen(&board_filenames[id*FILENAME_SIZE],"rb");
			if(fp==NULL)
			{
				if(board_where[id]==W_EMS) board_filenames[id*FILENAME_SIZE]=0;
				return 1;//Error...
			}
			//...and begin input!
			//Clear overlay
			overlay_mode=0;
			for(t1=0;t1<10000;t1++)
			{
				level_id[t1]=level_under_id[t1]=level_param[t1]=level_under_param[t1]=0;
				level_color[t1]=level_under_color[t1]=overlay_color[t1]=7;
				overlay[t1]=32;
			}
			max_bsiz_mode=fgetc(fp);
			convert_max_bsiz_mode();
			//Overlay
			if(fgetc(fp)==0)
			{
				overlay_mode=fgetc(fp);
				RLE2_load(fp,overlay);
				RLE2_load(fp,overlay_color);
			}
			else fseek(fp,-1,SEEK_CUR);
			//load RLE2 stuff
			RLE2_load(fp,level_id);
			RLE2_load(fp,level_color);
			RLE2_load(fp,level_param);
			RLE2_load(fp,level_under_id);
			RLE2_load(fp,level_under_color);
			RLE2_load(fp,level_under_param);
			//Load variables- Due to the way they are stored in DATA.ASM,
			//they can all be loaded as a series of 210 bytes starting at
			//mod_playing. Appropriate code to load each individually is
			//also included in case this ever fails. To use the individual
			//code, define the symbol SAVE_INDIVIDUAL.
#ifndef SAVE_INDIVIDUAL
			fread(mod_playing,1,207+FILENAME_SIZE,fp);
#else
			fread(mod_playing,1,FILENAME_SIZE,fp);
			viewport_x=fgetc(fp);

			viewport_y=fgetc(fp);
			viewport_xsiz=fgetc(fp);
			viewport_ysiz=fgetc(fp);
			can_shoot=fgetc(fp);
			can_bomb=fgetc(fp);
			fire_burn_brown=fgetc(fp);
			fire_burn_space=fgetc(fp);
			fire_burn_fakes=fgetc(fp);
			fire_burn_trees=fgetc(fp);
			explosions_leave=fgetc(fp);
			save_mode=fgetc(fp);
			forest_becomes=fgetc(fp);
			collect_bombs=fgetc(fp);
			fire_burns=fgetc(fp);
			fread(board_dir,1,4,fp);
			restart_if_zapped=fgetc(fp);
			fread(&time_limit,2,1,fp);
			last_key=fgetc(fp);
			fread(&num_input,2,1,fp);
			input_size=fgetc(fp);
			fread(input_string,1,81,fp);
			player_last_dir=fgetc(fp);
			fread(bottom_mesg,1,81,fp);
			b_mesg_timer=fgetc(fp);
			lazwall_start=fgetc(fp);
			b_mesg_row=fgetc(fp);
			b_mesg_col=fgetc(fp);
			fread(&scroll_x,2,1,fp);
			fread(&scroll_y,2,1,fp);
			fread(&locked_x,2,1,fp);
			fread(&locked_y,2,1,fp);
			player_ns_locked=fgetc(fp);
			player_ew_locked=fgetc(fp);
			player_attack_locked=fgetc(fp);
			volume=fgetc(fp);
			volume_inc=fgetc(fp);
			volume_target=fgetc(fp);
#endif
			//Before doing robots/scrolls, make sure all but #0 and global
			//are allocated to the minimum. Also clear all sensors.
			prepare_robot_mem();
			for(t1=1;t1<NUM_ROBOTS;t1++)
				clear_robot(t1);
			for(t1=1;t1<NUM_SCROLLS;t1++)
				clear_scroll(t1);
			for(t1=1;t1<NUM_SENSORS;t1++)
				clear_sensor(t1);
			//Robots- get count, then load 'em up. All must be allocated
			//first. A mis-allocation frees any ems memory and returns error
			//code #4.
			count=fgetc(fp);
			if(count)
			{
				for(t1=1;t1<=count;t1++)
				{
					//Read robot t1
					oldsize=robots[t1].program_length;
					oldloc=robots[t1].program_location;
					fread(&robots[t1],sizeof(Robot),1,fp);
					newsize=robots[t1].program_length;
					robots[t1].program_length=oldsize;
					robots[t1].program_location=oldloc;
					if(reallocate_robot_mem(T_ROBOT,t1,newsize))
					{
						//Error in allocation.
						fclose(fp);
						//EMS- delete temp file
						if(board_where[id]==W_EMS)
						{
							unlink(&board_filenames[id*FILENAME_SIZE]);
							board_filenames[id*FILENAME_SIZE]=0;
						}
						return 4;
					}
					//Load in robot
					fread(&robot_mem[robots[t1].program_location],1,
								robots[t1].program_length,fp);
				}
			}
			//Scroll count
			count=fgetc(fp);
			if(count)
			{
				for(t1=1;t1<=count;t1++)
				{
					//Read scroll t1
					oldsize=scrolls[t1].mesg_size;
					oldloc=scrolls[t1].mesg_location;
					fread(&scrolls[t1],sizeof(Scroll),1,fp);
					newsize=scrolls[t1].mesg_size;
					scrolls[t1].mesg_size=oldsize;
					scrolls[t1].mesg_location=oldloc;
					if(reallocate_robot_mem(T_SCROLL,t1,newsize))
					{
						//Error in allocation.
						fclose(fp);
						//EMS- delete temp file
						if(board_where[id]==W_EMS)
						{
							unlink(&board_filenames[id*FILENAME_SIZE]);
							board_filenames[id*FILENAME_SIZE]=0;
						}
						return 4;
					}
					fread(&robot_mem[scrolls[t1].mesg_location],1,
								scrolls[t1].mesg_size,fp);
				}
			}
			//Sensor count
			count=fgetc(fp);
			if(count)
			{
				for(t1=1;t1<=count;t1++) //Read sensor t1
					fread(&sensors[t1],sizeof(Sensor),1,fp);
			}
			//All loaded! Return if tempfile...
			fclose(fp);
			if(board_where[id]==W_TEMPFILE)	return 0;
			//...otherwise erase EMS file.
			unlink(&board_filenames[id*FILENAME_SIZE]);
			board_filenames[id*FILENAME_SIZE]=0;
			return 0;
	}
	return 3;
}

//Loads OR saves board of given id to/from an already open file. Space
//must already be allocated and board_sizes, etc must be set properly.
//All read characters are xor'd with xor_with, although not if it is 0,
//to save time for the same result. Returns non-0 for misc. errors.
//Set loading to non-0 to load FROM file, 0 to save TO file.
char disk_board(unsigned char id,FILE *fp,char loading,
								unsigned char xor_with)
{
	unsigned int siz,cpg1,buffsize;
	long copied=0,tmp;
	char far *ptr;
	FILE *destfp;
	//Switch according to storage type.
	switch(board_where[id])
	{
		case W_MEMORY:
			//Simply do a fread then a mem_xor. Must be read in chunks in case
			//size is larger than an unsigned int.
			ptr=(char far *)board_offsets[id].mem;
			do
			{
				siz=(unsigned int)(board_sizes[id]-copied);
				if((board_sizes[id]-copied)>32768U)	siz=32768U;
				if(!loading)//If saving, must xor first, but later xor again
					//to restore memory copy to normal.
					if(xor_with) mem_xor(ptr,siz,xor_with);
				if(loading)	fread(ptr,siz,1,fp);//LOAD
				else fwrite(ptr,siz,1,fp);//SAVE
				if(xor_with) mem_xor(ptr,siz,xor_with);
				copied+=siz;
				ptr+=siz;
			} while(copied<board_sizes[id]);
			//Done!
			return 0;
		case W_EMS:
			//Simply do a fread then a mem_xor. Read in chunks of one page.
			cpg1=board_offsets[id].EMS.page;
			do
			{
				map_page_EMS(board_offsets[id].EMS.handle,0,cpg1);
				siz=(unsigned int)(board_sizes[id]-copied);
				if((board_sizes[id]-copied)>16384) siz=16384;
				if(!loading)//If saving, must xor first, but later xor again
					//to restore memory copy to normal.
					if(xor_with) mem_xor(page_frame_EMS,siz,xor_with);
				if(loading)	fread(page_frame_EMS,siz,1,fp);//LOAD
				else fwrite(page_frame_EMS,siz,1,fp);//SAVE
				if(xor_with) mem_xor(page_frame_EMS,siz,xor_with);
				copied+=siz;
				cpg1++;
			} while(copied<board_sizes[id]);
			//Done!
			return 0;
		case W_TEMPFILE:
			//First, verify file can be opened...
			destfp=fopen(&board_filenames[id*FILENAME_SIZE],"rb+");
			if(destfp==NULL) return 1;
			//This one is easier to program but slower. Allocates a buffer
			//in conventional memory as large as possible (up to 32k) then
			//buffers from fp to dest file, xor'ing if required. If buffer
			//cannot be allocated, does it directly. (IE one byte at a time)
			copied=farcoreleft();
			if(copied>32768U)	buffsize=32768U;
			else buffsize=(unsigned int)copied;
			if(buffsize>=128)
			{
				//Allocate buffer
				ptr=(char far *)farmalloc(buffsize);
				if(ptr==NULL)	//Huh? Ok we'll do singles..
					goto transfer_bytewise;
				//Do transferring
				copied=0;
				do
				{
					//Pick size
					siz=((unsigned int)(board_sizes[id]-copied));
					if(board_sizes[id]>buffsize) siz=buffsize;
					//Read
					if(loading)	fread(ptr,siz,1,fp);
					else fread(ptr,siz,1,destfp);
					//XOR
					if(xor_with) mem_xor(ptr,siz,xor_with);
					//Write
					if(loading)	fwrite(ptr,siz,1,destfp);
					else fwrite(ptr,siz,1,fp);
					//Update variables
					copied+=siz;
				} while(copied<board_sizes[id]);
				//Done! Close file and deallocate buffer.
				farfree(ptr);
				fclose(destfp);
				return 0;
			}
			transfer_bytewise:
			//Couldn't allocate memory buffer. Do transfer byte by byte...
			tmp=board_sizes[id];
			if(xor_with)
			{
				for(;copied<tmp;copied++)
				{//Read, XOR, and write
					if(loading)	fputc(fgetc(fp)^xor_with,destfp);
					else fputc(fgetc(destfp)^xor_with,fp);
				}
			}
			else
			{
				for(;copied<tmp;copied++)
				{//Read, XOR, and write
					if(loading)	fputc(fgetc(fp),destfp);
					else fputc(fgetc(destfp),fp);
				}
			}
			//Done! Close file.
			fclose(destfp);
			return 0;
	}
	//Misc. error
	return 2;
}

//Attempts to clear up conventional memory by moving all boards in
//conventional memory to either EMS or disk. If there is no more room on
//disk/EMS, then boards are no longer moved. Runs a meter.

void free_up_board_memory(void)
{
	int t1,t2,nmb=0,nb_done=0;
	unsigned char far *tmp_ptr;
	unsigned char far *ptr;
	long copied;
	unsigned int tcpy;
	char temp=curr_rmem_status;
	FILE *fp;
	//Count boards
	for(t1=0;t1<NUM_BOARDS;t1++)
	{
		if(board_where[t1]==W_MEMORY)	nmb++;
	}
	if(nmb==0) return;//None to swap!
	save_screen(current_pg_seg);
	meter("Swapping boards to disk/EMS...",current_pg_seg,nb_done,nmb);
	//Go through the boards, picking out those in conventional memory...
	for(t1=0;t1<NUM_BOARDS;t1++)
	{
		if(board_where[t1]==W_MEMORY)
		{
			//Ok, board #t1 is in conventional memory. Now we need to try to
			//allocate a new area for it in anything BUT conventional memory.
			//First, since the allocate function destroys the allocation
			//variables, we need to save board_offset. (board_where is obvious
			//and board_sizes/board_filenames are not affected)
			tmp_ptr=board_offsets[t1].mem;
			if(allocate_board_space(board_sizes[t1],t1,0))
			{
				//No room. Restore variables and return.
				board_offsets[t1].mem=tmp_ptr;
				board_where[t1]=W_MEMORY;
				restore_screen(current_pg_seg);
				return;
			}
			//Aha, new area allocated. Switch for EMS or disk.
			switch(board_where[t1])
			{
				case W_EMS:
					//Copy to EMS from tmp_ptr for board_sizes[t1] bytes.
					//Copy a page at a time...
					t2=board_offsets[t1].EMS.page;//Current page
					copied=0;//Copied nothing so far.
					ptr=tmp_ptr;//Use a copy of the ptr so we save it
					do
					{
						//Map page
						map_page_EMS(board_offsets[t1].EMS.handle,0,t2);
						//Copy a page, or part of a page if that's all that's left.
						tcpy=16384;
						if((copied+tcpy)>board_sizes[t1])	//Too much...
							tcpy=(int)(board_sizes[t1]-(unsigned long)copied);
						//Now copy it...
						mem_cpy(page_frame_EMS,(char far *)ptr,tcpy);
						//...increment status...
						copied+=tcpy;
						ptr+=tcpy;
						t2++;//Page
						//...loop.
					} while(copied<board_sizes[t1]);
					//All copied! Free old memory for board and we're done.
					farfree(tmp_ptr);
					break;
				case W_TEMPFILE:
					//We're saving memory to a disk file. REAL simple.
					fp=fopen(&board_filenames[t1*FILENAME_SIZE],"rb+");
					if(fp==NULL)
					{
						//Not gonna work.
						board_offsets[t1].mem=tmp_ptr;
						board_where[t1]=W_MEMORY;
						board_filenames[t1*FILENAME_SIZE]=0;
						break;
					}
					//Do in chunks of 32768 bytes.
					ptr=tmp_ptr;
					copied=0;
					do
					{
						tcpy=(unsigned int)board_sizes[t1];
						if(board_sizes[t1]>32768U) tcpy=32768U;
						fwrite(ptr,tcpy,1,fp);//SAVE
						copied+=tcpy;
						ptr+=tcpy;
					} while(copied<board_sizes[t1]);
					//Done. Close file and free memory.
					fclose(fp);
					farfree(tmp_ptr);
					break;
				default:
					//Error.
					restore_screen(current_pg_seg);
					error("Error accessing boards",2,20,current_pg_seg,0x0701);
			}
			//All done with THIS board.
			meter_interior(current_pg_seg,++nb_done,nmb);
		}
		//Loop to next board.
	}
	restore_screen(current_pg_seg);
	//All done totally.
	prepare_robot_mem(temp);
}

void convert_max_bsiz_mode(void)
{
	switch(max_bsiz_mode)
	{
		case 0:
			max_bxsiz=60;
			max_bysiz=166;
			break;
		case 1:
			max_bxsiz=80;
			max_bysiz=125;
			break;
		default:
			max_bxsiz=100;
			max_bysiz=100;
			break;
		case 3:
			max_bxsiz=200;
			max_bysiz=50;
			break;
		case 4:
			max_bxsiz=400;
			max_bysiz=25;
			break;
	}
}
