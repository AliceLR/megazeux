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

/* Scroll display code */

#include "palette.h"
#include "arrowkey.h"
#include "string.h"
#include <stdio.h>
#include "data.h"
#include "scrdisp.h"
#include "window.h"
#include "graphics.h"
#include "error.h"
#include "getkey.h"
#include "intake.h"
#include "mouse.h"
#include "roballoc.h"
#include "beep.h"

int scroll_base_color=143;
int scroll_corner_color=135;
int scroll_pointer_color=128;
int scroll_title_color=143;
int scroll_arrow_color=142;

char lower_fix[32]={//Changes 0-31 ascii to printable codes
	' ','o','o','*','*','*','*','ù','Û','o','Û','o','+','&','&',
	'*','>','<','|','!','&','$','_','|','^','v','>','<','-','-',
	'^','v' };

char print(unsigned char far *str) {
	int pos=0,t1;
	//Prints. Shuffles out <32 chars and color codes.
	do {
		t1=str[pos++];
		if((t1=='~')||(t1=='@')) {
			t1=str[pos++];
			if((t1>='0')&&(t1<='9')) continue;
			if((t1>='a')&&(t1<='f')) continue;
			if((t1>='A')&&(t1<='F')) continue;
			if(fputc(t1,stdprn)!=t1) return 1;
			}
		else if(t1==5) fwrite("     ",1,5,stdprn);
		else if(t1==0) {
			fputc(10,stdprn);
			fputc(13,stdprn);
			fflush(stdprn);
			}
		else if(t1<32) fputc(lower_fix[t1],stdprn);
		else if(t1==127) fputc('þ',stdprn);
		else if(t1==255) fputc(' ',stdprn);
		else if(fputc(t1,stdprn)!=t1) return 1;
	} while(t1);
	return 0;
}

//Also used to display scrolls. Use a type of 0 for Show Scroll, 1 for Show
//Sign, 2 for Edit Scroll.
void scroll_edit(int id,char type) {
	//Important status vars (insert kept in intake.cpp)
	unsigned int pos=1,old_pos;//Where IN scroll?
	int currx=0;//X position in line
	int key;//Key returned by intake()
	int t1,t2,t3;
	char far *where;//Where scroll is
	char line[80];//For editing
	//Draw screen
	save_screen(current_pg_seg);
	scroll_edging(type);
	//Loop
	prepare_robot_mem(!id);
	where=&robot_mem[scrolls[id].mesg_location];
	do {
		//Display scroll
		scroll_frame(id,pos);
		if(type==2) {
			//Figure length
			for(t1=0;t1<80;t1++)
				if(where[pos+t1]=='\n') break;
			//t1==length
			//Edit line
			where[pos+t1]=0;
			str_cpy(line,&where[pos]);
			where[pos+t1]='\n';
			key=intake(line,64,8,12,current_pg_seg,scroll_base_color,2,0,0,&currx);
			//Modify scroll to hold new line (give errors here)
			t2=str_len(line);//Get length of NEW line
			//Resize and move- Resize FIRST if moving >, LAST if <.
			if(t2>t1) {
				if(reallocate_robot_mem(T_SCROLL,id,(t3=scrolls[id].mesg_size)+t2-t1)) {
					//Too big.
					if(error("Code/message is too large to fit into robot memory",1,25,
						current_pg_seg,0x2201)==1) return;
					//Shorten to orig. length
					t2=t1;
					line[t2]=0;
					if(key!=27) key=0;//Non-ESC is cancelled.
					}
				//Now move.
				mem_mov(&where[pos+t2],&where[pos+t1],t3-pos-t1,1);
				}
			else if(t2<t1) {
				mem_mov(&where[pos+t2],&where[pos+t1],(t3=scrolls[id].mesg_size)-pos-t1,0);
				reallocate_robot_mem(T_SCROLL,id,t3+t2-t1);
				}
			//Copy in new line
			str_cpy(&where[pos],line);
			where[pos+t2]='\n';
			//Act upon key...
			if(key==MOUSE_EVENT) acknowledge_mouse();
			}
		else key=getkey();
		old_pos=pos;
		switch(key) {
			case MOUSE_EVENT:
				//Move to line clicked on if mouse is in scroll, else exit
				if((mouse_event.cy>=6)&&(mouse_event.cy<=18)&&
					(mouse_event.cx>=8)&&(mouse_event.cx<=71)) {
						t1=mouse_event.cy-12;
						if(t1==0) break;
						//t1<0 = PGUP t1 lines
						//t1>0 = PGDN t1 lines
						if(t1<0) goto pgup;
						goto pgdn;
						}
				key=27;
				break;
			case -72://Up
			up_a_line:
				//Go back a line (if possible)
				if(where[pos-1]==1) break;//Can't.
				pos--;
				//Go to start of this line.
				do {
					pos--;
				} while((where[pos]!='\n')&&(where[pos]!=1));
				pos++;
				//Done.
				break;
			case -80://Down
			down_a_line:
				//Go forward a line (if possible)
				while(where[pos]!='\n') pos++;
				//At end of current. Is there a next line?
				if(where[++pos]==0) {
					//Nope.
					pos=old_pos;
					break;
					}
				//Yep. Done.
				break;
			case 13://Enter
				if(type<2) {
					key=27;
					break;
					}
				//Add in new line below. Need only add one byte.
				if(reallocate_robot_mem(T_SCROLL,id,(t3=scrolls[id].mesg_size)+1)) {
					//Too big.
					if(error("Code/message is too large to fit into robot memory",1,25,
						current_pg_seg,0x2202)==1) return;
					//Forget it.
					break;
					}
				//Move all at pos+currx up a space
				mem_mov(&where[pos+currx+1],&where[pos+currx],t3-pos-currx,1);
				//Insert a \n
				where[pos+currx]='\n';
				//Change pos and currx
				pos=pos+currx+1;
				currx=0;
				scrolls[id].num_lines++;
				//Done.
				break;
			case 8://Backspace
				if(type<2) goto nope;
				//We are at the start of the current line and we are trying to
				//append it to the end of the previous line. First, remember
				//the size of the current line is in t2. Now we go back a line,
				//if there is one.
				if(where[pos-1]==1) break;//Nope.
				pos--;
				//Go to start of this line, COUNTING CHARACTERS.
				t1=0;
				do {
					pos--; t1++;
				} while((where[pos]!='\n')&&(where[pos]!=1));
				//pos is at one before the start of this line. WHO CARES!? :)
				//Now we have to see if the size, t2, is over 64 characters.
				//t2+t1 is currently one too many so check for >65 for error.
				if((t2+t1)>65) {
					pos=old_pos;
					break;//Too long. Reset position.
					}
				//OKAY! Just copy backwards over the \n in the middle to
				//append...
				mem_mov(&where[old_pos-1],&where[old_pos],
					(t3=scrolls[id].mesg_size)-old_pos,0);
				//...and reallocate to one space less!
				reallocate_robot_mem(T_SCROLL,id,t3-1);
				//pos is one before this start. Fix to the start of this new
				//line. Set currx to the length of the old line this was.
				pos++; currx=t1-1;
				scrolls[id].num_lines--;
				//Done.
				break;
			case -81://Pagedown (by 6 lines)
				for(t1=6;t1>0;t1--) {
				pgdn:
					//Go forward a line (if possible)
					old_pos=pos;
					while(where[pos]!='\n') pos++;
					//At end of current. Is there a next line?
					if(where[++pos]==0) {
						//Nope.
						pos=old_pos;
						break;
						}
					//Yep. Done.
					}
				break;
			case -73://Pageup (by 6 lines)
				for(t1=-6;t1<0;t1++) {
				pgup:
					//Go back a line (if possible)
					if(where[pos-1]==1) break;//Can't.
					pos--;
					//Go to start of this line.
					do {
						pos--;
					} while((where[pos]!='\n')&&(where[pos]!=1));
					pos++;
					//Done.
					}
				break;
			case -71://Home
				t1=-30000;
				goto pgup;
			case -79://End
				t1=30000;
				goto pgdn;
			default:
			nope:
				beep();
			case 27:
			case 0:
				break;
			}
		//Continue?
	} while(key!=27);
	//Restore screen and exit
	restore_screen(current_pg_seg);
}

void scroll_frame(int id,unsigned int pos) {
	//Displays one frame of a scroll. The scroll edging, arrows, and title
	//must already be shown. Simply prints each line. POS is the position
	//of the scroll's line in the text. This is of the center line.
	int t1;
	unsigned int old_pos=pos;
	char far *where;
	prepare_robot_mem(!id);
	where=&robot_mem[scrolls[id].mesg_location];
	m_hide();
	//Display center line
	fill_line(64,8,12,32+(scroll_base_color<<8),current_pg_seg);
	write_line(&where[pos],8,12,scroll_base_color,current_pg_seg);
	//Display lines above center line
	for(t1=11;t1>=6;t1--) {
		fill_line(64,8,t1,32+(scroll_base_color<<8),current_pg_seg);
		//Go backward to previous line
		if(where[pos]!=1) {
			if(where[--pos]=='\n') {
				do {
					pos--;
				} while((where[pos]!='\n')&&(where[pos]!=1));
				//At start of prev. line -1. Display.
				write_line(&where[++pos],8,t1,scroll_base_color,current_pg_seg);
				}
			}
		//Next line...
		}
	//Display lines below center line
	pos=old_pos;
	for(t1=13;t1<=18;t1++) {
		fill_line(64,8,t1,32+(scroll_base_color<<8),current_pg_seg);
		if(where[pos]==0) continue;
		//Go forward to next line
		while(where[pos]!='\n')	pos++;
		//At end of current. If next is a 0, don't show nuthin'.
		if(where[++pos])
			write_line(&where[pos],8,t1,scroll_base_color,current_pg_seg);
		//Next line...
		}
	m_show();
}

char far scr_nm_strs[5][12]={ "  Scroll   ","   Sign    ","Edit Scroll",
	"   Help    ","" };

void scroll_edging(char type) {
	//Displays the edging box of scrolls. Type=0 for Scroll, 1 for Sign, 2
	//for Edit Scroll, 3 for Help, 4 for Robot (w/o title)
	//Doesn't save the screen.
	m_hide();
	//Box for the title
	draw_window_box(5,3,74,5,current_pg_seg,scroll_base_color&240,
		scroll_base_color,scroll_corner_color);
	//Main box
	draw_window_box(5,5,74,19,current_pg_seg,scroll_base_color,
		scroll_base_color&240,scroll_corner_color);/* Text on 6 - 18 */
	//Shows keys in a box at the bottom
	draw_window_box(5,19,74,21+(type==3),current_pg_seg,scroll_base_color&240,
		scroll_base_color,scroll_corner_color);
	//Fix chars on edging
	draw_char(217,scroll_base_color,74,5,current_pg_seg);
	draw_char(217,scroll_base_color&240,74,19,current_pg_seg);
	//Add arrows
	draw_char(16,scroll_pointer_color,6,12,current_pg_seg);
	draw_char(17,scroll_pointer_color,73,12,current_pg_seg);
	//Write title
	write_string(scr_nm_strs[type],34,4,scroll_title_color,current_pg_seg);
	//Write key reminders
	if(type==2) write_string(": Move cursor   Alt+C: Character   Esc: Done editing",
		13,20,scroll_corner_color,current_pg_seg);
	else if(type<2) write_string(": Scroll text   Esc/Enter: End reading",
		21,20,scroll_corner_color,current_pg_seg);
	else if(type==3) {
		write_string(":Scroll text  Esc:Exit help  Enter:Select  Alt+P:Print",
			13,20,scroll_corner_color,current_pg_seg);
		write_string("F1:Help on Help  Alt+F1:Table of Contents",
			20,21,scroll_corner_color,current_pg_seg);
		}
	else write_string(":Scroll text  Esc:Exit  Enter:Select",21,20,
		scroll_corner_color,current_pg_seg);
	m_show();
}

void help_display(unsigned char far *help,unsigned int offs,char far *file,
 char far *label) {
	//Display a help file
	unsigned int pos=offs,old_pos,next_pos;//Where
	int key,fad=is_faded();
	int t1,t2;
	char old_keyb=curr_table;
	char mclick;
	switch_keyb_table(1);
	allow_help=0;
	//Draw screen
	save_screen(current_pg_seg);
	if(fad) {
		clear_screen(1824,current_pg_seg);
		insta_fadein();
		}
	scroll_edging(3);
	//Loop
	file[0]=label[0]=0;
	do {
		//Display scroll
		help_frame(help,pos);
		mclick=0;
		key=getkey();
		old_pos=pos;
		switch(key) {
			case -25://Alt+P
				//Print this section
				//Find start....
				do {
					pos--;
				} while(help[pos]!=1);
				pos++;
				if(fputc(10,stdprn)!=10) break;//Error on printer
				if(fputc(13,stdprn)!=13) break;//Error on printer
				//Print ea. line
				do {
					next_pos=pos;
					while(help[next_pos]!='\n') next_pos++;
					//Temp 0            ]
					help[next_pos]=0;
					if(help[pos]!=255) {
						//Normal line.
						if(print(&help[pos])) break;
						}
					else {
						pos++;
						//Special
						switch(help[pos++]) {
							case '>':
							case '<':
								if(fputc('>',stdprn)!='>') break;
								if(fputc(' ',stdprn)!=' ') break;
							case ':':
								pos+=help[pos]+2;
								if(print(&help[pos])) break;
								break;
							case '$':
								t1=str_len_color(&help[pos]);
								if(t1<80)
									for(t2=0;t2<(40-(t1>>1));t2++)
										if(fputc(' ',stdprn)!=' ') break;
								if(print(&help[pos])) break;
								break;
							}
						}
					//Next line?
					help[next_pos++]='\n';
					pos=next_pos;
				} while(help[pos]!=0);
				pos=old_pos;
				break;
			case -59://F1
				//Jump to label 000 in HELPONHE.HLP
				str_cpy(file,"HELPONHE.HLP");
				str_cpy(label,"000");
				goto ex;
			case -104://Alt+F1
				//Jump to label 072 in MAIN.HLP
				str_cpy(file,"MAIN.HLP");
				str_cpy(label,"072");
				goto ex;
			case MOUSE_EVENT:
				//Move to line clicked on if mouse is in scroll, else exit
				if((mouse_event.cy>=6)&&(mouse_event.cy<=18)&&
					(mouse_event.cx>=8)&&(mouse_event.cx<=71)) {
						mclick=1;
						t1=mouse_event.cy-12;
						if(t1==0) goto option;
						//t1<0 = PGUP t1 lines
						//t1>0 = PGDN t1 lines
						if(t1<0) goto pgup;
						goto pgdn;
						}
				key=27;
				break;
			case -72://Up
			up_a_line:
				//Go back a line (if possible)
				if(help[pos-1]==1) break;//Can't.
				pos--;
				//Go to start of this line.
				do {
					pos--;
				} while((help[pos]!='\n')&&(help[pos]!=1));
				pos++;
				//Done.
				break;
			case -80://Down
			down_a_line:
				//Go forward a line (if possible)
				while(help[pos]!='\n') pos++;
				//At end of current. Is there a next line?
				if(help[++pos]==0) {
					//Nope.
					pos=old_pos;
					break;
					}
				//Yep. Done.
				break;
			case 13://Enter
			option:
				//Option?
				if((help[pos]==255)&&((help[pos+1]=='>')||
					(help[pos+1]=='<'))) {
						//Yep!
						if(help[++pos]=='<') {
							//Get file and label and exit
							t1=0;
							pos++;
							do {
								file[t1++]=help[++pos];
							} while(help[pos]!=':');
							file[t1-1]=0;
							str_cpy(label,&help[pos+1]);
							goto ex;
							}
						//Get label and jump
						str_cpy(label,&help[pos+2]);
						//Search backwards for a 1
						do {
							pos--;
						} while(help[pos]!=1);
						//Search for label OR a \n followed by a \0
						do {
							pos++;
							if(help[pos]==255)
								if(help[pos+1]==':')
									if(!str_cmp(&help[pos+3],label))
										//pos is correct!
										goto labdone;
							if(help[pos]=='\n')
								if(help[pos+1]==0) break;
						} while(1);
						}
				//If there WAS an option, any existing label was found.
			labdone:
				break;
			case -81://Pagedown (by 6 lines)
				for(t1=6;t1>0;t1--) {
				pgdn:
					//Go forward a line (if possible)
					old_pos=pos;
					while(help[pos]!='\n') pos++;
					//At end of current. Is there a next line?
					if(help[++pos]==0) {
						//Nope.
						pos=old_pos;
						break;
						}
					//Yep. Done.
					}
				if(mclick) goto option;
				break;
			case -73://Pageup (by 6 lines)
				for(t1=-6;t1<0;t1++) {
				pgup:
					//Go back a line (if possible)
					if(help[pos-1]==1) break;//Can't.
					pos--;
					//Go to start of this line.
					do {
						pos--;
					} while((help[pos]!='\n')&&(help[pos]!=1));
					pos++;
					//Done.
					}
				if(mclick) goto option;
				break;
			case -71://Home
				t1=-30000;
				goto pgup;
			case -79://End
				t1=30000;
				goto pgdn;
			default:
				beep();
			case 27:
			case 0:
				break;
			}
		//Continue?
	} while(key!=27);
	//Restore screen and exit
ex:
	if(fad) insta_fadeout();
	restore_screen(current_pg_seg);
	allow_help=1;
	switch_keyb_table(old_keyb);
}

void help_frame(unsigned char far *help,unsigned int pos) {
	//Displays one frame of the help. Simply prints each line. POS is the
	//position of the center line.
	int t1,t2;
	int first=12;
	unsigned int next_pos;
	m_hide();
	//Work backwards to line
	do {
		if(help[pos-1]==1) break;//Can't.
		pos--;
		//Go to start of this line.
		do {
			pos--;
		} while((help[pos]!='\n')&&(help[pos]!=1));
		pos++;
		//Back a line!
		first--;
	} while(first>6);
	//First holds first line pos (6-12) to draw
	if(first>6) {
		for(t1=6;t1<first;t1++)
			fill_line(64,8,t1,32+(scroll_base_color<<8),current_pg_seg);
		}
	//Display from First to either 18 or end of help
	for(t1=first;t1<19;t1++) {
		//Fill...
		fill_line(64,8,t1,32+(scroll_base_color<<8),current_pg_seg);
		//Find NEXT line NOW - Actually get end of this one.
		next_pos=pos;
		while(help[next_pos]!='\n') next_pos++;
		//Temp. make a 0
		help[next_pos]=0;
		//Write- What TYPE is it?
		if(help[pos]!=255) //Normal
			color_string(&help[pos],8,t1,scroll_base_color,current_pg_seg);
		else {
			switch(help[++pos]) {
				case '$':
					//Centered. :)
					t2=str_len_color(&help[++pos]);
					color_string(&help[pos],40-(t2>>1),t1,scroll_base_color,
						current_pg_seg);
					break;
				case '>':
				case '<':
					//Option- Jump to AFTER dest. label/file
					pos+=help[++pos]+2;
					//Now show, two spaces over
					color_string(&help[pos],10,t1,scroll_base_color,current_pg_seg);
					//Add arrow
					draw_char('',scroll_arrow_color,8,t1,current_pg_seg);
					break;
				case ':':
					//Label- Jump to mesg and show
					pos+=help[++pos]+2;
					color_string(&help[pos],8,t1,scroll_base_color,current_pg_seg);
					break;
				}
			}
		//Now fix EOS to be a \n
		help[next_pos]='\n';
		//Next line...
		if(help[(pos=(++next_pos))]==0) break;
		}
	if(t1<19) {
		for(t1+=1;t1<19;t1++)
			fill_line(64,8,t1,32+(scroll_base_color<<8),current_pg_seg);
		}
	m_show();
}
