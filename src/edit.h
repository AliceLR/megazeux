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

#include "world.h"

void edit_world(World *mzx_world);
void draw_debug_box(int ypos);
int change_param(World *mzx_world, int id, int param, Robot *copy_robot,
 Scroll *copy_scroll, Sensor *copy_sensor);
int change_param(World *mzx_world, int id, int param, Robot *copy_robot,
 Scroll *copy_scroll, Sensor *copy_sensor);
int place_current_at_xy(World *mzx_world, int id, int color, int param,
 int x, int y, Robot *copy_robot, Scroll *copy_scroll, Sensor *copy_sensor,
 int overlay_edit);
void grab_at_xy(World *mzx_world, int *new_id, int *new_color,
 int *new_param, Robot *copy_robot, Scroll *copy_scroll,
 Sensor *copy_sensor, int x, int y, int overlay_edit);
void thing_menu(World *mzx_world, int menu_number, int *new_id,
 int *new_color, int *new_param, Robot *copy_robot, Scroll *copy_scroll,
 Sensor *copy_sensor, int x, int y);
void flash_thing(World *mzx_world, int start, int end, int flash_one,
 int flash_two, int scroll_x, int scroll_y, int edit_screen_height);

extern char tmenu_thing_ids[8][18];
extern char tmenu_num_choices[8];
extern char *tmenu_titles[8];
extern char *thing_menus[8][20];
extern char def_colors[128];
extern char debug_mode; // Debug mode
extern char debug_x; // Debug box x pos

#define EC_MAIN_BOX           25
#define EC_MAIN_BOX_DARK      16
#define EC_MAIN_BOX_CORNER    24
#define EC_MENU_NAME          27
#define EC_CURR_MENU_NAME     159
#define EC_MODE_STR           30
#define EC_MODE_HELP          31
#define EC_CURR_THING         31
#define EC_CURR_PARAM         23
#define EC_OPTION             26
#define EC_HIGHLIGHTED_OPTION 31
#define EC_DEBUG_BOX          76
#define EC_DEBUG_BOX_DARK     64
#define EC_DEBUG_BOX_CORNER   70
#define EC_DEBUG_LABEL        78
#define EC_DEBUG_NUMBER       79
#define EC_NA_FILL            1

#endif
