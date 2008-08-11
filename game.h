/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
 * Copyright (C) 1999 Charles Goetzman
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

/* Declarations for GAME.CPP */

#ifndef __GAME_H
#define __GAME_H

void title_screen(void);
void draw_viewport(void);
void update_variables(char slowed=0);
void calculate_xytop(int &x,int &y);
void update_player(void);
void game_settings(void);
void play_game(char fadein=1);
char move_player(char dir);
char grab_item(int x,int y,char dir);//Dir is for transporter
void show_status(void);
void show_counter(char far *str,char x,char y,char skip_if_zero=0);
char update(char game,char &fadein);

//For changing screens AFTER an update is done and shown
extern int target_board;//Where to go
extern int target_where;//0 for x/y, 1 for entrance
extern int target_x;//Or color of entrance
extern int target_y;//Or id of entrance
extern int target_d_id;
extern int target_d_color;


extern char pal_update;

#ifdef __cplusplus
extern "C" {
#endif

void set_mesg(char far *str);
void set_3_mesg(char far *str1,int num,char far *str2);
void rotate(int x,int y,char dir);
void find_player(void);

#ifdef __cplusplus
}
#endif

#endif