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

// A function to display onscreen a percentage meter for progress. No
// screen saving is done.

#include "mouse.h"
#include "graphics.h"
#include "window.h"
#include "meter.h"
#include "string.h"
#include <dos.h>

//Calculates the percent from progress and out_of as in (progress/out_of).
void meter(char far *title,unsigned int segment,int progress,int out_of) {
	int titlex=40-(str_len(title)>>1);
	m_hide();
	draw_window_box(5,10,74,12,segment,DI_MAIN,DI_DARK,DI_CORNER);
	//Add title
	write_string(title,titlex,10,DI_TITLE,segment);
	draw_char(' ',DI_TITLE,titlex-1,10,segment);
	draw_char(' ',DI_TITLE,titlex+str_len(title),10,segment);
	meter_interior(segment,progress,out_of);
	m_show();
}

//Draws the meter but only the interior where the percent is. Use for
//speed and no flicker.
void meter_interior(unsigned int segment,int progress,int out_of) {
	//The actual meter has 66 spaces, or 132 half spaces, to use, so...
	int percent=(int)(((long)progress*132L)/(long)out_of);
	//...percent is the number of half spaces to display.
	int revcolor=((DI_METER&15)<<4)+(DI_METER>>4);
	char percentstr[5]="   %";
	//Pointer to % display on video (IE the ###%)
	char far *video=(char far *)MK_FP(segment,(37+11*80)*2);
	int t1;
	//Fill in meter
	m_hide();
	if(percent>1) {
		fill_line(percent>>1,7,11,32+(revcolor<<8),segment);
		}
	if(percent<131) {
		fill_line((133-percent)>>1,7+(percent>>1),11,32+(DI_METER<<8),segment);
		}
	if(percent&1) {
		draw_char('Þ',revcolor,7+(percent>>1),11,segment);
		}
	//Determine percentage
	percent=(int)(((long)progress*100L)/(long)out_of);
	percentstr[2]='0'+percent%10;
	percent/=10;
	if(percent>0) {
		percentstr[1]='0'+percent%10;
		if(percent>9) percentstr[0]='1';
		}
	//Draw percentage, directly w/o color changes
	for(t1=0;t1<4;t1++) {
		if(percentstr[t1]!=' ') video[t1<<1]=percentstr[t1];
		}
	m_show();
	//Done! :)
}