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

//Palette code. Works on both the EGA and the VGA, although fading and
//intensity percentage resolution is much less on the EGA, and rgb values
//are rounded to 6 bit when output to the graphics card. (VGA rgb values
//are 8 bit)

#include <stdio.h>
#include "palette.h"
#include "egacode.h"
#include "mouse.h"
#include "cursor.h"
#include "blink.h"
#include "retrace.h"
#include "comp_chk.h"

char smzx_mode = 0;
char smzx_pal_initialized = 0;
char ati_fix = 0;
char far smzx_mode2_palette[768];

//Current palette w/o intensity adjustments (18 bit)
char current_pal[16][3];
//Current intensity level (percentage) per color
char current_intensity[16];
//Intensity modified palette (updated at all times)
char intensity_pal[16][3];
//Whether we are currently "faded out" from a quick fade function
char faded_out=0;
//Saved intensities while faded out
char saved_intensity[16];

//Default EGA hardware palette. Also used for VGA palette refrencing.
char far default_EGA_hardware_pal[16]={
	0,1,2,3,4,5,20,7,56,57,58,59,60,61,62,63 };
//Default palette rgbs
char default_pal[16][3]={
	{ 00,00,00 },
	{ 00,00,42 },
	{ 00,42,00 },
	{ 00,42,42 },
	{ 42,00,00 },
	{ 42,00,42 },
	{ 42,21,00 },
	{ 42,42,42 },
	{ 21,21,21 },
	{ 21,21,63 },
	{ 21,63,21 },
	{ 21,63,63 },
	{ 63,21,21 },
	{ 63,21,63 },
	{ 63,63,21 },
	{ 63,63,63 } };

//Set one EGA palette register. Blanks screen. Sets attr, 0-15, to
//ccode, 0-63.
void set_ega_register(char attr,char ccode) {
	asm {
		mov dx,03C0h
		mov al,attr
		out dx,al
		mov al,ccode
		out dx,al
		}
}

//Unblank the screen after setting EGA palette registers.
void unblank_screen(void) {
	asm {
		mov dx,03C0h
		mov al,20h
		out dx,al
		out dx,al
		}
}
//Set one VGA DAC palette register.
void set_vga_register(char color,char r,char g,char b) {
	//Change color number to DAC register
	if(smzx_mode==0)
	{
		if(color<0) color=-color;
		else color=default_EGA_hardware_pal[color];
	}
	asm {
		mov dx,03C8h
		mov al,color
		out dx,al
		inc dx
		mov al,r
		out dx,al
		mov al,g
		out dx,al
		mov al,b
		out dx,al
		}
}

/*//Set one VGA DAC palette register.
void get_vga_register(char color, char &r, char &g, char &b) 
{
  // Assume it's going to be in 256 color mode.
  char r2, g2, b2;
	asm {
		mov dx, 03C7h
		mov al, color
    out dx, al
		add dx, 2
		in al, dx
		mov byte ptr r2, al
		in al, dx
		mov byte ptr g2, al
		in al, dx
		mov byte ptr b2, al
  }
  r = r2;
  g = g2;
  b = b2;
}*/

void update_smzx_palette()
{
  int i, i2;
  
  if(smzx_mode == 2)
  {
    for(i = 0, i2 = 0; i < 256; i++, i2 += 3)
    {
      update_smzx_color((unsigned char)i);
    }
  }
}

void update_smzx_color(unsigned char c)
{
  char c2 = c;
  if(ati_fix == 1)
    c2 = (((c & 0xF) << 4) | ((unsigned char)c >> 4));
  int c_off = c * 3;
  set_vga_register(c2, smzx_mode2_palette[c_off],
   smzx_mode2_palette[c_off + 1], smzx_mode2_palette[c_off + 2]);
}

void update_smzx_color_direct(unsigned char c, unsigned char r, 
 unsigned char g, unsigned char b)
{
  char c2 = c;
  if(ati_fix == 1)
    c2 = (((c & 0xF) << 4) | (c >> 4));
  set_vga_register(c2, r, g, b);
}

void load_smzx_palette(char far *fname)
{
  int i;

  FILE *fp = fopen(fname, "rb");
  if(fp != NULL)
  {
    int fs, i, i2;
    fseek(fp, 0, SEEK_END);
    fs = ftell(fp);
    if(fs > 768) fs = 768;
    fseek(fp, 0, SEEK_SET);
    
    for(i = 0, i2 = 0; i2 < fs; i++, i2 += 3)
    {
      smzx_mode2_palette[i2] = fgetc(fp);
      smzx_mode2_palette[i2 + 1] = fgetc(fp);
      smzx_mode2_palette[i2 + 2] = fgetc(fp);    
    }
    update_smzx_palette();
    fclose(fp);  
  }
}

void init_smzx_mode()
{
  m_deinit();
  smzx_14p_mode();

  if(ati_fix == 1)
  {
    asm {
      mov al, 013h
      mov dx, 03C0h
      out dx, al
      mov al, 01h
      out dx, al
    }
  }

  cursor_off();
  blink_off();
  m_init();
  ec_update_set();
  default_EGA_hardware_pal[6] = 6;
  reinit_palette();   
  m_show();

  if(smzx_mode == 2)
  {
    if(!smzx_pal_initialized)
    {
      load_smzx_palette("default.spl");
      smzx_pal_initialized = 1;
    }
    else
    {
      update_smzx_palette();
    }
  }
  else
  {
    smzx_update_palette(0);
  }
}

//Initialize palette- On both EGA and VGA, sets the proper palette
//codes. (IE sets up the default EGA palette) Call before using any
//other code. Also call on exit or anytime to fully reset the palette
//to its defaults.
void init_palette(void) {
	int t1;
	//On both EGA and VGA, we must set all 16 registers to the defaults.
	//Also copy it over to the current palette.

	wait_retrace();
	for(t1=0;t1<16;t1++) {
		set_ega_register(t1,default_EGA_hardware_pal[t1]);
		current_intensity[t1]=saved_intensity[t1]=100;
		if(vga_avail)
			set_vga_register(t1,default_pal[t1][0],default_pal[t1][1],
				default_pal[t1][2]);
		intensity_pal[t1][0]=current_pal[t1][0]=default_pal[t1][0];
		intensity_pal[t1][1]=current_pal[t1][1]=default_pal[t1][1];
		intensity_pal[t1][2]=current_pal[t1][2]=default_pal[t1][2];
		}
	unblank_screen();
	faded_out=0;
	//Palette is now fully at default, and all data is initialized.
}

void reinit_palette(void) {
	int t1;
	//On both EGA and VGA, we must set all 16 registers to the defaults.
	//Also copy it over to the current palette.
	insta_fadeout;
	wait_retrace();
	for(t1=0;t1<16;t1++) {
		set_ega_register(t1,default_EGA_hardware_pal[t1]);
		current_intensity[t1]=saved_intensity[t1]=100;

		set_color_intensity(t1,100);

		if(vga_avail)
			set_vga_register(t1,current_pal[t1][0],current_pal[t1][1],
				current_pal[t1][2]);
		intensity_pal[t1][0]=current_pal[t1][0];
		intensity_pal[t1][1]=current_pal[t1][1];
		intensity_pal[t1][2]=current_pal[t1][2];
		}
	insta_fadein();
	unblank_screen();
	faded_out=0;
	//Palette is now fully at default, and all data is initialized.
}



//Set current palette intensity, internally only.
void set_palette_intensity(char percent) {
	int t1,t2;
	if(percent<0) percent=0;
	if(percent>100) percent=100;
	if(faded_out) {
		//If faded out, save in saved_intensity and exit
		for(t1=0;t1<16;t1++)
			saved_intensity[t1]=percent;
		return;
		}
	//Copy palette to intensity palette, adjusting intensity
	for(t1=0;t1<16;t1++) {
		current_intensity[t1]=percent;
		for(t2=0;t2<3;t2++) {
			intensity_pal[t1][t2]=current_pal[t1][t2]*percent/100;
			}
		}
	//Done
	return;
}

//Set current palette intensity, internally only, but for only one color.
void set_color_intensity(char color,char percent) {
	int t2;
	if(percent<0) percent=0;
	if(percent>100) percent=100;
	if(faded_out) {
		//Put in saved if faded out
		saved_intensity[color]=percent;
		return;
		}
	//Copy palette to intensity palette, adjusting intensity
	current_intensity[color]=percent;
	for(t2=0;t2<3;t2++)
		intensity_pal[color][t2]=current_pal[color][t2]*percent/100;
	//Done
	return;
}

//Set rgb for one color, internally only. Sets current_pal. Also sets
//intensity_pal according to current intensity. R G B can range from
//0 to 63.
void set_rgb(char color,char r,char g,char b) {
	int t2;
	if(r<0) r=0; if(g<0) g=0; if(b<0) b=0;
	if(r>63) r=63; if(g>63) g=63; if(b>63) b=63;
	//Set current pal
	current_pal[color][0]=r;
	current_pal[color][1]=g;
	current_pal[color][2]=b;
	//Copy palette to intensity palette, adjusting intensity
	for(t2=0;t2<3;t2++)
		intensity_pal[color][t2]=current_pal[color][t2]*
			current_intensity[color]/100;
	//Done
	return;
}

//Update palette onscreen. Waits for retrace if specified. (default)
void update_palette(char wait_for_retrace) {
	if(smzx_mode)
	{
		smzx_update_palette(wait_for_retrace);
	}
	else
	{
		normal_update_palette(wait_for_retrace);
	}
}

void normal_update_palette(char wait_for_retrace) 
{
	int t1,t2,r,g,b;

  for(t1 = 0; t1 < 8; t1++)
  {
    default_EGA_hardware_pal[t1] = t1;
    default_EGA_hardware_pal[t1 + 8] = t1 + 56;
  }
  default_EGA_hardware_pal[6] = 20;

	if(faded_out) return;
	//Wait for retrace if applicable
	if(wait_for_retrace) wait_retrace();

	//VGA
	if(vga_avail) {
		for(t1=0;t1<16;t1++)
			set_vga_register(t1,intensity_pal[t1][0],intensity_pal[t1][1],
				intensity_pal[t1][2]);
		//Special code for #0
		 set_vga_register(-6,intensity_pal[0][0],intensity_pal[0][1],
			intensity_pal[0][2]);
		for(t1=8;t1<20;t1++)
			set_vga_register(-t1,intensity_pal[0][0],intensity_pal[0][1],
				intensity_pal[0][2]);
		for(t1=21;t1<56;t1++)
			set_vga_register(-t1,intensity_pal[0][0],intensity_pal[0][1],
				intensity_pal[0][2]);
		//Done
		return;
		}
	/*		//EGA- turn 18 bit to 6 bit
	for(t1=0;t1<16;t1++) {
		t2=0;
		r=intensity_pal[t1][0];
		g=intensity_pal[t1][1];
		b=intensity_pal[t1][2];
		if(r<16) ;
		else if(r<32) t2|=32;
		else if(r<48) t2|=4;
		else t2|=36;
		if(g<16) ;
		else if(g<32) t2|=16;
		else if(g<48) t2|=2;
		else t2|=18;
		if(b<16) ;
		else if(b<32) t2|=8;
		else if(b<48) t2|=1;
		else t2|=9;
		set_ega_register(t1,t2);
		}
	unblank_screen(); */
	//Done
}

void smzx_update_palette(char wait_for_retrace) 
{
  int i;
  
  if(faded_out)return;
  // Wait for retrace if applicable
  if(wait_for_retrace) wait_retrace();

  // Only update the diagonals if SMZX mode 2 is set
  if(smzx_mode == 1)
  {
    for(i = 0; i < 256; i++)
    {
      update_smzx_color_direct(i,
       ((intensity_pal[i & 15][0] << 1) + intensity_pal[i >> 4][0]) / 3,
       ((intensity_pal[i & 15][1] << 1) + intensity_pal[i >> 4][1]) / 3,
       ((intensity_pal[i & 15][2] << 1) + intensity_pal[i >> 4][2]) / 3);
    }
  }
}

//Very quick fade out. Saves intensity table for fade in. Be sure
//to use in conjuction with the next function.
void vquick_fadeout(void) {
	int t1,t2;
	if(faded_out) return;
	//Save intensity table
	for(t1=0;t1<16;t1++)
		saved_intensity[t1]=current_intensity[t1];
	//Quick fadeout
	for(t1=0;t1<10;t1++) {
		for(t2=0;t2<16;t2++)
			set_color_intensity(t2,current_intensity[t2]-10);
		update_palette();
		}
	//Done
	faded_out=1;
}

//Very quick fade in. Uses intensity table saved from fade out. For
//use in conjuction with the previous function.
void vquick_fadein(void) {
	int t1,t2;
	if(!faded_out) return;
	//Clear now so update function works
	faded_out=0;
	//Quick fadein
	for(t1=0;t1<10;t1++) {
		for(t2=0;t2<16;t2++) {
			current_intensity[t2]+=10;
			if(current_intensity[t2]>saved_intensity[t2])
				current_intensity[t2]=saved_intensity[t2];
			set_color_intensity(t2,current_intensity[t2]);
			}
		update_palette();
		}
	//Done
}

//Instant fade out
void insta_fadeout(void) {
	int t1;
	if(faded_out) return;
	//Save intensity table
	for(t1=0;t1<16;t1++)
		saved_intensity[t1]=current_intensity[t1];
	for(t1=0;t1<16;t1++)
		set_color_intensity(t1,0);
	update_palette();
	//Done
	faded_out=1;
}

//Insta fade in
void insta_fadein(void) {
	int t1;
	if(!faded_out) return;
	//Clear now so update function works
	faded_out=0;
	//Quick fadein
	for(t1=0;t1<16;t1++)
		set_color_intensity(t1,current_intensity[t1]=saved_intensity[t1]);
	update_palette();
	//Done
}

//Sets RGB's to default EGA palette
void default_palette(void) {
	for(int t1=0;t1<16;t1++)
		set_rgb(t1,default_pal[t1][0],default_pal[t1][1],
			default_pal[t1][2]);
	update_palette();
}

//Get the current intensity of a color
char get_color_intensity(char color) {
	if(faded_out)
		return saved_intensity[color];
	return current_intensity[color];
}

//Gets the current RGB of a color
void get_rgb(char color,char &r,char &g,char &b) {
	r=current_pal[color][0];
	g=current_pal[color][1];
	b=current_pal[color][2];
}

//Returns non-zero if faded out
char is_faded(void) {
	return faded_out;
}


void set_Color_Aspect(char color,char aspect,int value)
{
	switch((int) aspect)
	{
	case 0:
	set_rgb(color,(char) value,current_pal[color][1],current_pal[color][2]);
	break;

	case 1:
	set_rgb(color,current_pal[color][0],(char) value,current_pal[color][2]);
	break;

	case 2:
	set_rgb(color,current_pal[color][0],current_pal[color][1],(char)value);
	break;
	}
	update_palette();
}

int get_Color_Aspect(char color,char aspect)
{
	return (int) current_pal[color][aspect];
}




