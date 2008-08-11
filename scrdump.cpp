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

/* This will dump to a bmp, because I didn't feel like messing with PCX, and Greg's code
 is terrible. SMZX is pretty terrible for RLE, as it is. - Exo */
  
void smzx_dump_screen()
{
	FILE *fp;
  unsigned int i, i3, i4, temp_short;
  signed int i2, i5;
  long int temp_long;
	unsigned char *screen = (unsigned char *)MK_FP(current_pg_seg, 0);
  // Remember, start the screen at the LAST row.
  unsigned char *screen_edge = screen + ((80 * 24) * 2);
  unsigned char current_char, current_color, current_row;
  unsigned char cc[4];
  unsigned char r, g, b;
  char fname[11];

	for(i = 0; i < 100; i++) 
  {
	  sprintf(fname, "smzx%d.bmp", i);
	  if(!(access(fname, 0) == 0)) 
    {
      break;
    }
	}

	fp = fopen(fname,"wb");

  // BMP header
  fputc('B', fp);                              // B
  fputc('M', fp);                              // M
  temp_long = 113078l;                         // File size of the BMP
  fwrite(&temp_long, 1, 4, fp);
  temp_long = 0;                               // Reserved bytes
  fwrite(&temp_long, 1, 4, fp);
  temp_long = 1078;                            // Offset to the image information
  fwrite(&temp_long, 1, 4, fp);
  
  // BMP info header
  temp_long = 40;                              // Size of the info header
  fwrite(&temp_long, 1, 4, fp);
  temp_long = 320;                             // Image width
  fwrite(&temp_long, 1, 4, fp);
  temp_long = 350;                             // Image height
  fwrite(&temp_long, 1, 4, fp);
  temp_short = 1;                              // Planes
  fwrite(&temp_short, 1, 2, fp);               
  temp_short = 8;                              // Color depth
  fwrite(&temp_short, 1, 2, fp);
  temp_long = 0;                               // Compression
  fwrite(&temp_long, 1, 4, fp);
  temp_long = 112000l;                         // Image size
  fwrite(&temp_long, 1, 4, fp);
  temp_long = 0;                               // Meter width
  fwrite(&temp_long, 1, 4, fp);
  temp_long = 0;                               // Meter height
  fwrite(&temp_long, 1, 4, fp);
  temp_long = 0;                               // Colors used
  fwrite(&temp_long, 1, 4, fp);
  temp_long = 0;                               // Important colors
  fwrite(&temp_long, 1, 4, fp);

  // Write the palette. In mode 1 it can be calculated, in mode 2 it should
  // be taken from the smzx_mode2_palette array.
  // This is actually a "color table", meaning it has a quad entered, and its
  // in BGR order. Oh boy.
  if(smzx_mode == 1)
  {
    if(ati_fix == 1)
    {
      for(i = 0; i < 256; i++)
      {
        i2 = (i >> 4) | ((i & 0xF) << 4);
        // Blue
        b = ((((intensity_pal[i2 & 15][2] << 1) + intensity_pal[i2 >> 4][2]) / 3) * 255) / 63;
        // Green
        g = ((((intensity_pal[i2 & 15][1] << 1) + intensity_pal[i2 >> 4][1]) / 3) * 255) / 63;
        // Red
        r = ((((intensity_pal[i2 & 15][0] << 1) + intensity_pal[i2 >> 4][0]) / 3) * 255) / 63;
        fputc(b, fp);
        fputc(g, fp);
        fputc(r, fp);
        fputc(0, fp);
      }
    }
    else
    {    
      for(i = 0; i < 256; i++)
      {
        // Blue
        b = ((((intensity_pal[i & 15][2] << 1) + intensity_pal[i >> 4][2]) / 3) * 255) / 63;
        // Green
        g = ((((intensity_pal[i & 15][1] << 1) + intensity_pal[i >> 4][1]) / 3) * 255) / 63;
        // Red
        r = ((((intensity_pal[i & 15][0] << 1) + intensity_pal[i >> 4][0]) / 3) * 255) / 63;
        fputc(b, fp);
        fputc(g, fp);
        fputc(r, fp);
        fputc(0, fp);
      }
    }
  }
  else
  {
    for(i = 0; i < 768; i += 3)
    {
      // Blue
      b = (smzx_mode2_palette[i + 2] * 255) / 63;
      // Green
      g = (smzx_mode2_palette[i + 1] * 255) / 63;
      // Red
      r = (smzx_mode2_palette[i] * 255) / 63;

      fputc(b, fp);
      fputc(g, fp);
      fputc(r, fp);
      fputc(0, fp);
    }
  }

  // Now the fun begins... let's all remember that BMP's are stored upside
  // down this time.

  // Go through each row, then go through each subrow, for each char.  
  for(i = 0; i < 25; i++)
  {
    // Go backwards. Start at 13, and work to 0.
    for(i2 = 13; i2 >= 0; i2--)
    {
      for(i3 = 0; i3 < 80; i3++)
      {
        // Grab the current char
        current_char = *screen;
        // Grab the current color
        current_color = *(screen + 1);
        // Spit out the color combinations, from the palette
        // For color 0xAB, the first combination is 0xAA
        cc[0] = (current_color & 0xF0) | (current_color >> 4);
        // The second is 0xAB
        cc[1] = current_color;
        // The third is 0xBA
        cc[2] = (current_color >> 4) | ((current_color & 0xF) << 4);
        // And the fourth is 0xBB
        cc[3] = (current_color & 0xF) | ((current_color & 0xF) << 4);
        // Now, grab the row from the char bitmap
        current_row = ec_read_byte(current_char, i2);
        // Draw the four colors, masking out 1 at a time (2 bits)
        // i5 determines how many to shift down the masked out color
        for(i4 = 0xC0, i5 = 6; i5 >= 0; i4 >>= 2, i5 -= 2)
        {
          // Write the byte determined by the color array
          fputc(cc[(current_row & i4) >> i5], fp);
        }
        // Move on to the next char/color combo
        screen += 2;
      }
      // Back up to the last row
      screen = screen_edge;
    }
    // Move on to the previous edge (backtracking)
    screen -= 160;
    screen_edge = screen;
  }

  // All done!
  fclose(fp);
}

/* FIXME: not endian-safe */
void dump_screen() {
  if(smzx_mode)
  {
    smzx_dump_screen();
    return;
  }
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
    // Have it put the current intensity instead. - Exo
		//get_rgb(t1,r,g,b);
    r = intensity_pal[t1][0];
    g = intensity_pal[t1][1];
    b = intensity_pal[t1][2];
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
