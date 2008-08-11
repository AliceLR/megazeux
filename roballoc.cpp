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

/* Code to allocate, deallocate, reallocate, and otherwise keep track of
	robot/scroll memory. (a 64k chunk in either EMS or main memory if there
	isn't enough room in EMS) */

#include "beep.h"
#include <dos.h>
#include "meminter.h"
#include <_null.h>
#include "ems.h"
#include "roballoc.h"
#include "const.h"
#include "struct.h"
#include "data.h"
#include "string.h"

//Where robot memory is (W_NOWHERE, W_EMS, or W_MEMORY)
char robot_mem_type=W_NOWHERE;
unsigned char far *robot_mem=NULL;//Set into EMS or reg. memory
unsigned char far *rm_mirror=NULL;//Mirror of above variable
int robot_ems=0;
int rob_global_ems=0;
unsigned int robot_free_mem=0;
char curr_rmem_status=-1;

//Technique of storage- All robots are set to length of 2, all scrolls to
//length of 3. Empty programs. Stored in memory in order of- scrolls 1-??,
//robots 1-??, not including global robot. Minimum of 16 bytes allocated
//to each, then increased in chunks of 16. Increasing one moves all others
//forward. Decreasing moves all backwards. Another page of memory has scroll
//0, robot 0, and global robot.

//Returns non-0 for out of memory.
int init_robot_mem(void) {
	unsigned int t1,t2;
	//First reset all scrolls, robots, counters, and sensors.
	for(t1=0;t1<=NUM_ROBOTS;t1++) {//Include global
		for(t2=0;t2<sizeof(Robot);t2++) {
			((char far *)(&robots[t1]))[t2]=0;
			}
		robots[t1].program_length=2;
		robots[t1].robot_cycle=1;
		robots[t1].cur_prog_line=1;
		robots[t1].robot_char=2;
		robots[t1].bullet_type=1;
		}
	for(t1=0;t1<NUM_SCROLLS;t1++) {
		scrolls[t1].num_lines=1;
		scrolls[t1].mesg_location=scrolls[t1].used=0;
		scrolls[t1].mesg_size=3;
		}
	for(t1=0;t1<NUM_COUNTERS;t1++) {
		counters[t1].counter_name[0]=counters[t1].counter_value=0;
		}
	for(t1=0;t1<NUM_SENSORS;t1++) {
		sensors[t1].sensor_name[0]=sensors[t1].robot_to_mesg[0]=
			sensors[t1].sensor_char=sensors[t1].used=0;
		}
	//Allocate robot memory (2 pgs of 64k)
	//Try EMS first
	t1=alloc_EMS(4);//4 pages of 16k each
	if(t1) {
		robot_ems=t1;
		t1=alloc_EMS(4);
		if(t1) {
			//Got it! :)
			robot_mem_type=W_EMS;
			rob_global_ems=t1;
			robot_mem=(unsigned char far *)page_frame_EMS;
			}
		else {
			free_EMS(t1);
			goto try_conv;
			}
		}
	else {
	try_conv:
		//Try conventional memory.
		robot_mem=(unsigned char far *)farmalloc(131072L);
		if(robot_mem==NULL) return 1;//Out of memory
		//Conventional works.
		robot_mem_type=W_MEMORY;
		robot_ems=rob_global_ems=0;
		}
	rm_mirror=robot_mem;
	//Now we have to work with storage
	prepare_robot_mem();
	//Setup initial storage- scrolls[] and robots[].
	t1=0;//Current working location
	//Do this for each scroll
	for(t2=1;t2<NUM_SCROLLS;t2++) {
		scrolls[t2].mesg_location=t1;
		robot_mem[t1]=0x01;//Start code
		robot_mem[t1+1]='\n';//End of line code
		robot_mem[t1+2]=0x00;//End code
		t1+=16;
		}
	//Now do this for each robot
	for(t2=1;t2<NUM_ROBOTS;t2++) {
		robots[t2].program_location=t1;
		robot_mem[t1]=0xFF;//Start code
		robot_mem[t1+1]=0x00;//End code
		t1+=16;
		}
	//Now set free memory as 65536-t1
	robot_free_mem=(unsigned int)(65536L-(long)t1);
	//Let's now do global robotics
	prepare_robot_mem(1);
	//Setup robot 0, scroll 0, and global robot
	//Do this for a scroll
	scrolls[0].mesg_location=t1=0;//Current working location
	robot_mem[t1]=0x01;//Start code
	robot_mem[t1+1]='\n';//End of line code
	robot_mem[t1+2]=0x00;//End code
	t1+=16;
	//Now do this for 2 robots
	robots[0].program_location=t1;
	robot_mem[t1]=0xFF;//Start code
	robot_mem[t1+1]=0x00;//End code
	t1+=16;
	robots[NUM_ROBOTS].program_location=t1;
	robot_mem[t1]=0xFF;//Start code
	robot_mem[t1+1]=0x00;//End code
	//Done!
	return 0;
}

//Deallocate memory
void exit_robot_mem(void) {
	if(robot_mem==NULL) return;
	if(robot_mem_type==W_EMS) {
		free_EMS(robot_ems);
		free_EMS(rob_global_ems);
		}
	else farfree(rm_mirror);
	robot_mem=rm_mirror=NULL;
	robot_ems=rob_global_ems=robot_free_mem=0;
	robot_mem_type=W_NOWHERE;
}

//If robot memory is in ems, loads the correct ems pages
void prepare_robot_mem(char global) {
	if(robot_ems) {
		if(global) {
			map_page_EMS(rob_global_ems,0,0);
			map_page_EMS(rob_global_ems,1,1);
			map_page_EMS(rob_global_ems,2,2);
			map_page_EMS(rob_global_ems,3,3);
			}
		else {
			map_page_EMS(robot_ems,0,0);
			map_page_EMS(robot_ems,1,1);
			map_page_EMS(robot_ems,2,2);
			map_page_EMS(robot_ems,3,3);
			}
		}
	else {
		if(global) robot_mem=(unsigned char far *)
			MK_FP(FP_SEG(rm_mirror)+0x1000,FP_OFF(rm_mirror));
		else robot_mem=rm_mirror;
		}
	curr_rmem_status=global;
}

//Round up an unsigned int to a multiple of 16. Min. of 16.
unsigned int _round16(unsigned int n) {
	if(!n) return 16;
	if(n&15) n+=16-(n&15);
	return n;
}

//Reallocate a robot or scroll to a different size of memory. Returns non-0
//if there isn't enough room. Type is 0 for a scroll and 1 for a robot.
char reallocate_robot_mem(char type,unsigned char id,unsigned int size) {
	unsigned int old_size,t1,t2,loc,dif,unr_size=size;//Save unrounded size
	int temp=curr_rmem_status;
	//Check size change
	if(!type) old_size=scrolls[id].mesg_size;
	else old_size=robots[id].program_length;
	size=_round16(size);
	old_size=_round16(old_size);
	if(old_size==size) {
		if(!type) scrolls[id].mesg_size=unr_size;
		else robots[id].program_length=unr_size;
		return 0;//Already proper size. (to the nearest 16th)
		}
	//If incrementing...
	if(size>old_size) {
		//...is there room?
		if((id==0)||(id==NUM_ROBOTS)) ;//Special- only limit is
																  //the 31k one.
		else if((size-old_size)>robot_free_mem) return 1;//No room.
		//Verify <31k
		if(size>31744U) return 1;//Too big.
		}
	//Decreasing always works.
	//Change to new size and get location...
	if(!type) {
		scrolls[id].mesg_size=unr_size;
		loc=scrolls[id].mesg_location;
		}
	else {
		robots[id].program_length=unr_size;
		loc=robots[id].program_location;
		}
	//...move memory to compensate...
	prepare_robot_mem((id==0)||(id==NUM_ROBOTS));
	t1=1;//Code bit (1=getting bigger/moving up)
	if(size<old_size) t1=0;//Moving down
	dif=size-old_size;
	mem_mov((char far *)&robot_mem[loc+size],(char far *)&robot_mem[loc+old_size],
		(unsigned int)(65536L-loc-(t1?size:old_size)),t1);
	//...adjust free memory.
	if((id>0)&&(id<NUM_ROBOTS)) {
		robot_free_mem-=dif;
		//Now walk offsets, adjusting all those after the changed one.
		//Do scrolls first- if a scroll was resized other than the last one.
		if((type==0)&&(id<(NUM_SCROLLS-1))) {
			//Start at id+1
			for(t2=id+1;t2<NUM_SCROLLS;t2++) {
				//Increase or decrease
				scrolls[t2].mesg_location+=dif;
				}
			}
		//Now, unless the last robot was resized, do robots
		if(id<(NUM_ROBOTS-1)) {
			//Start at 1 unless robot was edited (then start at id+1)
			if(type<1) t2=1;
			else t2=id+1;
			for(;t2<NUM_ROBOTS;t2++) {
				//Increase or decrease
				robots[t2].program_location+=dif;
				}
			}
		}
	else {
		if(!type) {//Scroll 0 was changed- Change robot 0
			robots[0].program_location+=dif;
			}
		if(!id) {//Scroll 0 or robot 0 was changed- Change global robot
			robots[NUM_ROBOTS].program_location+=dif;
			}
		}
	//Done!
	prepare_robot_mem(temp);
	return 0;
}

//Clears a robot and sets it to use the minimum amount of memory.
void clear_robot(unsigned char id) {
	unsigned int t1,t2;
	int temp=curr_rmem_status;
	//Reallocate
	reallocate_robot_mem(T_ROBOT,id,2);
	prepare_robot_mem((id==0)||(id==NUM_ROBOTS));
	//Clear struct
	t1=robots[id].program_location;
	for(t2=0;t2<sizeof(Robot);t2++) {
		((char far *)(&robots[id]))[t2]=0;
		}
	robots[id].program_length=2;
	robots[id].robot_cycle=1;
	robots[id].program_location=t1;
	robots[id].cur_prog_line=1;
	robots[id].robot_char=2;
	robots[id].bullet_type=1;
	//Clear program (robot mem prepared from reallocate)
	robot_mem[t1]=0xFF;
	robot_mem[t1+1]=0x00;
	prepare_robot_mem(temp);
}

//Clears a scroll's struct and program.
void clear_scroll(unsigned char id) {
	unsigned int t1;
	int temp=curr_rmem_status;
	//Reallocate
	reallocate_robot_mem(T_SCROLL,id,3);
	prepare_robot_mem(!id);
	//Clear struct
	t1=scrolls[id].mesg_location;
	scrolls[id].num_lines=1;
	scrolls[id].used=0;
	//Clear program (robot mem prepared from reallocate)
	robot_mem[t1]=0x01;
	robot_mem[t1+1]='\n';
	robot_mem[t1+2]=0x00;
	prepare_robot_mem(temp);
}

//Clears a sensor's struct
void clear_sensor(unsigned char id) {
	sensors[id].sensor_name[0]=sensors[id].robot_to_mesg[0]=
		sensors[id].sensor_char=sensors[id].used=0;
}

//Finds the first available robot and marks it if asked. Returns 0 if none
//available, otherwise returns the id.
unsigned char find_robot(char mark) {
	int t1;
	//Don't stray into global robot territory!
	for(t1=1;t1<NUM_ROBOTS;t1++)
		if(!robots[t1].used) break;
	if(t1<NUM_ROBOTS) {
		if(mark) robots[t1].used=1;
		return t1;
		}
	return 0;
}

//Finds the first available scroll and marks it if asked. Returns 0 if none
//available, otherwise returns the id.
unsigned char find_scroll(char mark) {
	int t1;
	for(t1=1;t1<NUM_SCROLLS;t1++)
		if(!scrolls[t1].used) break;
	if(t1<NUM_SCROLLS) {
		if(mark) scrolls[t1].used=1;
		return t1;
		}
	return 0;
}

//Finds the first available sensor and marks it if asked. Returns 0 if none
//available, otherwise returns the id.
unsigned char find_sensor(char mark) {
	int t1;
	for(t1=1;t1<NUM_SENSORS;t1++)
		if(!sensors[t1].used) break;
	if(t1<NUM_SENSORS) {
		if(mark) sensors[t1].used=1;
		return t1;
		}
	return 0;
}

//Copies one robot to another. Returns non-0 for not enough memory.
//DOESN'T copy robot[].used variable.
char copy_robot(unsigned char dest,unsigned char source) {
	unsigned int t1;
	unsigned int sl,dl,size,pos;
	int temp=curr_rmem_status;
	char used=robots[dest].used;
	//Allocate
	if(reallocate_robot_mem(T_ROBOT,dest,robots[source].program_length))
		return 1;
	//From 0 -> # or # -> 0 ??
	if((dest==0)||(source==0)||(dest==GLOBAL_ROBOT)||(source==GLOBAL_ROBOT)) {
		//Non EMS?
		if(robot_mem_type==W_MEMORY) {
			prepare_robot_mem(1);
			//From #0
			if((source==0)||(source==GLOBAL_ROBOT)) {
				mem_cpy(&rm_mirror[t1=robots[dest].program_location],
					&robot_mem[robots[source].program_location],
					robots[source].program_length);
				}
			//To #0
			else {
					mem_cpy(&robot_mem[t1=robots[dest].program_location],
					&rm_mirror[robots[source].program_location],
					robots[source].program_length);
				}
			}
		else {
			//Within EMS
			dl=t1=robots[dest].program_location;
			sl=robots[source].program_location;
			size=robots[source].program_length;
			//Special code
			//From #0
			//Sixteen-byte copies, since (sl | dl) % 16 = 0 and size % 16 = 0
			pos=0;
			if((source==0)||(source==GLOBAL_ROBOT)) {
				do {
					//Copy 16 bytes, from sl+pos to dl+pos
					map_page_EMS(robot_ems,0,(dl+pos)>>14);
					map_page_EMS(rob_global_ems,1,(sl+pos)>>14);
					mem_cpy(&robot_mem[(dl+pos)&16383],
						&robot_mem[((sl+pos)&16383)+16384],16);
					pos+=16;
				} while(pos<size);
				}
			//To #0
			else {
				do {
					//Copy 16 bytes, from dl+pos to sl+pos
					map_page_EMS(rob_global_ems,0,(dl+pos)>>14);
					map_page_EMS(robot_ems,1,(sl+pos)>>14);
					mem_cpy(&robot_mem[(dl+pos)&16383],
						&robot_mem[((sl+pos)&16383)+16384],16);
					pos+=16;
				} while(pos<size);
				}
			}
		}
	else {
		//Copy
		prepare_robot_mem(0);
		mem_cpy(&robot_mem[t1=robots[dest].program_location],
			&robot_mem[robots[source].program_location],
			robots[dest].program_length);
		}
	//Copy struct, other than location
	mem_cpy((char far *)&robots[dest],(char far *)&robots[source],
		sizeof(Robot));
	robots[dest].program_location=t1;
	robots[dest].used=used;
	prepare_robot_mem(temp);
	//Done!
	return 0;
}

//Copies one scroll to another. Returns non-0 for not enough memory.
//DOESN'T copy scroll[].used variable.
char copy_scroll(unsigned char dest,unsigned char source) {
	unsigned int t1;
	unsigned int sl,dl,size,pos;
	int temp=curr_rmem_status;
	char used=scrolls[dest].used;
	//Allocate
	if(reallocate_robot_mem(T_SCROLL,dest,scrolls[source].mesg_size))
		return 1;
	//From 0 -> # or # -> 0 ??
	if((dest==0)||(source==0)) {
		//Non EMS?
		if(robot_mem_type==W_MEMORY) {
			prepare_robot_mem(1);
			//From #0
			if(source==0) {
				mem_cpy(&rm_mirror[t1=scrolls[dest].mesg_location],
					&robot_mem[scrolls[source].mesg_location],
					scrolls[source].mesg_size);
				}
			//To #0
			else {
					mem_cpy(&robot_mem[t1=scrolls[dest].mesg_location],
					&rm_mirror[scrolls[source].mesg_location],
					scrolls[source].mesg_size);
				}
			}
		else {
			//Within EMS
			dl=t1=scrolls[dest].mesg_location;
			sl=scrolls[source].mesg_location;
			size=scrolls[source].mesg_size;
			//Special code
			//From #0
			//Sixteen-byte copies, since (sl | dl) % 16 = 0 and size % 16 = 0
			pos=0;
			if(source==0) {
				do {
					//Copy 16 bytes, from sl+pos to dl+pos
					map_page_EMS(robot_ems,0,(dl+pos)>>14);
					map_page_EMS(rob_global_ems,1,(sl+pos)>>14);
					mem_cpy(&robot_mem[(dl+pos)&16383],
						&robot_mem[((sl+pos)&16383)+16384],16);
					pos+=16;
				} while(pos<size);
				}
			//To #0
			else {
				do {
					//Copy 16 bytes, from dl+pos to sl+pos
					map_page_EMS(rob_global_ems,0,(dl+pos)>>14);
					map_page_EMS(robot_ems,1,(sl+pos)>>14);
					mem_cpy(&robot_mem[(dl+pos)&16383],
						&robot_mem[((sl+pos)&16383)+16384],16);
					pos+=16;
				} while(pos<size);
				}
			}
		}
	else {
		//Copy
		prepare_robot_mem();
		mem_cpy(&robot_mem[t1=scrolls[dest].mesg_location],
			&robot_mem[scrolls[source].mesg_location],
			scrolls[source].mesg_size);
		}
	//Copy struct, other than location
	mem_cpy((char far *)&scrolls[dest],(char far *)&scrolls[source],
		sizeof(Scroll));
	scrolls[dest].mesg_location=t1;
	scrolls[dest].used=used;
	prepare_robot_mem(temp);
	//Done!
	return 0;
}

//Copies one sensor to another, NOT including sensor[].used variable.
void copy_sensor(unsigned char dest,unsigned char source) {
	char used=sensors[dest].used;
	//Copy struct
	mem_cpy((char far *)&sensors[dest],(char far *)&sensors[source],
		sizeof(Sensor));
	sensors[dest].used=used;
}