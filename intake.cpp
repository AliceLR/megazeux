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

// Code to the intake function, which inputs text with friendliness.

#include "mouse.h"
#include "beep.h"
#include "intake.h"
#include "graphics.h"
#include "getkey.h"
#include "cursor.h"
#include "string.h"
#include "window.h"
#include "hexchar.h"

// Global status of insert
char insert_on=1;

// (returns the key used to exit) String points to your memory for storing
// the new string. The current "value" is used- clear the string before
// calling intake if you need a blank string. Max_len is the maximum length
// of the string. X, y, segment, and color are self-explanitory. Exit_type
// determines when intake exits- 0 means exit only on Enter, 1 on Enter or
// ESC, 2 on any non-display char except the editing keys. Filter_type is
// bits to turn on filters- 1 changes all alpha to upper, 2 changes all
// alpha to lower, 4 blocks all numbers, 8 blocks all alpha, 16 blocks
// spaces, 32 blocks graphics, (ascii>126) 64 blocks punctuation that isn't
// allowed in a filename/path/drive combo, 128 blocks punctuation that isn't
// allowed in a filename only combo (use w/64), 256 blocks all punctuation.
// (IE non-alphanumerics and spaces) The editing keys supported are as
// follows- Keys to enter characters, Enter/ESC to exit, Home/End to move to
// the front/back of the line, Insert to toggle insert/overwrite, Left/Right
// to move within the line, Bkspace/Delete to delete as usual, and Alt-Bksp
// to clear the entire line. No screen saving is performed. After this
// function, the cursor is automatically off. If password is set, all
// characters appear as x.

//Password character
#define PW_CHAR 42

//Mouse support- Clicking inside string sends cursor there. Clicking
//outside string returns a MOUSE_EVENT to caller without acknowledging
//the event.

//Returns a backspace if attempted at start of line. (and exit_type==2)
//Returns a delete if attempted at end of line. (and exit_type==2)

//If robo_intk is set, only 76 chars are shown and symbols are altered
//on the far left/right if there is more to the left/right. Scrolling
//of the line is supported. The current character (1 on) is shown at
//x=32,y=0, in color 79, min. 3 chars.

//Also, if robo_intk is set, then ANY line consisting of ONLY semicolons,
//commas, and spaces is returned as a blank line (0 length)
int intake(char far *string,int max_len,char x,char y,unsigned int segment,
 unsigned char color,char exit_type,int filter_type,char password,
 int *return_x_pos,char robo_intk,char far *macro) {
	int currx,key,curr_len,t1,ret;
	int macron=-1;
	static unsigned char ch_sel=219;
	if(macro!=NULL) macron=0;
	//Turn off auto acknowledge of mouse events
	auto_pop_mouse_events=0;
	//Activate cursor
	if(insert_on) cursor_underline();
	else cursor_solid();
	//Put cursor at the end of the string...
	currx=curr_len=str_len(string);
	//...unless return_x_pos says not to.
	if(return_x_pos)
		if(*return_x_pos<currx) currx=*return_x_pos;
	if(robo_intk&&(currx>75)) move_cursor(77,y);
	else move_cursor(x+currx,y);
	if(insert_on) cursor_underline();
	else cursor_solid();
	do {
		//Draw current string
		m_hide();
		if(password) fill_line(curr_len,x,y,(color<<8)+PW_CHAR,segment);
		else if(!robo_intk) write_string(string,x,y,color,segment,0);
		else {
			draw_char('',color,79,y,segment);
			if((curr_len<76)||(currx<76)) {
				draw_char('',color,0,y,segment);
				write_line(string,x,y,color,segment,0);
				if(curr_len<76)
					fill_line(76-curr_len,x+curr_len,y,(color<<8)+32,segment);
				else draw_char('¯',color,79,y,segment);
				}
			else {
				draw_char(' ',color,77,y,segment);
				write_line(&string[currx-75],x,y,color,segment,0);
				draw_char('®',color,0,y,segment);
				if(currx<curr_len) draw_char('¯',color,79,y,segment);
				}
			draw_char(' ',color,78,y,segment);
			}
		if(!robo_intk) fill_line(max_len+1-curr_len,x+curr_len,y,(color<<8)+32,segment);
		if(robo_intk) {
			write_number(currx+1,79,32,0,segment,3);
			write_number(curr_len,79,36,0,segment,3);
			}
		m_show();
		//Get key
		if(macron>=0) {
			key=macro[macron++];
			if(macro[macron]==0) macron=-1;
			if(key=='^') key=13;
			else goto text_place;
			}
		else key=getkey();
		//Process
		switch(key) {
			case MOUSE_EVENT:
				//In our string?
				t1=mouse_event.cx;
				if((mouse_event.cy==y)&&(t1>=x)&&(t1<=x+max_len)) {
					//Yep, reposition cursor and acknowledge event.
					currx=t1-x;
					if(currx>curr_len) currx=curr_len;
					acknowledge_mouse();
					break;
					}
				//Nope- exit or beep, depending upon exit_type.
				//(first, acknowledge event if exit_type is too low)
				if(exit_type<2) acknowledge_mouse();
			default:
				//Exit key or keycode.
				if(key<32) {
				keyer:
					//Exit?
					if(exit_type==2) {
						auto_pop_mouse_events=1;
						cursor_off();
						if(return_x_pos) *return_x_pos=currx;
						ret=key;
						goto rett;
						}
					else beep();
					break;
					}
				//Keycode.. Filter.
				if(filter_type&1) {
					if((key>='a')&&(key<='z')) key-=32;
					}
				if(filter_type&2) {
					if((key>='A')&&(key<='Z')) key+=32;
					}
				if(filter_type&4) {
					if((key>='0')&&(key<='9')) {
						beep();
						break;
						}
					}
				if(filter_type&8) {
					if((key>='a')&&(key<='z')) {
						beep();
						break;
						}
					if((key>='A')&&(key<='Z')) {
						beep();
						break;
						}
					}
				if(filter_type&16) {
					if(key==' ') {
						beep();
						break;
						}
					}
				if(filter_type&32) {
					if(key>126) {
						beep();
						break;
						}
					}
				if(filter_type&64) {
					if((key=='*')||(key=='[')||(key==']')||(key=='>')||
						(key=='<')||(key==',')||(key=='|')||(key=='?')||
						(key=='=')||(key==';')||(key=='\"')||(key=='/')) {
							beep();
							break;
							}
					}
				if(filter_type&128) {
					if((key==':')||(key=='\\')) {
						beep();
						break;
						}
					}
				if(filter_type&256) {
					if((key>' ')&&(key<'0')) {
						beep();
						break;
						}
					if((key>'9')&&(key<'A')) {
						beep();
						break;
						}
					if((key>'Z')&&(key<'a')) {
						beep();
						break;
						}
					if((key>'z')&&(key<127)) {
						beep();
						break;
						}
					}
				//Add key to string
			text_place:
				if(currx==max_len) {
					beep();
					break;//At end of str
					}
				if((insert_on)&&(curr_len==max_len)) {
					beep();
					break;//No room to insert
					}
				//Overwrite or insert?
				if((insert_on)||(currx==curr_len)) {
					//Insert- Move all ahead 1, increasing string length
					mem_mov(&string[currx+1],&string[currx],(++curr_len)-currx,1);
					}
				//Add character and move forward one
				string[currx++]=key;
				break;
			case -46:
				//Alt+C choose character
				if(filter_type) {
					beep();
					break;
					}
				ch_sel=key=char_selection(ch_sel,segment);
				if(key<32) break;
				goto text_place;
			case 27:
				//ESC
				if(exit_type>0) {
					cursor_off();
					auto_pop_mouse_events=1;
					if(return_x_pos) *return_x_pos=currx;
					ret=27;
					goto rett;
					}
				beep();
				break;
			case 13:
				//Enter
				cursor_off();
				auto_pop_mouse_events=1;
				if(return_x_pos) *return_x_pos=currx;
				ret=13;
				goto rett;
			case -71:
				//Home
				if(currx==0) goto keyer;
				currx=0;
				break;
			case -79:
				//End
				if(currx==curr_len) goto keyer;
				currx=curr_len;
				break;
			case -14:
				//Alt-Bkspace
				curr_len=currx=0;
				string[0]=0;
				break;
			case -75:
				//Left
				if(currx>0) currx--;
				break;
			case -77:
				//Right
				if(currx<curr_len) currx++;
				break;
			case -82:
				//Insert
				if(insert_on) cursor_solid();
				else cursor_underline();
				insert_on^=1;
				break;
			case 8:
				//Bkspace
				if(currx==0) {
					if(exit_type==2) {
						auto_pop_mouse_events=1;
						cursor_off();
						if(return_x_pos) *return_x_pos=currx;
						ret=8;
						goto rett;
						}
					beep();
					break;
					}
				//Move all back 1, decreasing string length
				mem_mov(&string[currx-1],&string[currx],(curr_len--)-currx+1,0);
				//Cursor back one
				currx--;
			case 0:
				break;
			case -83:
				//Delete
				if(currx==curr_len) {
					if(exit_type==2) {
						auto_pop_mouse_events=1;
						cursor_off();
						if(return_x_pos) *return_x_pos=currx;
						ret=-83;
						goto rett;
						}
					beep();
					break;
					}
				//Move all back 1, decreasing string length
				mem_mov(&string[currx],&string[currx+1],(curr_len--)-currx,0);
				break;
			}
		//Move cursor
		if(robo_intk&&(currx>75)) move_cursor(77,y);
		else move_cursor(x+currx,y);
		if(insert_on) cursor_underline();
		else cursor_solid();
		//Loop
	} while(1);
rett:
	//Return ret. If robo_intk, verify that the string is valid
	if(robo_intk) {
		curr_len=str_len(string);
		if(curr_len) {
			for(t1=0;t1<curr_len;t1++)
				if((string[t1]!=';')&&(string[t1]!=',')&&(string[t1]!=' '))
					break;
			if(t1>=curr_len) {
				string[0]=0;//Become an empty string
				if(return_x_pos) *return_x_pos=0;
				}
			}
		}
	return ret;
}