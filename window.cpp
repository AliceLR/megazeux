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

// Windowing code- Save/restore screen and draw windows and other elements,
//                 also displays and runs dialog boxes.

#include "helpsys.h"
#include "hexchar.h"
#include "timer.h"
#include "sfx.h"
#include "ezboard.h"
#include "mod.h"
#include "error.h"
#include "boardmem.h"
#include <stdio.h>
#include "mouse.h"
#include "beep.h"
#include "intake.h"
#include <stdlib.h>
#include "cursor.h"
#include "window.h"
#include "getkey.h"
#include "graphics.h"
#include "string.h"
extern "C" int         _CType strcmp(const char *__s1, const char *__s2);
#include <_null.h>
#include "meminter.h"
#include <dos.h>
#include "const.h"
#include "data.h"
#include <dir.h>
#include "helpsys.h"

#define NUM_SAVSCR 2

// Storage for NUM_SAVSCR saved screens- dynamically allocated at runtime
// Another two saved screens use ega pages 2 and 3.

char far *screen_storage[NUM_SAVSCR+2]={ NULL,NULL,
	(char far *)MK_FP(0xBA00,0),(char far *)MK_FP(0xBB00,0) };
char curr_screen=0;//Current space for save_screen and restore_screen
char far *vid_usage=NULL;//Mouse video usage array (for dialogs)

// Reserve memory- returns 0 if ok, non-0 for error.

char window_cpp_entry(void) {
	int t1=0;
	if((vid_usage=(char far *)farmalloc(2000))==NULL) return 1;
	for(;t1<NUM_SAVSCR;t1++) {
		screen_storage[t1]=(char far *)farmalloc(4000);
		if(screen_storage[t1]==NULL) break;
		}
	if(t1<NUM_SAVSCR) {//Allocation error
		if(t1>0) {
			for(t1--;t1>=0;t1--) {
				farfree(screen_storage[t1]);
				screen_storage[t1]=NULL;
				}
			}
		farfree(vid_usage);
		vid_usage=NULL;
		return 1;
		}
	return 0;
}

// Free up memory.

void window_cpp_exit(void) {
	int t1=0;
	if(vid_usage!=NULL) farfree(vid_usage);
	for(;t1<NUM_SAVSCR;t1++) {
		if(screen_storage[t1]!=NULL) farfree(screen_storage[t1]);
		}
}

// The following functions do NOT check to see if memory is reserved, in
// the interest of time, space, and simplicity. Make sure you have already
// called window_cpp_init.

// Saves current screen to buffer and increases buffer count. Returns
// non-0 if the buffer for screens is already full. (IE 6 count)

char save_screen(unsigned int segment) {
	if(curr_screen>=(NUM_SAVSCR+2)) {
		curr_screen=0;
		error("Windowing code bug",2,20,segment,0x1F01);
		}
	m_hide();
	mem_cpy(screen_storage[curr_screen++],(char far *)MK_FP(segment,0),4000);
	m_show();
	return 0;
}

// Restores top screen from buffer to screen and decreases buffer count.
// Returns non-0 if there are no screens in the buffer.

char restore_screen(unsigned int segment) {
	if(curr_screen==0)
		error("Windowing code bug",2,20,segment,0x1F02);
	m_hide();
	mem_cpy((char far *)MK_FP(segment,0),screen_storage[--curr_screen],4000);
	m_show();
	return 0;
}

// Draws an outlined and filled box from x1/y1 to x2/y2. Color is used to
// fill box and for upper left lines. Dark_color is used for lower right
// lines. Corner_color is used for the single chars in the upper right and
// lower left corners. If Corner_color equals 0, these are auto calculated.
// Returns non-0 for invalid parameters. Shadow (black spaces) is drawn and
// clipped if specified. Operation on a size smaller than 3x3 is undefined.
// This routine is highly unoptimized. Center is not filled if fill_center
// is set to 0. (defaults to 1)

char draw_window_box(int x1,int y1,int x2,int y2,unsigned int segment,
 unsigned char color,unsigned char dark_color,unsigned char corner_color,
 char shadow,char fill_center) {
	int t1,t2;
	//Validate parameters
	if((x1<0)||(y1<0)||(x1>79)||(y1>24)) return 1;
	if((x2<0)||(y2<0)||(x2>79)||(y2>24)) return 1;
	if(x2<x1) {
		t1=x1;
		x1=x2;
		x2=t1;
		}
	if(y2<y1) {
		t1=y1;
		y1=y2;
		y2=t1;
		}
	m_hide();
	//Fill center
	if(fill_center) {
		for(t1=x1+1;t1<x2;t1++) {
			for(t2=y1+1;t2<y2;t2++) {
				draw_char(' ',color,t1,t2,segment);
				}
			}
		}
	//Draw top and bottom edges
	for(t1=x1+1;t1<x2;t1++) {
		draw_char('Ä',color,t1,y1,segment);
		draw_char('Ä',dark_color,t1,y2,segment);
		}
	//Draw left and right edges
	for(t2=y1+1;t2<y2;t2++) {
		draw_char('³',color,x1,t2,segment);
		draw_char('³',dark_color,x2,t2,segment);
		}
	//Draw corners
	draw_char('Ú',color,x1,y1,segment);
	draw_char('Ù',dark_color,x2,y2,segment);
	if(corner_color) {
		draw_char('À',corner_color,x1,y2,segment);
		draw_char('¿',corner_color,x2,y1,segment);
		}
	else {
		draw_char('À',color,x1,y2,segment);
		draw_char('¿',dark_color,x2,y1,segment);
		}
	//Draw shadow if applicable
	if(shadow) {
		//Right edge
		if(x2<79) {
			for(t2=y1+1;t2<=y2;t2++) {
				draw_char(' ',0,x2+1,t2,segment);
				}
			}
		//Lower edge
		if(y2<24) {
			for(t1=x1+1;t1<=x2;t1++) {
				draw_char(' ',0,t1,y2+1,segment);
				}
			}
		//Lower right corner
		if((y2<24)&&(x2<79)) {
			draw_char(' ',0,x2+1,y2+1,segment);
			}
		}
	m_show();
	return 0;
}

// Strings for drawing different dialog box elements.
// All parts are assumed 3 wide.

#define check_on "[û]"
#define check_off "[ ]"
#define radio_on "()"
#define radio_off "( )"
#define color_blank ' '
#define color_wild '?'
#define color_dot 'þ'
#define list_button "  "
#define num_buttons "    "

//Foreground colors that look nice for each background color
char fg_per_bk[16]={ 15,15,15,15,15,15,15,0,15,15,0,0,15,15,0,0 };

// For list/choice menus-

#define arrow_char ''

// List/choice menu colors- Box same as dialog, elements same as
// inactive question, current element same as active question, pointer
// same as text, title same as title.

// Put up a box listing choices. Move pointer and enter to pick. choice
// size is the size in bytes per choice in the array of choices. This size
// includes null terminator and any junk padding. All work is done on
// segment given, no page flipping. Choices are drawn using color_string.
// On ESC, returns negative current choice, unless current choice is 0,
// then returns -32767. Returns OUT_OF_WIN_MEM if out of screen storage space.

// A progress column is displayed directly to the right of the list, using
// the following characters. This is mainly used for mouse support.

#define pc_top_arrow ''
#define pc_bottom_arrow ''
#define pc_filler '±'
#define pc_dot 'þ'

// Mouse support- Click on a choice to move there auto, click on progress
// column arrows to move one line, click on progress meter itself to move
// there percentage-wise. If you click on the current choice, it exits.
// If you click on a choice to move there, the mouse cursor moves with it.
int list_menu(char far *choices,char choice_size,char far *title,int
 current,int num_choices,unsigned int segment,char xpos) {
	int width=choice_size+6,t1;
	if(str_len(title)>choice_size) width=str_len(title)+6;
	//Save screen
	if(save_screen(segment)) return OUT_OF_WIN_MEM;

	//Display box
	m_hide();
	draw_window_box(xpos,2,xpos+width-1,22,segment,DI_MAIN,DI_DARK,DI_CORNER);
	//Add title
	write_string(title,xpos+3,2,DI_TITLE,segment);
	draw_char(' ',DI_TITLE,xpos+2,2,segment);
	draw_char(' ',DI_TITLE,xpos+3+str_len(title),2,segment);
	//Add pointer
	draw_char(arrow_char,DI_TEXT,xpos+2,12,segment);
	//Add meter arrows
	if(num_choices>1) {
		draw_char(pc_top_arrow,DI_PCARROW,xpos+4+choice_size,3,segment);
		draw_char(pc_bottom_arrow,DI_PCARROW,xpos+4+choice_size,21,segment);
		}
	do {
		//Fill over old menu
		draw_window_box(xpos+3,3,xpos+3+choice_size,21,segment,DI_DARK,DI_MAIN,
			DI_CORNER,0);
		//Draw current
		color_string(&(choices[choice_size*current]),xpos+4,12,DI_ACTIVE,
			segment);
		//Draw above current
		for(t1=1;t1<9;t1++) {
			if((current-t1)<0) break;
			color_string(&(choices[choice_size*(current-t1)]),xpos+4,12-t1,
				DI_NONACTIVE,segment);
			}
		//Draw below current
		for(t1=1;t1<9;t1++) {
			if((current+t1)>=num_choices) break;
			color_string(&(choices[choice_size*(current+t1)]),xpos+4,12+t1,
				DI_NONACTIVE,segment);
			}
		//Draw meter (xpos 9+choice_size, ypos 4 thru 20)
		if(num_choices>1) {
			for(t1=4;t1<21;t1++)
				draw_char(pc_filler,DI_PCFILLER,xpos+4+choice_size,t1,segment);
			//Add progress dot
			t1=(current*16)/(num_choices-1);
			draw_char(pc_dot,DI_PCDOT,xpos+4+choice_size,t1+4,segment);
			}
		//Get keypress
		m_show();
		t1=getkey();
		m_hide();
      
		//Act upon it
		switch(t1) {
			case MOUSE_EVENT:
				//Clicking on- List, column, arrow, or nothing?
				if((mouse_event.cx==xpos+4+choice_size)&&(mouse_event.cy>3)&&
					(mouse_event.cy<21)) {//Column
					t1=mouse_event.cy-4;
					current=(t1*(num_choices-1))/16;
					}
				else if((mouse_event.cy>3)&&(mouse_event.cy<21)&&
					(mouse_event.cx>xpos+3)&&(mouse_event.cx<xpos+3+choice_size)) {
					//List
					if(mouse_event.cy==12) goto enter;
					current+=(mouse_event.cy)-12;
					if(current<0) current=0;
					if(current>=num_choices) current=num_choices-1;
					//Move mouse with choices
					m_move(mouse_event.cx,12);
					}
				else if((mouse_event.cx==xpos+4+choice_size)&&
					(mouse_event.cy==3)) {//Top arrow
					if(current>0) current--;
					}
				else if((mouse_event.cx==xpos+4+choice_size)&&
					(mouse_event.cy==21)) {//Bottom arrow
					if(current<(num_choices-1)) current++;
					}
				else beep();//Nothing of importance was clicked on
				break;
			case 27:
				//ESC
				restore_screen(segment);
				m_show();
				if(current==0) return -32767;
				else return -current;
			case ' ':
			case 13:
			enter:
				//Selected
				restore_screen(segment);
				m_show();
				return current;
			case -72:
			case '8':
				//Up
				if(current>0) current--;
				break;
			case -80:
			case '2':
				//Down
				if(current<(num_choices-1)) current++;
				break;
			case -73:
				//Page Up
				current-=8;
				if(current<0) current=0;
				break;
			case -81:
				//Page Down
				current+=8;
				if(current>=num_choices) current=num_choices-1;
				break;
			case -71:
				//Home
				current=0;
				break;
			case -79:
				//End
				current=num_choices-1;
				break;
			default:
        // Not necessarily invalid. Might be an alphanumeric; seek the
        // sucker.

        if((t1 >= 'A') && (t1 <= 'Z'))
        {
          int i;
    
          for(i = 0; i < num_choices; i++)
          {
            if((choices[i * choice_size] == ' ') &&
             (choices[(i * choice_size) + 1] == t1))
            {
              // It's good!
              current = i;
              break;
            }
          }
        }          

        if(((t1 >= 'a') && (t1 <= 'z')) || ((t1 >= '0') && (t1 <= '9')))
        {
          int i;

          if((t1 >= 'a') && (t1 <= 'z'))
          {
            t1 -= 32;
          }

          for(i = 0; i < num_choices; i++)
          {
            if(choices[i * choice_size] == t1)
            {
              // It's good!
              current = i;
              break;
            }
          }
        }

      // I couldn't get rid of it for shift + num, besides that it
      // annoys the hell out of me anyway. ^^ - Exo
      /*  else
        {
				  //Invalid character
				  beep();
        } */
			case 0:
				break;
			}
		//Loop
	} while(1);
}

// For char selection screen

#define char_sel_arrows_0 ''
#define char_sel_arrows_1 ''
#define char_sel_arrows_2 ''
#define char_sel_arrows_3 ''

// Char selection screen colors- Window colors same as dialog, non-current
// characters same as nonactive, current character same as active, arrows
// along edges same as text, title same as title.

// Put up a character selection box. Returns selected, or negative selected
// for ESC, -256 for -0. Returns OUT_OF_WIN_MEM if out of window mem.
// Display is 32 across, 8 down. All work done on given segment.

// Mouse support- Click on a character to select it. If it is the current
// character, it exits.
int char_selection(unsigned char current,unsigned int segment) {
	int t1,t2,t3;
	//Save screen
	if(save_screen(segment)) return OUT_OF_WIN_MEM;
	if(context==72) set_context(98);
	else set_context(context);
	//Draw box
	m_hide();
	draw_window_box(20,5,57,16,segment,DI_MAIN,DI_DARK,DI_CORNER);
	//Add title
	write_string(" Select a character ",22,5,DI_TITLE,segment);
	//Draw character set
	for(t1=0;t1<32;t1++) {
		for(t2=0;t2<8;t2++) {
			draw_char(t1+t2*32,DI_NONACTIVE,t1+23,t2+7,segment);
			}
		}
	do {
		//Calculate x/y
		t1=(current&31)+23; t2=(current>>5)+7,
		//Highlight active character
		draw_char(current,DI_ACTIVE,t1,t2,segment);
		//Draw arrows
		draw_window_box(22,6,55,15,segment,DI_DARK,DI_MAIN,DI_CORNER,0,0);
		draw_char(char_sel_arrows_0,DI_TEXT,t1,15,segment);
		draw_char(char_sel_arrows_1,DI_TEXT,t1,6,segment);
		draw_char(char_sel_arrows_2,DI_TEXT,22,t2,segment);
		draw_char(char_sel_arrows_3,DI_TEXT,55,t2,segment);
		//Write number of character
		write_number(current,DI_MAIN,53,16,segment,3);
		//Get key
		m_show();
		t3=getkey();
		m_hide();
		//Erase marks
		draw_char(current,DI_NONACTIVE,t1,t2,segment);
		//Process key
		switch(t3) {
			case MOUSE_EVENT:
				//Within area?
				if((mouse_event.cx<23)||(mouse_event.cx>54)||
					(mouse_event.cy<7)||(mouse_event.cy>14)) {//Nope!
					beep();
					break;
					}
				//Yep!
				t1=(mouse_event.cx-23)+((mouse_event.cy-7)<<5);
				if(current==t1) goto enter;
				current=t1;
				break;
			case 27:
				//ESC
				pop_context();
				restore_screen(segment);
				m_show();
				if(current==0) return -256;
				else return -current;
			case ' ':
			case 13:
			enter:
				//Selected
				pop_context();
				restore_screen(segment);
				m_show();
				return current;
			case -72:
			case '8':
				//Up
				if(current>31) current-=32;
				break;
			case -80:
			case '2':
				//Down
				if(current<224) current+=32;
				break;
			case -75:
			case '4':
				//Left
				if(t1>23) current--;
				break;
			case -77:
			case '6':
				//Right
				if(t1<54) current++;
				break;
			case -71:
				//Home
				current=0;
				break;
			case -79:
				//End
				current=255;
				break;
			default:
				//If this is from 32 to 255, jump there.
				//Else beep.
				if(t3>31) current=t3;
				else beep();
			case 0:
				break;
			}
		//Loop
	} while(1);
}

// For color selection screen

#define color_sel_char 'þ'
#define color_sel_wild '?'

// Color selection screen colors- see char selection screen.

// Put up a color selection box. Returns selected, or negative selected
// for ESC, -512 for -0. Returns OUT_OF_WIN_MEM if out of window mem.
// Display is 16 across, 16 down. All work done on given segment. Current
// uses bit 256 for the wild bit.

// Mouse support- Click on a color to select it. If it is the current color,
// it exits.
int color_selection(int current,unsigned int segment,char allow_wild) {
	int t1,t2,t3;
	char currx,curry;
	//Save screen
	if(save_screen(segment)) return OUT_OF_WIN_MEM;
	if(context==72) set_context(98);
	else set_context(context);
	//Ensure allow_wild is 0 or 1
	allow_wild=(allow_wild!=0);
	//Calculate current x/y
	if((current&256)&&(allow_wild)) {
		//Wild
		current-=256;
		if(current<16) {
			currx=current;
			curry=16;
			}
		else {
			curry=current-16;
			currx=16;
			}
		}
	else if(current&256) {
		//Wild when not allowed to be
		current-=256;
		currx=current&15;
		curry=current>>4;
		}
	else {
		//Normal
		currx=current&15;
		curry=current>>4;
		}
	//Draw box
	m_hide();
	draw_window_box(12,2,33+allow_wild,21+allow_wild,segment,DI_MAIN,DI_DARK,
		DI_CORNER);
	//Add title
	write_string(" Select a color ",14,2,DI_TITLE,segment);
	do {
		//Draw outer edge
		draw_window_box(14,3,31+allow_wild,20+allow_wild,segment,DI_DARK,
			DI_MAIN,DI_CORNER,0,0);
		//Draw colors
		for(t1=0;t1<16+allow_wild;t1++) {
			for(t2=0;t2<16+allow_wild;t2++) {
				if(t1==16) {
					if(t2==16) draw_char(color_sel_wild,135,t1+15,t2+4,segment);
					else draw_char(color_sel_wild,fg_per_bk[t2]+t2*16,t1+15,t2+4,
						segment);
					}
				else if(t2==16) {
					if(t1==0) draw_char(color_sel_wild,128,t1+15,t2+4,segment);
					else draw_char(color_sel_wild,t1,t1+15,t2+4,segment);
					}
				else draw_char(color_sel_char,t1+t2*16,t1+15,t2+4,segment);
				}
			}
		//Add selection box
		draw_window_box(currx+14,curry+3,currx+16,curry+5,segment,DI_MAIN,DI_MAIN,
			DI_MAIN,0,0);
		//Write number of color
		if((allow_wild)&&((currx==16)||(curry==16))) {
			//Convert wild
			if(currx==16) {
				if(curry==16) t1=288;
				else t1=272+curry;
				}
			else t1=256+currx;
			}
		else t1=currx+curry*16;
		write_number(t1,DI_MAIN,28+allow_wild,21+allow_wild,segment,3);
		//Get key
		m_show();
		t3=getkey();
		m_hide();
		//Process
		switch(t3) {
			case MOUSE_EVENT:
				//Within area?
				if((mouse_event.cx<15)||(mouse_event.cx>30+allow_wild)||
					(mouse_event.cy<4)||(mouse_event.cy>19+allow_wild)) {//Nope!
					beep();
					break;
					}
				//Yep!
				t1=mouse_event.cx-15;
				t2=mouse_event.cy-4;
				if((currx==t1)&&(curry==t2)) {
					t3=13;
					goto enter;
					}
				currx=t1;
				curry=t2;
				break;
			case 27:
				//ESC
			case ' ':
			case 13:
			enter:
				pop_context();
				//Selected
				restore_screen(segment);
				m_show();
				if((allow_wild)&&((currx==16)||(curry==16))) {
					//Convert wild
					if(currx==16) {
						if(curry==16) current=288;
						else current=272+curry;
						}
					else current=256+currx;
					}
				else current=currx+curry*16;
				if(t3==27) {
					if(current==0) current=-512;
					else current=-current;
					}
				return current;
			case -72:
			case '8':
				//Up
				if(curry>0) curry--;
				break;
			case -80:
			case '2':
				//Down
				if(curry<(15+allow_wild)) curry++;
				break;
			case -75:
			case '4':
				//Left
				if(currx>0) currx--;
				break;
			case -77:
			case '6':
				//Right
				if(currx<(15+allow_wild)) currx++;
				break;
			case -71:
				//Home
				currx=curry=0;
				break;
			case -79:
				//End
				currx=curry=15+allow_wild;
				break;
			default:
				//Invalid character- beep.
				beep();
			case 0:
				break;
			}
		//Loop
	} while(1);
}

//Short function to display a color as a colored box
void draw_color_box(unsigned char color,char q_bit,char x,char y,
 unsigned int segment) {
	m_hide();
	//If q_bit is set, there are unknowns
	if(q_bit) {
		if(color<16) {
			//Unknown background
			//Use black except for black fg, which uses grey
			if(color==0) color+=128;
			draw_char(color_wild,color,x,y,segment);
			draw_char(color_dot,color,x+1,y,segment);
			draw_char(color_wild,color,x+2,y,segment);
			}
		else if(color<32) {
			//Unkown foreground
			//Use foreground from array
			color-=16;
			color=(color<<4)+fg_per_bk[color];
			draw_char(color_wild,color,x,y,segment);
			draw_char(color_wild,color,x+1,y,segment);
			draw_char(color_wild,color,x+2,y,segment);
			}
		else {
			//Both unknown
			draw_char(color_wild,8,x,y,segment);
			draw_char(color_wild,135,x+1,y,segment);
			draw_char(color_wild,127,x+2,y,segment);
			}
		}
	else {
		draw_char(color_blank,color,x,y,segment);
		draw_char(color_dot,color,x+1,y,segment);
		draw_char(color_blank,color,x+2,y,segment);
		}
	m_show();
}

//Prototype- Internal function that displays a dialog box element.
void display_element(unsigned int segment,char type,char x,char y,
	char far *str,int p1,int p2,void far *value,char active=0,
	char curr_check=0,char set_vid_usage=0);

//The code that runs the dialog boxes... Returns OUT_OF_WIN_MEM if can't
//save screen.

//Mouse support- Click to move/select/choose.
int run_dialog(dialog far *di,unsigned int segment,char reset_curr,
 char sfx_alt_t) {
	int t1,t2,key;
	long tlng;
	char mouse_held=0;
	//Current radio button or check box
	int curr_check=0;
	//Use these to save time and coding hassle
	int num_e=di->num_elements;
	int curr_e=di->curr_element;
	int x1=di->x1;
	int y1=di->y1;
	//X pos of title
	int titlex=x1+((di->x2-x1+1)>>1)-(str_len(di->title)>>1);
	//Save screen
	if(save_screen(segment)) return OUT_OF_WIN_MEM;
	if(context==72) set_context(98);
	else set_context(context);
	//Clear mouse usage array
	for(t1=0;t1<2000;t1++)
		vid_usage[t1]=-1;
	//Turn cursor off
	cursor_off();
	//Draw main box and title
	m_hide();
	draw_window_box(x1,y1,di->x2,di->y2,segment,DI_MAIN,DI_DARK,
		DI_CORNER);
	write_string(di->title,titlex,y1,DI_TITLE,segment);
	draw_char(' ',DI_TITLE,titlex-1,y1,segment);
	draw_char(' ',DI_TITLE,titlex+str_len(di->title),y1,segment);
	//Reset current
	if(reset_curr) di->curr_element=curr_e=0;
	//Initial draw of elements, as well as init of vid_usage
	for(t1=0;t1<num_e;t1++)
		display_element(segment,di->element_types[t1],x1+di->element_xs[t1],
			y1+di->element_ys[t1],di->element_strs[t1],di->element_param1s[t1],
			di->element_param2s[t1],di->element_storage[t1],t1==curr_e,0,t1+1);
	//Loop of selection
	do {
		//Check current...
		do {
			if(curr_e>=num_e) curr_e=0;
			t1=di->element_types[curr_e];
			if((t1==DE_BOX)||(t1==DE_TEXT)||(t1==DE_LINE))
				curr_e++;
			else break;
		} while(1);
		if(t1==DE_CHECK) {
			if(curr_check>=di->element_param1s[curr_e]) curr_check=0;
			}
		//Highlight current...
		display_element(segment,t1,x1+di->element_xs[curr_e],
			y1+di->element_ys[curr_e],di->element_strs[curr_e],
			di->element_param1s[curr_e],di->element_param2s[curr_e],
			di->element_storage[curr_e],1,curr_check);
		//Get key (through intake for DE_INPUT)
		m_show();
		if(mouse_held) {
			if(m_buttonstatus()&LEFTBDOWN) {
				if(mouse_held<3) {
					delay_time(100);
					if(!(m_buttonstatus()&LEFTBDOWN)) goto nombuttondn;
					mouse_held+=2;
					}
				if(mouse_held==3) {
					delay_time(5);
					key=-72;
					goto change_up;
					}
				else {
					delay_time(5);
					key=-80;
					goto change_down;
					}
				}
		nombuttondn:
			mouse_held=0;
			}
		if(t1==DE_INPUT) {
			key=intake((char far *)di->element_storage[curr_e],
				di->element_param1s[curr_e],x1+di->element_xs[curr_e]+
				str_len(di->element_strs[curr_e]),
				y1+di->element_ys[curr_e],segment,DI_INPUT,2,
				di->element_param2s[curr_e]);
			if(key==MOUSE_EVENT) acknowledge_mouse();
			}
		else key=getkey();
		//Process key (t1 still holds element type)
		switch(key) {
			case -20://Alt+T
				//Test sfx
				if(sfx_alt_t!=1) break;
				if((di->element_types[curr_e])!=DE_INPUT) break;
				//Play a sfx
				clear_sfx_queue();
				play_str((char far *)di->element_storage[curr_e]);
				break;
			case MOUSE_EVENT:
				//Is it over anything important?
				if(vid_usage[mouse_event.cx+mouse_event.cy*80]==-1) beep();//No.
				else {//Yes.
					//Get id number
					t2=vid_usage[mouse_event.cx+mouse_event.cy*80];
					if(t2!=curr_e) {
						//Unhighlight old element
						m_hide();
						display_element(segment,t1,x1+di->element_xs[curr_e],
							y1+di->element_ys[curr_e],di->element_strs[curr_e],
							di->element_param1s[curr_e],di->element_param2s[curr_e],
							di->element_storage[curr_e],0,curr_check);
						m_show();
						curr_e=t2;
						t2=-1;
						}
					//Get type of new one
					t1=di->element_types[curr_e];
					//Code for clicking on something... (t1!=-1 means it was
					//already current)
					switch(t1) {
						case DE_INPUT:
							//Nothing special
							break;
						case DE_CHECK:
							//Move curr_check and toggle
							curr_check=(mouse_event.cy-y1)-di->element_ys[curr_e];
							((char far *)di->element_storage[curr_e])[curr_check]^=1;
							break;
						case DE_RADIO:
							//Move radio button
							((char far *)di->element_storage[curr_e])[0]=
								(mouse_event.cy-y1)-di->element_ys[curr_e];
							break;
						case DE_COLOR:
						case DE_CHAR:
						case DE_LIST:
						case DE_BOARD:
							//Just like enter
							goto enter;
						case DE_BUTTON:
							//If it was already current, select. (enter)
							//Otherwise nothing happens.
							if(t2!=-1) goto enter;
							break;
						case DE_NUMBER:
							//Two types of numeral apply here.
							if((di->element_param1s[curr_e]==1)&&
								(di->element_param2s[curr_e]<10)) {//Number line
								//Select number IF on the number line itself
								t2=mouse_event.cx-x1;
								t2-=di->element_xs[curr_e];
								t2-=str_len(di->element_strs[curr_e]);
								if((t2<di->element_param2s[curr_e])&&(t2>=0))
									((int far *)di->element_storage[curr_e])[0]=t2+1;
								break;
								}
						case DE_FIVE:
							//Numeric selector
							//Go to change_up w/-72 as key for up button
							//Go to change_down w/-80 as key for down button
							//Anything else is ignored
							//Set mouse_held to cycle.
							t2=mouse_event.cx-7-x1;
							t2-=di->element_xs[curr_e];
							t2-=str_len(di->element_strs[curr_e]);
							if((t2>=0)&&(t2<=2)) {
								key=-72;
								mouse_held=1;
								goto change_up;
								}
							else if((t2>=3)&&(t2<=5)) {
								key=-80;
								mouse_held=2;
								goto change_down;
								}
							break;
						}
					}
				break;
			case 9://Tab
			next_elem:
				//Unhighlight old element
				m_hide();
				display_element(segment,t1,x1+di->element_xs[curr_e],
					y1+di->element_ys[curr_e],di->element_strs[curr_e],
					di->element_param1s[curr_e],di->element_param2s[curr_e],
					di->element_storage[curr_e],0,curr_check);
				m_show();
				//Move to next element
				curr_e++;
				break;
			case -15://Shift + Tab
			prev_elem:
				//Unhighlight old element
				m_hide();
				display_element(segment,t1,x1+di->element_xs[curr_e],
					y1+di->element_ys[curr_e],di->element_strs[curr_e],
					di->element_param1s[curr_e],di->element_param2s[curr_e],
					di->element_storage[curr_e],0,curr_check);
				m_show();
				//Move to previous element, going past text/box/line
				do {
					curr_e--;
					if(curr_e<0) curr_e=num_e-1;
					t1=di->element_types[curr_e];
					if((t1==DE_BOX)||(t1==DE_TEXT)||(t1==DE_LINE)) continue;
				} while(0);
				break;
			case 32://Space
			case 13://Enter
			enter:
				//Activate button, choose from list/menu, or toggle check.
				switch(t1) {
					case DE_CHECK:
						((char far *)di->element_storage[curr_e])[curr_check]^=1;
						break;
					case DE_COLOR:
						t2=color_selection(
							((int far *)di->element_storage[curr_e])[0],segment,
							di->element_param1s[curr_e]);
						if(t2>=0) ((int far *)di->element_storage[curr_e])[0]=t2;
						break;
					case DE_CHAR:
						t2=char_selection(
							((unsigned char far *)di->element_storage[curr_e])[0],
							segment);
						if(t2==0) t2=1;
						if((t2==255)&&(di->element_param1s[curr_e]==0)) t2=1;
						if(t2>=0)
							((unsigned char far *)di->element_storage[curr_e])[0]=t2;
						break;
					case DE_BUTTON:
						//Restore screen, set current, and return answer
						pop_context();
						restore_screen(segment);
						di->curr_element=curr_e;
						return di->element_param1s[curr_e];
					case DE_BOARD:
						t2=choose_board(((int far *)di->element_storage[curr_e])[0],
							di->element_strs[curr_e],segment,
							di->element_param1s[curr_e]);
						if(t2>=0) ((int far *)di->element_storage[curr_e])[0]=t2;
						break;
					case DE_LIST:
						t2=di->element_param2s[curr_e];
						t2=list_menu(&(di->element_strs[curr_e][t2]),t2,
							di->element_strs[curr_e],
							((int far *)di->element_storage[curr_e])[0],
							di->element_param1s[curr_e],segment);
						if(t2>=0) ((int far *)di->element_storage[curr_e])[0]=t2;
						break;
					default:
						//Same as a tab
						goto next_elem;
					}
				break;
			case -77://Right
				if((t1!=DE_NUMBER)&&(t1!=DE_FIVE)) goto change_down;
			case -72://Up
			case -152://Alt + Up
			case -141://Ctrl + Up
			case -73://PageUp
			change_up:
				//Change numeric, radio, or current check. Otherwise, move
				//to previous element.
				switch(t1) {
					case DE_RADIO:
						if(key==-73)
							((char far *)di->element_storage[curr_e])[0]=0;
						else {
							if(((char far *)di->element_storage[curr_e])[0]>0)
								((char far *)di->element_storage[curr_e])[0]--;
							}
						break;
					case DE_CHECK:
						if(key==-73) curr_check=0;
						else if(curr_check>0) curr_check--;
						break;
					case DE_NUMBER:
					case DE_FIVE:
						switch(key) {
							case -77://Right
							case -72://Up
								t2=1;
								break;
							case -152://Alt + Up
							case -141://Ctrl + Up
								t2=10;
								break;
							case -73://PageUp
								t2=100;
								break;
							}
					change_num:
						tlng=((int far *)di->element_storage[curr_e])[0];
						if((tlng+t2)<di->element_param1s[curr_e]) {
							((int far *)di->element_storage[curr_e])[0]=
								di->element_param1s[curr_e];
							break;
							}
						if((tlng+t2)>di->element_param2s[curr_e]) {
							((int far *)di->element_storage[curr_e])[0]=
								di->element_param2s[curr_e];
							break;
							}
						((int far *)di->element_storage[curr_e])[0]+=t2;
						break;
					default:
						goto prev_elem;
					}
				break;
			case -71://Home
				//Change numeric, radio, or current check. Otherwise, move
				//to first element.
				switch(t1) {
					case DE_NUMBER:
					case DE_FIVE:
						((int far *)di->element_storage[curr_e])[0]=
							di->element_param2s[curr_e];
						break;
					default:
						//Unhighlight old element
						m_hide();
						display_element(segment,t1,x1+di->element_xs[curr_e],
							y1+di->element_ys[curr_e],di->element_strs[curr_e],
							di->element_param1s[curr_e],di->element_param2s[curr_e],
							di->element_storage[curr_e],0,curr_check);
						m_show();
						//Jump to first
						curr_e=0;
						if(sfx_alt_t>1) curr_e=sfx_alt_t;
						else if(sfx_alt_t==1) curr_e=4;
						break;
					}
				break;
			case -75://Left
				if((t1!=DE_NUMBER)&&(t1!=DE_FIVE)) goto change_up;
			case -80://Down
			case -160://Alt + Down
			case -145://Ctrl + Down
			case -81://PageDown
			change_down:
				//Change numeric, radio, or current check. Otherwise, move
				//to next element.
				switch(t1) {
					case DE_RADIO:
						if(key==-81)
							((char far *)di->element_storage[curr_e])[0]=
								di->element_param1s[curr_e]-1;
						else {
							if(((char far *)di->element_storage[curr_e])[0]<
								di->element_param1s[curr_e]-1)
									((char far *)di->element_storage[curr_e])[0]++;
							}
						break;
					case DE_CHECK:
						if(key==-81) curr_check=di->element_param1s[curr_e]-1;
						else if(curr_check<di->element_param1s[curr_e]-1)
							curr_check++;
						break;
					case DE_NUMBER:
					case DE_FIVE:
						switch(key) {
							case -75://Left
							case -80://Down
								t2=-1;
								break;
							case -160://Alt + Down
							case -145://Ctrl + Down
								t2=-10;
								break;
							case -81://PageDown
								t2=-100;
								break;
							}
						goto change_num;
					default:
						goto next_elem;
					}
				break;
			case -79://End
				//Change numeric, radio, or current check. Otherwise, move
				//to last element.
				switch(t1) {
					case DE_NUMBER:
					case DE_FIVE:
						((int far *)di->element_storage[curr_e])[0]=
							di->element_param1s[curr_e];
						break;
					default:
						//Unhighlight old element
						m_hide();
						display_element(segment,t1,x1+di->element_xs[curr_e],
							y1+di->element_ys[curr_e],di->element_strs[curr_e],
							di->element_param1s[curr_e],di->element_param2s[curr_e],
							di->element_storage[curr_e],0,curr_check);
						m_show();
						//Jump to end
						if(sfx_alt_t>0) curr_e=0;
						else {
							curr_e=num_e-1;
							//Work backwards past found text, box, or line
							do {
								curr_e--;
								t1=di->element_types[curr_e];
								if((t1==DE_BOX)||(t1==DE_TEXT)||(t1==DE_LINE)) continue;
							} while(0);
							}
						break;
					}
				break;
			case 27://ESC
				//Restore screen, set current, and return -1
				restore_screen(segment);
				di->curr_element=curr_e;
				pop_context();
				return -1;
			default:
				//Change character to key. Otherwise beep.
				if((t1==DE_CHAR)&&(key>31)&&(key<255))
					((char far *)di->element_storage[curr_e])[0]=key;
				else beep();
			case 0:
				break;
			}
		m_hide();
		//Loop
	} while(1);
}

//Internal function to display an element, whether active or not.
//Does NOT preserve mouse cursor.
void display_element(unsigned int segment,char type,char x,char y,
 char far *str,int p1,int p2,void far *value,char active,char curr_check,
 char set_vid_usage) {
	//If set_vid_usage is non-0, set the vid_usage array appropriately.
	//set_vid_usage is equal to the element number plus 1.
	int t1,t2;
	char temp[7];
	//Color for elements (active vs. inactive)
	int color=DI_NONACTIVE;
	if(active) color=DI_ACTIVE;
	//Act according to type, and fill vid_usage array as well if flag set
	switch(type) {
		case DE_TEXT:
			color_string(str,x,y,DI_TEXT,segment);
			break;
		case DE_BOX:
			draw_window_box(x,y,x+p1-1,y+p2-1,segment,DI_DARK,DI_MAIN,DI_CORNER,
				0,0);
			break;
		case DE_LINE:
			if(p2) {
				//Vertical
				for(t1=0;t1<p1;t1++)
					draw_char('³',DI_LINE,x,y+t1,segment);
				}
			else {
				//Horizontal
				fill_line(p1,x,y,(DI_LINE<<8)+196,segment);
				}
			break;
		case DE_INPUT:
			write_string(str,x,y,color,segment);
			write_string((char far *)value,x+str_len(str),y,DI_INPUT,segment);
			fill_line(p1-str_len((char far *)value)+1,
				x+str_len(str)+str_len((char far *)value),y,(DI_INPUT<<8)+32,
				segment);
			//Fill vid_usage
			if(set_vid_usage) {
				for(t1=0;t1<=p1+str_len(str);t1++)
					vid_usage[t1+x+y*80]=set_vid_usage-1;
				}
			break;
		case DE_CHECK:
			write_string(str,x+4,y,DI_NONACTIVE,segment);
			//Draw boxes
			for(t1=0;t1<p1;t1++) {
				if(((char far *)value)[t1]) write_string(check_on,x,y+t1,
					DI_NONACTIVE,segment);
				else write_string(check_off,x,y+t1,DI_NONACTIVE,segment);
				}
			//Highlight current
			if(active) {
				if(curr_check>=p1) curr_check=p1-1;
				color_line(p2+4,x,y+curr_check,DI_ACTIVE,segment);
				}
			//Fill vid_usage
			if(set_vid_usage) {
				for(t1=0;t1<p1;t1++)
					for(t2=0;t2<p2+4;t2++)
						vid_usage[t2+x+(t1+y)*80]=set_vid_usage-1;
				}
			break;
		case DE_RADIO:
			write_string(str,x+4,y,DI_NONACTIVE,segment);
			//Draw boxes
			for(t1=0;t1<p1;t1++) {
				if(t1==((char far *)value)[0]) write_string(radio_on,x,y+t1,
					DI_NONACTIVE,segment);
				else write_string(radio_off,x,y+t1,DI_NONACTIVE,segment);
				}
			//Highlight current
			if(active) {
				color_line(p2+4,x,y+((char far *)value)[0],DI_ACTIVE,segment);
				}
			//Fill vid_usage
			if(set_vid_usage) {
				for(t1=0;t1<p1;t1++)
					for(t2=0;t2<p2+4;t2++)
						vid_usage[t2+x+(t1+y)*80]=set_vid_usage-1;
				}
			break;
		case DE_COLOR:
			write_string(str,x,y,color,segment);
			t1=((int far *)value)[0];
			draw_color_box(t1&255,(t1&256)==256,x+str_len(str),y,segment);
			//Fill vid_usage
			if(set_vid_usage) {
				t2=str_len(str)+3;
				for(t1=0;t1<t2;t1++)
					vid_usage[t1+x+y*80]=set_vid_usage-1;
				}
			break;
		case DE_CHAR:
			write_string(str,x,y,color,segment);
			draw_char(((unsigned char far *)value)[0],DI_CHAR,x+str_len(str)+1,
				y,segment);
			draw_char(' ',DI_CHAR,x+str_len(str),y,segment);
			draw_char(' ',DI_CHAR,x+str_len(str)+2,y,segment);
			//Fill vid_usage
			if(set_vid_usage) {
				t2=str_len(str)+3;
				for(t1=0;t1<t2;t1++)
					vid_usage[t1+x+y*80]=set_vid_usage-1;
				}
			break;
		case DE_BUTTON:
			if(active) color=DI_ACTIVEBUTTON;
			else color=DI_BUTTON;
			write_string(str,x+1,y,color,segment);
			draw_char(' ',color,x,y,segment);
			draw_char(' ',color,x+str_len(str)+1,y,segment);
			//Fill vid_usage
			if(set_vid_usage) {
				t2=str_len(str)+2;
				for(t1=0;t1<t2;t1++)
					vid_usage[t1+x+y*80]=set_vid_usage-1;
				}
			break;
		case DE_NUMBER:
		case DE_FIVE:
			t1=1;
			if(type==DE_FIVE) t1=5;
			write_string(str,x,y,color,segment);
			x+=str_len(str);
			if((p1==1)&&(p2<10)&&(t1==1)) {
				//Number line
				for(t1=1;t1<=p2;t1++) {
					if(t1==((int far *)value)[0]) draw_char(t1+'0',DI_ACTIVE,
						x+t1-1,y,segment);
					else draw_char(t1+'0',DI_NONACTIVE,x+t1-1,y,segment);
					}
				//Fill vid_usage
				if(set_vid_usage) {
					t2=str_len(str);
					x-=t2;
					t2+=p2;
					for(t1=0;t1<t2;t1++)
						vid_usage[t1+x+y*80]=set_vid_usage-1;
					}
				}
			else {
				//Number
				itoa(((int far *)value)[0]*t1,temp,10);
				fill_line(7,x,y,(DI_NUMERIC<<8)+32,segment);
				write_string(temp,x+6-str_len(temp),y,DI_NUMERIC,segment);
				//Buttons
				write_string(num_buttons,x+7,y,DI_ARROWBUTTON,segment);
				//Fill vid_usage
				if(set_vid_usage) {
					t2=str_len(str);
					x-=t2;
					t2+=13;
					for(t1=0;t1<t2;t1++)
						vid_usage[t1+x+y*80]=set_vid_usage-1;
					}
				}
			break;
		case DE_LIST:
			write_string(str,x,y,color,segment);
			//Draw in current choice
			t1=p2*(((int far *)value)[0]+1);
			fill_line(p2+1,x,y+1,(DI_LIST<<8)+32,segment);
			color_string(&str[t1],x+1,y+1,DI_LIST,segment);
			//Draw button
			write_string(list_button,x+p2+1,y+1,DI_ARROWBUTTON,
				segment);
			//Fill vid_usage
			if(set_vid_usage) {
				for(t1=0;t1<=p2+3;t1++) {
					vid_usage[t1+x+y*80]=set_vid_usage-1;
					vid_usage[t1+x+(y+1)*80]=set_vid_usage-1;
					}
				}
			break;
		case DE_BOARD:
			write_string(str,x,y,color,segment);
			fill_line(BOARD_NAME_SIZE+1,x,y+1,(DI_LIST<<8)+32,segment);
			if(board_where[((int far *)value)[0]]==W_NOWHERE)
				color_string("(no board)",x+1,y+1,DI_LIST,segment);
			else if((((int far *)value)[0]==0)&&(p1))
				color_string("(none)",x+1,y+1,DI_LIST,segment);
			else color_string(&board_list[((int far *)value)[0]*BOARD_NAME_SIZE],
				x+1,y+1,DI_LIST,segment);
			//Draw button
			write_string(list_button,x+BOARD_NAME_SIZE+1,y+1,DI_ARROWBUTTON,
				segment);
			//Fill vid_usage
			if(set_vid_usage) {
				for(t1=0;t1<=BOARD_NAME_SIZE+3;t1++) {
					vid_usage[t1+x+y*80]=set_vid_usage-1;
					vid_usage[t1+x+(y+1)*80]=set_vid_usage-1;
					}
				}
			break;
		}
	//Done!
}

//Shell for list_menu()
int choose_board(int current,char far *title,unsigned int segment,
 char board0_none) {
	int t1,t2=1,old_curr=curr_board,add_board=-1;
	char temp[BOARD_NAME_SIZE];
	//Go through- blank boards get a (no board) marker. t2 keeps track
	//of the number of boards.
	for(t1=0;t1<NUM_BOARDS;t1++) {
		if(board_where[t1]==W_NOWHERE)
			str_cpy(&board_list[t1*BOARD_NAME_SIZE],"(no board)");
		else t2=t1+1;
		}
	if((current<0)||(current>=t2)) current=0;
	//Add (new board) to bottom
	if(t2<NUM_BOARDS) {
		str_cpy(&board_list[t2*BOARD_NAME_SIZE],"(add board)");
		add_board=t2;
		t2++;
		}
	//Change top to (none) if needed
	if(board0_none) {
		str_cpy(temp,board_list);
		str_cpy(board_list,"(none)");
		}
	//Run the list_menu()
	current=list_menu(board_list,BOARD_NAME_SIZE,title,current,t2,segment);
	//Fix board list
	for(t1=0;t1<NUM_BOARDS;t1++)
		if(board_where[t1]==W_NOWHERE)
			board_list[t1*BOARD_NAME_SIZE]=0;
	if(board0_none) str_cpy(board_list,temp);
	//New board? (if select no board or add board)
	if((board_where[current]==W_NOWHERE)&&(current>=0)) {
		//Yepper! Save current board...
		t1=curr_board;
		//Find the first available...
		if(t2==add_board) {
			for(t2=0;t2<NUM_BOARDS;t2++)
				if(board_where[t2]==W_NOWHERE) break;
			}
		else t2=current;//Use the board selected
		//..get a name...
		m_hide();
		save_screen(segment);
		draw_window_box(16,12,64,14,segment,79,64,70);
		write_string("Name for new board:",18,13,78,segment);
		board_list[t2*BOARD_NAME_SIZE]=0;
		m_show();
		if(intake(&board_list[t2*BOARD_NAME_SIZE],BOARD_NAME_SIZE-1,38,13,
			segment,15,1,0)==27) {
				restore_screen(segment);
				return -1;
				}
		restore_screen(segment);
		//...make a new board and return to current board
		store_current();
		clear_current_and_select(t2);
		board_where[curr_board=t2]=W_CURRENT;
		board_sizes[t2]=0;
		swap_with(old_curr);
		return t2;
		}
	//Return board number
	return current;
}

char cd_types[2]={ DE_BUTTON,DE_BUTTON };
char cd_xs[2]={ 15,37 };
char cd_ys[2]={ 2,2 };
char far *cd_strs[2]={ "OK","Cancel" };
int cd_p1s[2]={ 0,1 };
dialog c_di={ 10,9,69,13,NULL,2,cd_types,cd_xs,cd_ys,cd_strs,cd_p1s,NULL,
	NULL,0 };
//Shell for run_dialog()
char confirm(char far *str) {
	c_di.title=str;
	return run_dialog(&c_di,current_pg_seg);
}

char yn_types[2]={ DE_BUTTON,DE_BUTTON };
char yn_xs[2]={ 15,37 };
char yn_ys[2]={ 2,2 };
char far *yn_strs[2]={ "Yes","No" };
int yn_p1s[2]={ 0,1 };
dialog yn_di={ 10,9,69,13,NULL,2,yn_types,yn_xs,yn_ys,yn_strs,yn_p1s,NULL,
	NULL,0 };
//Shell for run_dialog()
char ask_yes_no(char far *str) {
	yn_di.title=str;
	return run_dialog(&yn_di,current_pg_seg);
}

//Sort function for below use in qsort
int cdecl sort_function( const void *a, const void *b) {
	//If one has a space first and the other doesn't, and the other
	//doesn't have a 'ú' first, the other comes first.
	if((((char *)a)[0]==32)&&(((char *)b)[0]!=32)&&
		(((char *)b)[0]!='ú')) return 1;
	if((((char *)b)[0]==32)&&(((char *)a)[0]!=32)&&
		(((char *)a)[0]!='ú')) return -1;
	return strcmp((char *)a,(char *)b);
}

//Shell for list_menu() (copies file chosen to ret and returns -1 for ESC)
//dirs_okay of 1 means drive and directory changing is allowed.
//Use NULL for wildcards to mean "all module files"
#define NUM_MOD_TYPES 5
#define MAX_FILES 1638
char *mod_types[NUM_MOD_TYPES]={
	"*.MOD","*.NST","*.GDM","*.WOW","*.OCT" };
char choose_file(char far *wildcards,char far *ret,char far *title,
 char dirs_okay) {
	char prevdir[PATHNAME_SIZE],prevdrive;

	long max_files;
	struct ffblk ff;
	int num=0,t1,t2,t3,curr_mod_type=-1;
	char use_titles=0;
	/* Room for filename AND title */
	char far *list;
	FILE *fp;

	prevdrive=getdisk();
	getcwd(prevdir,PATHNAME_SIZE);

	//Allocate array
	if(wildcards==NULL) free_sam_cache(1);//Okay in editor
	max_files=farcoreleft()/40;
	if(max_files>MAX_FILES) max_files=MAX_FILES;
	if(max_files>100) list=(char far *)farmalloc(40*max_files);
	else list=NULL;

	if(list==NULL) {/* Attempt to grab some memory */
		free_sam_cache(1);
		max_files=farcoreleft()/40;
		if(max_files>MAX_FILES) max_files=MAX_FILES;
		if(max_files>100) list=(char far *)farmalloc(40*max_files);
		else list=NULL;
		if(list==NULL) {/* Free up sample mem */
			free_up_board_memory();
			max_files=farcoreleft()/40;
			if(max_files>MAX_FILES) max_files=MAX_FILES;
			if(max_files>100) list=(char far *)farmalloc(40*max_files);
			else list=NULL;
			if(list==NULL) {/* Clear module.. :( */
				end_mod();
				max_files=farcoreleft()/40;
				if(max_files>MAX_FILES) max_files=MAX_FILES;
				if(max_files>100) list=(char far *)farmalloc(40*max_files);
				else list=NULL;
				if(list==NULL)//Out of mem
					error("Out of memory and/or disk space",2,4,
						current_pg_seg,0x203);
				}
			}
		}
re_search:
	if(_fstricmp(wildcards,"*.MZX")==0) use_titles=1;
	if(wildcards==NULL) wildcards=mod_types[curr_mod_type=0];
	if(findfirst(wildcards,&ff,0)==-1) goto call_fc;
	do {
		str_cpy(&list[40*num],"              ");
		str_cpy(&list[40*num],ff.ff_name);/* Save filename, update num */
		list[40*num+39]=str_len(ff.ff_name);// Save size for copying later
		if(use_titles==1) {
			fp=fopen(ff.ff_name,"rb");/* Open file to read name */
			if(fp!=NULL) {
				list[(40*num)+str_len(ff.ff_name)]=' ';
				fread(&list[(40*num)+14],1,24,fp);/* Read title ! */
				list[(40*num)+38]=0;
				fclose(fp);
				}
			}
		num++;
		if(findnext(&ff)==-1) break;
		if(num==max_files) goto file_lim;/* Limit on files because of memory... :( */
	} while(1);
call_fc:
	//Next mod type?
	if((curr_mod_type>-1)&&(curr_mod_type<14)) {
		wildcards=mod_types[++curr_mod_type];
		goto re_search;
		}
	//Find directorys too, if wanted
if(dirs_okay) {
	if(findfirst("*.*",&ff,FA_DIREC)==-1) goto call_fc2;
	do {
		if(!(ff.ff_attrib&FA_DIREC)) goto not_direc;
		if(!_fstricmp(ff.ff_name,".")) goto not_direc;
		str_cpy(&list[40*num],"              [subdirectory]");
		str_cpy(&list[40*num+1],ff.ff_name);/* Save filename, update num */
		list[40*num+1+str_len(ff.ff_name)]=' ';
		list[40*num+39]=str_len(ff.ff_name);
		num++;
	not_direc:
		if(findnext(&ff)==-1) break;
		if(num==max_files) goto file_lim;/* Limit on files because of memory... :( */
	} while(1);
call_fc2:
	//Add drives if room
	t1=setdisk(t3=getdisk());
	//t1=# drives
	for(t2=0;t2<t1;t2++) {
		if(num==max_files) goto file_lim;
		setdisk(t2);
		if(getdisk()!=t2) continue;
		str_cpy(&list[40*num],"úA:           [drive]");
		list[40*num+1]+=t2;
		num++;
		}
	setdisk(t3);
	}
file_lim:
	if(num==0) {
		farfree(list);
		error("No files found to select from",0,24,current_pg_seg,0x1701);
		return -1;//No files to choose from!
		}
	//Sort
	//(directorys automatically at bottom because of starting dot)
	qsort((void *)list,num,40,sort_function);
	num=list_menu(list,40,title,0,num,current_pg_seg,15);
	if(num<0) {
		setdisk(prevdrive);
		chdir(prevdir);
		farfree(list);
		return -1;/* ESC or exit. */
		}
	//Directory or drive
	if(list[40*num]=='ú') {
		setdisk(list[40*num+1]-'A');
		num=0;
		use_titles=0;
		goto re_search;
		}
	else if(list[40*num]==' ') {
		list[40*num+1+list[40*num+39]]=0;
		chdir(&list[40*num+1]);
		num=0;
		use_titles=0;
		goto re_search;
		}
	mem_cpy(ret,&list[40*num],list[40*num+39]);
	ret[list[40*num+39]]=0;
	farfree(list);
	return 0;
}