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

//Character editor

#include "helpsys.h"
#include "mouse.h"
#include "beep.h"
#include "egacode.h"
#include "char_ed.h"
#include "window.h"
#include "graphics.h"
#include "getkey.h"
#include "hexchar.h"
#include "data.h"
#include "charset.h"

//-------------------------------------------------
//     +----------------+ Current char-     (#000)
// 000 |................|    ..........0..........
// 000 |................| %%      Move cursor
// 000 |................| -+      Change char
// 000 |................| Space   Toggle pixel
// 000 |................| Enter   Select char
// 000 |................| Del     Clear char
// 000 |................| N       'Negative'
// 000 |................| Alt+%%  Shift char
// 000 |................| M       'Mirror'
// 000 |................| F       'Flip'
// 000 |................| F2      Copy to buffer
// 000 |................| F3      Copy from buffer
// 000 |................| Tab     Draw (off)
// 000 |................| F4      Revert to Ascii
//     +----------------+ F5      Revert to MZX
//-------------------------------------------------

//Mouse- Menu=Command
//       Grid=Toggle
//       "Current Char-"=Enter
//       "..0.."=Change char

int ce_menu_cmds[14]={ 0,0,' ',13,-83,'N',0,'M','F',-60,-61,9,-62,-63 };

int char_editor(void) {
	unsigned char matrix[14];
	static unsigned char buffer[14];
	char x=3,y=6,draw=0,bit;
	int t1,t2,t3,t4,key;
	static unsigned char chr=1;
	set_context(79);
	m_hide();
	save_screen(current_pg_seg);
	draw_window_box(15,3,65,20,current_pg_seg,143,128,135);
	//Draw in static elements
	draw_window_box(21,4,38,19,current_pg_seg,128,143,135,0,0);
	write_string("Current char-(#000)\n\n\
 Move cursor\n\
-+ Change char\n\
Space   Toggle pixel\n\
Enter   Select char\n\
DelClear char\n\
N  'Negative'\n\
Alt+  Shift char\n\
M  'Mirror'\n\
F  'Flip'\n\
F2 Copy to buffer\n\
F3 Copy from buffer\n\
TabDraw\n\
F4 Revert to Ascii\n\
F5 Revert to MZX",40,4,143,current_pg_seg);
	m_show();
	//Get char
	ec_read_char(chr,matrix);
	do {
		//Update char
		m_hide();
		for(t1=0;t1<8;t1++) {
			for(t2=0;t2<14;t2++) {
				bit=matrix[t2]&(128>>t1);
				if(bit) write_string("€€",22+(t1<<1),5+t2,135,
					current_pg_seg);
				else write_string("˙˙",22+(t1<<1),5+t2,135,current_pg_seg);
				}
			}
		for(t2=0;t2<14;t2++)
			write_number(matrix[t2],135,17,5+t2,current_pg_seg,3);
		//Update draw status
		if(draw==1) write_string("(set)   ",53,17,143,current_pg_seg);
		else if(draw==2) write_string("(clear) ",53,17,143,current_pg_seg);
		else if(draw==3) write_string("(toggle)",53,17,143,current_pg_seg);
		else write_string("(off)   ",53,17,135,current_pg_seg);
		//Highlight cursor
		color_line(2,22+(x<<1),5+y,27,current_pg_seg);
		//Current character
		write_number(chr,143,60,4,current_pg_seg,3);
		for(t1=-10;t1<11;t1++)
			draw_char((unsigned char)(chr+t1),128+(t1==0)*15,53+t1,5,
				current_pg_seg);
		//Key
		m_show();
		key=getkey();
		if((key>='a')&&(key<='z')) key-=32;
	re_evaul_key:
		switch(key) {
			case MOUSE_EVENT:
				//Mouse- check area
				if((mouse_event.cx>21)&&(mouse_event.cx<38)&&
					(mouse_event.cy>4)&&(mouse_event.cy<19)) {
						//Grid.
						x=(mouse_event.cx-22)>>1;
						y=mouse_event.cy-5;
						goto place;
					}
				if((mouse_event.cx>38)&&(mouse_event.cx<64)) {
					if((mouse_event.cy>5)&&(mouse_event.cy<20)) {
						//Menu.
						key=ce_menu_cmds[mouse_event.cy-6];
						goto re_evaul_key;
						}
					else if(mouse_event.cy==4) //Choose char.
						goto select_char;
					else if(mouse_event.cy==5) {
						//Change char
						if(mouse_event.cx<43) {
							beep();//Too far left
							break;
							}
						//Become...
						chr+=mouse_event.cx-53;
						ec_read_char(chr,matrix);
						break;
						}
					}
			default:
				//Bad mouse/key
				beep();
				break;
			case -75://Left
				if((--x)<0) x=7;
				if(draw==1) goto set;
				if(draw==2) goto reset;
				if(draw==3) goto place;
				break;
			case -77://Right
				if((++x)>7) x=0;
				if(draw==1) goto set;
				if(draw==2) goto reset;
				if(draw==3) goto place;
				break;
			case -72://Up
				if((--y)<0) y=13;
				if(draw==1) goto set;
				if(draw==2) goto reset;
				if(draw==3) goto place;
				break;
			case -80://Down
				if((++y)>13) y=0;
				if(draw==1) goto set;
				if(draw==2) goto reset;
				if(draw==3) goto place;
				break;
			case '-':
			case '_':
				chr--;
				ec_read_char(chr,matrix);
				break;
			case '+':
			case '=':
				chr++;
				ec_read_char(chr,matrix);
				break;
			case ' ':
			place:
				matrix[y]^=(128>>x);
				ec_change_char(chr,matrix);
				break;
			set:
				matrix[y]|=(128>>x);
				ec_change_char(chr,matrix);
				break;
			reset:
				matrix[y]&=~(128>>x);
				ec_change_char(chr,matrix);
				break;
			case 13:
			select_char:
				t1=char_selection(chr,current_pg_seg);
				if(t1>=0) chr=t1;
				ec_read_char(chr,matrix);
				break;
			case 'Q':
				key=27;
				break;
			case -83://Delete
				for(t1=0;t1<14;t1++) matrix[t1]=0;
				ec_change_char(chr,matrix);
				break;
			case -60://F2
				for(t1=0;t1<14;t1++) buffer[t1]=matrix[t1];
				break;
			case -61://F3
				for(t1=0;t1<14;t1++) matrix[t1]=buffer[t1];
				ec_change_char(chr,matrix);
				break;
			case 'M':
				for(t1=0;t1<14;t1++) {
					t3=128;
					t4=1;
					for(t2=0;t2<4;t2++) {
						if((matrix[t1]&t3)&&(matrix[t1]&t4)) ;/* Both on */
						else if((matrix[t1]&t3)||(matrix[t1]&t4)) matrix[t1]^=t3+t4;
						t3>>=1;
						t4<<=1;
						}
					}
				ec_change_char(chr,matrix);
				break;
			case 'F':
				for(t1=0;t1<7;t1++) {
					t2=matrix[t1];
					matrix[t1]=matrix[13-t1];
					matrix[13-t1]=t2;
					}
				ec_change_char(chr,matrix);
				break;
			case 9://Tab
				if((++draw)>3) draw=0;
				if(draw==1) goto set;
				if(draw==2) goto reset;
				if(draw==3) goto place;
				break;
			case 'N':
				for(t1=0;t1<14;t1++)
					matrix[t1]^=255;
				ec_change_char(chr,matrix);
				break;
			case -62:
				//Revert to ascii
				for(t1=0;t1<14;t1++)
					matrix[t1]=ascii_set[chr*14+t1];
				ec_change_char(chr,matrix);
				break;
			case -63:
				//Revert to megazeux
				for(t1=0;t1<14;t1++)
					matrix[t1]=default_mzx_char_set[chr*14+t1];
				ec_change_char(chr,matrix);
				break;
			case -152:/* Alt Up */
				t1=matrix[0];
				for(t2=0;t2<13;t2++) matrix[t2]=matrix[t2+1];
				matrix[13]=t1;
				ec_change_char(chr,matrix);
				break;
			case -160:/* Alt Down */
				t1=matrix[13];
				for(t2=14;t2>0;t2--) matrix[t2]=matrix[t2-1];
				matrix[0]=t1;
				ec_change_char(chr,matrix);
				break;
			case -155:/* Alt Left */
				for(t1=0;t1<14;t1++) {
					t2=matrix[t1]&128;
					matrix[t1]<<=1;
					if(t2) matrix[t1]|=1;
					}
				ec_change_char(chr,matrix);
				break;
			case -157:/* Alt Right */
				for(t1=0;t1<14;t1++) {
					t2=matrix[t1]&1;
					matrix[t1]>>=1;
					if(t2) matrix[t1]|=128;
					}
				ec_change_char(chr,matrix);
			case 27:
			case 0:
				break;
			}
	} while(key!=27);
	restore_screen(current_pg_seg);
	pop_context();
	return chr;
}