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

/* Declarations for EDIT.CPP */

#ifndef __EDIT_H
#define __EDIT_H

void add_ext(char far *str,char far *ext);
void edit_world(void);
void draw_debug_box(char ypos=12);

extern char tmenu_thing_ids[8][18];
extern char tmenu_num_choices[8];
extern char far *tmenu_titles[8];
extern char far *thing_menus[8];
extern unsigned char def_colors[128];
extern char debug_mode;//Debug mode
extern char debug_x;//Debug box x pos

#define EC_MAIN_BOX	25
#define EC_MAIN_BOX_DARK	16
#define EC_MAIN_BOX_CORNER	24
#define EC_MENU_NAME	27
#define EC_CURR_MENU_NAME	159
#define EC_MODE_STR	30
#define EC_MODE_HELP	31
#define EC_CURR_THING	31
#define EC_CURR_PARAM	23
#define EC_OPTION	26
#define EC_HIGHLIGHTED_OPTION	31
#define EC_DEBUG_BOX	76
#define EC_DEBUG_BOX_DARK	64
#define EC_DEBUG_BOX_CORNER	70
#define EC_DEBUG_LABEL	78
#define EC_DEBUG_NUMBER	79
#define EC_NA_FILL	1

#endif