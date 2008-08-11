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

/* Fill function. */

#include "beep.h"
#include "idarray.h"
#include "fill.h"
#include "roballoc.h"
#include "error.h"
#include "data.h"

/* Pseudo-code algorithm-

	(MATCHES means that the id/color at that id is what we are
	 filling over. PLACING includes deleting the old, deleting programs,
	 and copying current program, if any. The STACK is an array for
	 keeping track of areas we need to continue filling at, and the
	 direction to fill in at that point. A PUSH onto the STACK moves the
	 stack pointer up one and inserts info, unless the STACK is full, in
	 which case the operation is cancelled. A STACK size of about 500
	 elements is usally enough for a fairly complex 100 by 100 section.)

	(Note that the usual Robot#0 code is NOT performed- If current is a
	 robot, it is already #0 and must stay that way for proper filling.)

	1) Initialize fill stack and record what we are filling over.
	2) Push current position going left.
	3) Loop:

		1) Take top element off of stack.
		1.5) Verify it still matches (in case another branch got here)
		2) Start at noted position. Set ABOVE_MATCH and BELOW_MATCH to 0.
		3a) If direction is left, push onto stack-

			1) Id above, going left, if matches. Set ABOVE_MATCH to 1.
			2) Id below, going left, if matches. Set BELOW_MATCH to 1.
			3) Id to the right, going right, if matches.

		3b) If direction is right, push nothing.
		4) Place at position.
		6) Note what is above- If it DOESN'T match, set ABOVE_MATCH to 0.
		7) Note what is below- If it DOESN'T match, set BELOW_MATCH to 0.
		8) If above DOES match and ABOVE_MATCH is 0-

			1) Push onto stack Id above, going left.
			2) Set ABOVE_MATCH to 1.

		9) If below DOES match and BELOW_MATCH is 0-

			1) Push onto stack Id below, going left.
			2) Set BELOW_MATCH to 1.

		10) Go one step in direction. (left/right)
		11) If position matches, jump to #4.
		12) If any elements remain on stack, loop.

	4) Done.
*/

//Structure for one element of the stack
struct StackElem {
	unsigned int x:9;
	unsigned int y:8;
	signed int dir:2;//-1=Left, 1=Right.
};
typedef struct StackElem StackElem;

//The stack
#define STACK_SIZE 500
int stack_pos;//Current element. -1=empty.
StackElem stack[STACK_SIZE+1];

//with_param is assumed to be the current param, so is changed to 0 when
//appropriate for robots/etc. (according to with_id)
void fill_area(int x,int y,int with_id,int with_color,int &with_param) {
	int fover_id,fover_color,t1;//What we are filling over
	char dir,above_match,below_match;
	if(with_id==127) return;//Cannot fill w/player
	if(level_id[x+y*max_bxsiz]==127) return;//Cannot fill over player
	if((level_id[x+y*max_bxsiz]==with_id)&&(level_color[x+y*max_bxsiz]==with_color)) {
		if(with_id==100) return;//Strange... ?
		//We are trying to fill with a similar color and id. First we
		//must fill instead with ids of 100, same color/param.
		fill_area(x,y,100,with_color,with_param);
		//Now we just fill normally.
		}
	//1) Initialize fill stack and record what we are filling over.
	fover_id=level_id[x+y*max_bxsiz];
	fover_color=level_color[x+y*max_bxsiz];
	stack_pos=0;
	//2) Push current position going left.
	stack[0].x=x;
	stack[0].y=y;
	stack[0].dir=-1;
	//3) Loop:
	do {
	//	1) Take top element off of stack.
		x=stack[stack_pos].x;
		y=stack[stack_pos].y;
		dir=stack[stack_pos--].dir;
	// 1.5) Verify it matches
		if((level_id[x+y*max_bxsiz]!=fover_id)||
			(level_color[x+y*max_bxsiz]!=fover_color))
				continue;
	//	2) Start at noted position. Set ABOVE_MATCH and BELOW_MATCH to 0.
		above_match=below_match=0;
	//	3a) If direction is left, push onto stack-
		if(dir==-1) {
	//		1) Id above, going left, if matches. Set ABOVE_MATCH to 1.
			if(y>0) {
				if((level_id[x+(y-1)*max_bxsiz]==fover_id)&&
					(level_color[x+(y-1)*max_bxsiz]==fover_color)) {
						if(stack_pos<STACK_SIZE) {
							stack[++stack_pos].x=x;
							stack[stack_pos].y=y-1;
							stack[stack_pos].dir=-1;
							above_match=1;
							}
						}
				}
	//		2) Id below, going left, if matches. Set BELOW_MATCH to 1.
			if(y<(board_ysiz-1)) {
				if((level_id[x+(y+1)*max_bxsiz]==fover_id)&&
					(level_color[x+(y+1)*max_bxsiz]==fover_color)) {
						if(stack_pos<STACK_SIZE) {
							stack[++stack_pos].x=x;
							stack[stack_pos].y=y+1;
							stack[stack_pos].dir=-1;
							below_match=1;
							}
						}
				}
	//		3) Id to the right, going right, if matches.
			if(x<(board_xsiz-1)) {
				if((level_id[x+1+y*max_bxsiz]==fover_id)&&
					(level_color[x+1+y*max_bxsiz]==fover_color)) {
						if(stack_pos<STACK_SIZE) {
							stack[++stack_pos].x=x+1;
							stack[stack_pos].y=y;
							stack[stack_pos].dir=1;
							}
						}
				}
			}
	//	3b) If direction is right, push nothing.
	//	4) Place at position.
	num_four:
		// [delete at position, copying current to 0 if neccesary]
		if(fover_id==122) {//(sensor)
			t1=level_param[x+y*max_bxsiz];
//			if((with_id==122)&&(with_param==t1)) {
//				copy_sensor(0,t1);
//				with_param=0;
//				}
			clear_sensor(t1);
			}
		else if((fover_id==123)||(fover_id==124)) {//(robot)
			t1=level_param[x+y*max_bxsiz];
//			if(((with_id==123)||(with_id==124))&&(with_param==t1)) {
//				copy_robot(0,t1);
//				with_param=0;
//				}
			clear_robot(t1);
			}
		else if((fover_id==125)||(fover_id==126)) {//(scroll)
			t1=level_param[x+y*max_bxsiz];
//			if(((with_id==125)||(with_id==126))&&(with_param==t1)) {
//				copy_scroll(0,t1);
//				with_param=0;
//				}
			clear_scroll(t1);
			}
		id_remove_top(x,y);
		// [place at position, copying current if neccesary]
		if(with_id==122) {//(sensor)
			if(!(t1=find_sensor())) {
				error("No available sensors",1,24,current_pg_seg,0x0801);
				return;//Fill aborted
				}
			copy_sensor(t1,with_param);
//			if(with_param==0) clear_sensor(0);
//			with_param=t1;
			id_place(x,y,with_id,with_color,t1);
			}
		else if((with_id==123)||(with_id==124)) {//(robot)
			if(!(t1=find_robot())) {
				error("No available robots",1,24,current_pg_seg,0x0901);
				return;//Fill aborted
				}
			if(copy_robot(t1,with_param)) {
				robots[t1].used=0;
				error("Out of robot memory",1,21,current_pg_seg,0x0502);
				return;//Fill aborted
				}
//			if(with_param==0) clear_robot(0);
//			with_param=t1;
			id_place(x,y,with_id,with_color,t1);
			}
		else if((with_id==125)||(with_id==126)) {//(scroll)
			if(!(t1=find_scroll())) {
				error("No available scrolls",1,24,current_pg_seg,0x0A01);
				return;//Fill aborted
				}
			if(copy_scroll(t1,with_param)) {
				robots[t1].used=0;
				error("Out of robot memory",1,21,current_pg_seg,0x0503);
				return;//Fill aborted
				}
//			if(with_param==0) clear_scroll(0);
//			with_param=t1;
			id_place(x,y,with_id,with_color,t1);
			}
		else id_place(x,y,with_id,with_color,with_param);
	//	6) Note what is above- If it DOESN'T match, set ABOVE_MATCH to 0.
		if(y>0) {
			if((level_id[x+(y-1)*max_bxsiz]!=fover_id)||
				(level_color[x+(y-1)*max_bxsiz]!=fover_color))
					above_match=0;
			}
	//	7) Note what is below- If it DOESN'T match, set BELOW_MATCH to 0.
		if(y<(board_ysiz-1)) {
			if((level_id[x+(y+1)*max_bxsiz]!=fover_id)||
				(level_color[x+(y+1)*max_bxsiz]!=fover_color))
					below_match=0;
			}
	//	8) If above DOES match and ABOVE_MATCH is 0-
		if(y>0) {
			if((level_id[x+(y-1)*max_bxsiz]==fover_id)&&
				(level_color[x+(y-1)*max_bxsiz]==fover_color)&&
				(above_match==0)) {
	//		1) Push onto stack Id above, going left.
					if(stack_pos<STACK_SIZE) {
						stack[++stack_pos].x=x;
						stack[stack_pos].y=y-1;
						stack[stack_pos].dir=-1;
						}
	//		2) Set ABOVE_MATCH to 1.
					above_match=1;
					}
			}
	//	9) If below DOES match and BELOW_MATCH is 0-
		if(y<(board_ysiz-1)) {
			if((level_id[x+(y+1)*max_bxsiz]==fover_id)&&
				(level_color[x+(y+1)*max_bxsiz]==fover_color)&&
				(below_match==0)) {
	//		1) Push onto stack Id below, going left.
					if(stack_pos<STACK_SIZE) {
						stack[++stack_pos].x=x;
						stack[stack_pos].y=y+1;
						stack[stack_pos].dir=-1;
						}
	//		2) Set BELOW_MATCH to 1.
					below_match=1;
				}
			}
	//	10) Go one step in direction. (left/right)
		x+=dir;
	//	11) If position matches, jump to #4.
		if((x>=0)&&(x<board_xsiz))
			if((level_id[x+y*max_bxsiz]==fover_id)&&
				(level_color[x+y*max_bxsiz]==fover_color))
					goto num_four;
	//	12) If any elements remain on stack, loop.
	} while(stack_pos>=0);
	//4) Done.
}

//with_id is, of course, really considered the parameter in the editor.
void fill_overlay(int x,int y,int with_id,int with_color) {
	int fover_id,fover_color;//What we are filling over
	char dir,above_match,below_match;
	//1) Initialize fill stack and record what we are filling over.
	fover_id=overlay[x+y*max_bxsiz];
	fover_color=overlay_color[x+y*max_bxsiz];
	if((fover_id==with_id)&&(fover_color==with_color)) return;
	stack_pos=0;
	//2) Push current position going left.
	stack[0].x=x;
	stack[0].y=y;
	stack[0].dir=-1;
	//3) Loop:
	do {
	//	1) Take top element off of stack.
		x=stack[stack_pos].x;
		y=stack[stack_pos].y;
		dir=stack[stack_pos--].dir;
	//	2) Start at noted position. Set ABOVE_MATCH and BELOW_MATCH to 0.
		above_match=below_match=0;
	//	3a) If direction is left, push onto stack-
		if(dir==-1) {
	//		1) Id above, going left, if matches. Set ABOVE_MATCH to 1.
			if(y>0) {
				if((overlay[x+(y-1)*max_bxsiz]==fover_id)&&
					(overlay_color[x+(y-1)*max_bxsiz]==fover_color)) {
						if(stack_pos<STACK_SIZE) {
							stack[++stack_pos].x=x;
							stack[stack_pos].y=y-1;
							stack[stack_pos].dir=-1;
							above_match=1;
							}
						}
				}
	//		2) Id below, going left, if matches. Set BELOW_MATCH to 1.
			if(y<(board_ysiz-1)) {
				if((overlay[x+(y+1)*max_bxsiz]==fover_id)&&
					(overlay_color[x+(y+1)*max_bxsiz]==fover_color)) {
						if(stack_pos<STACK_SIZE) {
							stack[++stack_pos].x=x;
							stack[stack_pos].y=y+1;
							stack[stack_pos].dir=-1;
							below_match=1;
							}
						}
				}
	//		3) Id to the right, going right, if matches.
			if(x<(board_xsiz-1)) {
				if((overlay[x+1+y*max_bxsiz]==fover_id)&&
					(overlay_color[x+1+y*max_bxsiz]==fover_color)) {
						if(stack_pos<STACK_SIZE) {
							stack[++stack_pos].x=x+1;
							stack[stack_pos].y=y;
							stack[stack_pos].dir=1;
							}
						}
				}
			}
	//	3b) If direction is right, push nothing.
	//	4) Place at position.
	num_four:
		overlay[x+y*max_bxsiz]=with_id;
		overlay_color[x+y*max_bxsiz]=with_color;
	//	6) Note what is above- If it DOESN'T match, set ABOVE_MATCH to 0.
		if(y>0) {
			if((overlay[x+(y-1)*max_bxsiz]!=fover_id)||
				(overlay_color[x+(y-1)*max_bxsiz]!=fover_color))
					above_match=0;
			}
	//	7) Note what is below- If it DOESN'T match, set BELOW_MATCH to 0.
		if(y<(board_ysiz-1)) {
			if((overlay[x+(y+1)*max_bxsiz]!=fover_id)||
				(overlay_color[x+(y+1)*max_bxsiz]!=fover_color))
					below_match=0;
			}
	//	8) If above DOES match and ABOVE_MATCH is 0-
		if(y>0) {
			if((overlay[x+(y-1)*max_bxsiz]==fover_id)&&
				(overlay_color[x+(y-1)*max_bxsiz]==fover_color)&&
				(above_match==0)) {
	//		1) Push onto stack Id above, going left.
					if(stack_pos<STACK_SIZE) {
						stack[++stack_pos].x=x;
						stack[stack_pos].y=y-1;
						stack[stack_pos].dir=-1;
						}
	//		2) Set ABOVE_MATCH to 1.
					above_match=1;
					}
			}
	//	9) If below DOES match and BELOW_MATCH is 0-
		if(y<(board_ysiz-1)) {
			if((overlay[x+(y+1)*max_bxsiz]==fover_id)&&
				(overlay_color[x+(y+1)*max_bxsiz]==fover_color)&&
				(below_match==0)) {
	//		1) Push onto stack Id below, going left.
					if(stack_pos<STACK_SIZE) {
						stack[++stack_pos].x=x;
						stack[stack_pos].y=y+1;
						stack[stack_pos].dir=-1;
						}
	//		2) Set BELOW_MATCH to 1.
					below_match=1;
				}
			}
	//	10) Go one step in direction. (left/right)
		x+=dir;
	//	11) If position matches, jump to #4.
		if((x>=0)&&(x<board_xsiz))
			if((overlay[x+y*max_bxsiz]==fover_id)&&
				(overlay_color[x+y*max_bxsiz]==fover_color))
					goto num_four;
	//	12) If any elements remain on stack, loop.
	} while(stack_pos>=0);
	//4) Done.
}