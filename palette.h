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

/* PALETTE.H- Declarations for PALETTE.CPP */

#ifndef __PALETTE_H
#define __PALETTE_H
extern char smzx_mode;
extern char smzx_pal_initialized;
extern char ati_fix;

void set_vga_register(char color,char r,char g,char b);
//void get_vga_register(char color, char &r, char &g, char &b);
void update_smzx_palette();
void update_smzx_color(unsigned char c);
void update_smzx_color_direct(unsigned char c, unsigned char r, 
 unsigned char g, unsigned char b);
void load_smzx_palette(char far *fname);
void init_smzx_mode();
void set_ega_register(char attr,char ccode);
void unblank_screen(void);

extern char far default_EGA_hardware_pal[16];
extern char far smzx_mode2_palette[768];
extern char intensity_pal[16][3];

void init_palette(void);
void reinit_palette(void);
void set_palette_intensity(char percent);
void set_color_intensity(char color,char percent);
void set_rgb(char color,char r,char g,char b);
char get_color_intensity(char color);
void get_rgb(char color,char &r,char &g,char &b);
void update_palette(char wait_for_retrace=1);
void smzx_update_palette(char wait_for_retrace=1);
void normal_update_palette(char wait_for_retrace=1);
void update_HIGHpalette(char wait_for_retrace=1);
void vquick_fadeout(void);
void vquick_fadein(void);
void insta_fadeout(void);
void insta_fadein(void);
void default_palette(void);
char is_faded(void);
int  get_Color_Aspect(char color,char aspect);
void  set_Color_Aspect(char color,char aspect,int value);
#endif