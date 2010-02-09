/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* Declarations for EDIT.CPP */

#ifndef __EDITOR_EDIT_H
#define __EDITOR_EDIT_H

#include "compat.h"

__M_BEGIN_DECLS

#include "../world_struct.h"

typedef struct
{
  Board *src_board;
  int cursor_x, cursor_y;
  int cursor_board_x, cursor_board_y;
  int scroll_x, scroll_y;
  mzx_thing current_id;
  int current_color;
  int current_param;
  int board_width, board_height;
  int draw_mode;
  int current_menu;
  int show_level;
  int block_x, block_y;
  int block_dest_x, block_dest_y;
  int block_command;
  Board *block_board;
  int text_place;
  int text_start_x;
  int modified;
  int debug_x;
  int saved_overlay_mode;
  int edit_screen_height;
  int copy_repeat_width;
  int copy_repeat_height;
  char *level_id;
  char *level_param;
  char *level_color;
  char *overlay;
  char *overlay_color;
  int overlay_edit;
  char current_world[128];
  char mzm_name_buffer[128];
  char current_listening_dir[MAX_PATH];
  char current_listening_mod[512];
  int listening_flag;
  int saved_scroll_x[16];
  int saved_scroll_y[16];
  int saved_cursor_x[16];
  int saved_cursor_y[16];
  int saved_board[16];
  int saved_debug_x[16];
  Robot copy_robot;
  Scroll copy_scroll;
  Sensor copy_sensor;
} EditorState;

void edit_world(World *mzx_world);
int place_current_at_xy(World *mzx_world, mzx_thing id, int color,
 int param, int x, int y, Robot *copy_robot, Scroll *copy_scroll,
 Sensor *copy_sensor, int overlay_edit);

extern char debug_mode; // Debug mode
extern char debug_x; // Debug box x pos

#ifdef CONFIG_AUDIO
extern const char *mod_gdm_ext[];
#endif

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
#define EC_NA_FILL            1

__M_END_DECLS

#endif // __EDITOR_EDIT_H
