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

/* Palette editor */

#include "timer.h"
#include "helpsys.h"
#include "palette.h"
#include "pal_ed.h"
#include "window.h"
#include "getkey.h"
#include "data.h"
#include "graphics.h"
#include "beep.h"
#include "hexchar.h"

//---------------------------------------------
//
// ##..##..##..##..##..##..##..##.. Color #00-
// ##..##..##..##..##..#1.1#1.1#1.1  Red 00/63
// #0.1#2.3#4.5#6.7#8.9#0.1#2.3#4.5  Grn 00/63
// ##..##..##..##..##..##..##..##..  Blu 00/63
// ^^
//
// %- Select color   Alt+D- Default palette
// R- Increase Red   Alt+R- Decrease Red
// G- Increase Grn   Alt+G- Decrease Grn
// B- Increase Blu   Alt+B- Decrease Blu
// A- Increase All   Alt+A- Decrease All
// 0- Blacken color     F2- Store color
// Q- Quit editing      F3- Retrieve color
//
//---------------------------------------------

//Mouse- Color=Change color
//       Menu=Do command
//       RGB #'s=Raise/Lower R/G/B

//2 columns 7 rows
int menu_cmds[2][7]={
{ 0,'R','G','B','A','0','Q' },
{ -32,-19,-34,-48,-30,-60,-61 } };

void palette_editor(void) {
	int t1,t2,key,chr,col;
	int curcol=0;
	char r,g,b,mouse_held=0;
	static char sr=-1,sg=-1,sb=-1;
	set_context(93);
	m_hide();
	save_screen(current_pg_seg);
	//Draw window
	draw_window_box(17,3,63,19,current_pg_seg,128,143,135);
	//Draw palette bars
	for(t1=0;t1<32;t1++) {
		for(t2=0;t2<4;t2++) {
			chr=' ';
			if((t1&1)&&(t2==1)&&(t1>19)) chr='1';
			if((t1&1)&&(t2==2)) chr='0'+((t1>>1)%10);
			col=((t1&30)<<3);
			if(t1<20) col+=15;
			draw_char(chr,col,19+t1,5+t2,current_pg_seg);
			}
		}
	//Write rgb info stuff
	write_string("Color #00-\n Red 00/63\n Grn 00/63\n Blu 00/63",
		52,5,143,current_pg_seg);
	//Write menu
	write_string("- Select color   Alt+D- Default palette\n\
R- Increase Red   Alt+R- Decrease Red\n\
G- Increase Grn   Alt+G- Decrease Grn\n\
B- Increase Blu   Alt+B- Decrease Blu\n\
A- Increase All   Alt+A- Decrease All\n\
0- Blacken colorF2- Store color\n\
Q- Quit editing F3- Retrieve color",19,11,143,current_pg_seg);
	m_show();
	do {
		//Write rgb and color #
		get_rgb(curcol,r,g,b);
		m_hide();
		write_number(curcol,143,59,5,current_pg_seg,2);
		write_number(r,143,57,6,current_pg_seg,2);
		write_number(g,143,57,7,current_pg_seg,2);
		write_number(b,143,57,8,current_pg_seg,2);
		//Draw '^^', get key, erase '^^'
		write_string("",19+(curcol<<1),9,143,current_pg_seg);
		m_show();
		key=getkey();
		if((key>='a')&&(key<='z')) key-=32;
		m_hide();
		write_string("  ",19+(curcol<<1),9,143,current_pg_seg);
		m_show();
		//Process
	re_evaul_key:
		switch(key) {
			case MOUSE_EVENT:
				//Menu?
				if((mouse_event.cx>18)&&(mouse_event.cx<62)&&
					(mouse_event.cy>10)&&(mouse_event.cy<18)) {
						if(mouse_event.cx<36) key=menu_cmds[0][mouse_event.cy-11];
						else key=menu_cmds[1][mouse_event.cy-11];
						goto re_evaul_key;
					}
				//Color?
				if((mouse_event.cx>18)&&(mouse_event.cx<51)&&
					(mouse_event.cy>4)&&(mouse_event.cy<9)) {
						curcol=(mouse_event.cx-19)>>1;
						mouse_held=2;
						break;
					}
				//R/G/B +/-?
				if((mouse_event.cx>52)&&(mouse_event.cx<62)&&
					(mouse_event.cy>5)&&(mouse_event.cy<9)) {
						//Determine plus/minus (by 21!)
						if(mouse_event.cx<59) {
							//Minus
							if(mouse_event.cy==6) {
								r-=21;
								if(r<0) r=0;
								}
							else if(mouse_event.cy==7) {
								g-=21;
								if(g<0) g=0;
								}
							else {
								b-=21;
								if(b<0) b=0;
								}
							}
						else {
							//Plus
							if(mouse_event.cy==6) {
								r+=21;
								if(r>63) r=63;
								}
							else if(mouse_event.cy==7) {
								g+=21;
								if(g>63) g=63;
								}
							else {
								b+=21;
								if(b>63) b=63;
								}
							}
						set_rgb(curcol,r,g,b);
						update_palette();
						mouse_held=2;
						break;
					}
				//Nothing
			default:
				beep();
				mouse_held=2;
				break;
			case -75:
			case '-':
			case '_':
				if(curcol>0) curcol--;
				break;
			case -77:
			case '=':
			case '+':
				if(curcol<15) curcol++;
				break;
			case 'Q':
				key=27;
				mouse_held=2;
				break;
			case 'R':
				if(r<63) {
					r++;
					set_rgb(curcol,r,g,b);
					update_palette();
					}
				break;
			case 'G':
				if(g<63) {
					g++;
					set_rgb(curcol,r,g,b);
					update_palette();
					}
				break;
			case 'B':
				if(b<63) {
					b++;
					set_rgb(curcol,r,g,b);
					update_palette();
					}
				break;
			case 'A':
				if(r<63) r++;
				if(g<63) g++;
				if(b<63) b++;
				set_rgb(curcol,r,g,b);
				update_palette();
				break;
			case -19://AltR
				if(r>0) {
					r--;
					set_rgb(curcol,r,g,b);
					update_palette();
					}
				break;
			case -34://AltG
				if(g>0) {
					g--;
					set_rgb(curcol,r,g,b);
					update_palette();
					}
				break;
			case -48://AltB
				if(b>0) {
					b--;
					set_rgb(curcol,r,g,b);
					update_palette();
					}
				break;
			case -30://AltA
				if(r>0) r--;
				if(g>0) g--;
				if(b>0) b--;
				set_rgb(curcol,r,g,b);
				update_palette();
				break;
			case '0':
				r=g=b=0;
				set_rgb(curcol,r,g,b);
				update_palette();
				mouse_held=2;
				break;
			case -32://AltD
				default_palette();
				mouse_held=2;
				break;
			case -60://F2
				sr=r; sg=g; sb=b;
				mouse_held=2;
				break;
			case -61://F3
				if(sr==-1) break;
				r=sr; g=sg; b=sb;
				set_rgb(curcol,r,g,b);
				update_palette();
			case 27:
			case 0:
				mouse_held=2;
				break;
			}
		if(mouse_held==2) mouse_held=0;//Prevent multiples of dumb cmds
		else if(m_buttonstatus()&LEFTBDOWN) {
			//Holding down button
			//Update RGB codes
			write_number(curcol,143,59,5,current_pg_seg,2);
			write_number(r,143,57,6,current_pg_seg,2);
			write_number(g,143,57,7,current_pg_seg,2);
			write_number(b,143,57,8,current_pg_seg,2);
			if(mouse_held==0) {
				delay_time(100);
				if(m_buttonstatus()&LEFTBDOWN) mouse_held=1;
				}
			else {
				delay_time(5);
				if(!m_buttonstatus()&LEFTBDOWN) mouse_held=0;
				}
			}
		else mouse_held=0;
		if(mouse_held) goto re_evaul_key;//Repeat last key;
	} while(key!=27);
	restore_screen(current_pg_seg);
	pop_context();
}