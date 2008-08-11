/* $Id: scrdump.c,v 1.4 1999/02/16 05:45:41 mental Exp $
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <io.h>
#include <dos.h>
#include "const.h"
#include "data.h"
#include "egacode.h"
#include "palette.h"
#include "scrdump.h"

//static volatile char rcsid[] = "$Id: scrdump.c,v 1.4 1999/02/16 05:45:41 mental Exp $";

/* FIXME: not endian-safe */
void dump_screen() {
	char fname[11];
	FILE *fp;
	int t1, t2, t3, t4, t5, t7, t8, snum;
	unsigned char *scrn = (unsigned char*)MK_FP(current_pg_seg,0);
	char r,g,b;
	for (snum = 0;snum <= 99;snum++) {
	  sprintf(fname,"screen%d.pcx",snum);
	  //break;
	  if (!(access(fname,0) == 0)) break;
	}
	fp=fopen(fname,"wb");
	//header
	fputc(10,fp);
	fputc(5,fp);
	fputc(1,fp);
	fputc(1,fp);
	t1=0; fwrite(&t1,1,2,fp);
	t1=0; fwrite(&t1,1,2,fp);
	t1=639; fwrite(&t1,1,2,fp);
	t1=349; fwrite(&t1,1,2,fp);
	t1=300; fwrite(&t1,1,2,fp);
	t1=300; fwrite(&t1,1,2,fp);
	//colormap
	for(t1=0;t1<16;t1++) {
		get_rgb(t1,r,g,b);
		fputc(r<<2,fp);
		fputc(g<<2,fp);
		fputc(b<<2,fp);
	}
	//header part 2
	fputc(0,fp);
	fputc(4,fp);
	t1=80; fwrite(&t1,1,2,fp);
	t1=1; fwrite(&t1,1,2,fp);
	t1=640; fwrite(&t1,1,2,fp);
	t1=350; fwrite(&t1,1,2,fp);
	for(t1=0;t1<54;t1++) fputc(0,fp);
	//picture
	for(t1=0;t1<25;t1++) {//25 rows
		for(t5=0;t5<14;t5++) {//14 bitmap rows per char
			t7=1;
			for(t2=0;t2<4;t2++) {//4 planes
				for(t3=0;t3<80;t3++) {//80 columns
					//Check FG color to see if applicable
					t4=(scrn[((t1*80+t3)<<1)+1])&t7;
					if(t4) {
						//Get character
						t4=scrn[(t1*80+t3)<<1];
						//Write character bitmap byte
						t8=ec_read_byte(t4,t5);
					}
					else t8=0;//Byte for this bitplane
					//Check BK color to see if applicable
					t4=((scrn[((t1*80+t3)<<1)+1])>>4)&t7;
					if(t4) {
						//Get character
						t4=scrn[(t1*80+t3)<<1];
						//Write inverse of character bitmap byte
						t8|=255^ec_read_byte(t4,t5);
					}
					//Write byte
					if(t8>191) fputc(193,fp);
					fputc(t8,fp);
				}
				t7<<=1;//Upgrade bit plane
			}
		}
	}
	fclose(fp);
}
