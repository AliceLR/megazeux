/* $Id$
 * MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick
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

#ifndef BOARD_H
#define BOARD_H

#include "const.h"
#include "robot.h"

typedef struct _Board Board;

#define MAX_BOARDS 250

struct _Board
{
  int size;

  char board_name[BOARD_NAME_SIZE];

  int board_width;
  int board_height;

  int overlay_mode;
  char *level_id;
  char *level_param;
  char *level_color;
  char *level_under_id;
  char *level_under_param;
  char *level_under_color;
  char *overlay;
  char *overlay_color;

  char mod_playing[FILENAME_SIZE];
  int viewport_x;
  int viewport_y;
  int viewport_width;
  int viewport_height;
  int can_shoot;
  int can_bomb;
  int fire_burn_brown;
  int fire_burn_space;
  int fire_burn_fakes;
  int fire_burn_trees;
  int explosions_leave;
  int save_mode;
  int forest_becomes;
  int collect_bombs;
  int fire_burns;
  int board_dir[4];
  int restart_if_zapped;
  int time_limit;
  int last_key;
  int num_input;
  int input_size;
  char input_string[81];
  int player_last_dir;
  char bottom_mesg[81];
  int b_mesg_timer;
  int lazwall_start;
  int b_mesg_row;
  int b_mesg_col;
  int scroll_x;
  int scroll_y;
  int locked_x;
  int locked_y;
  int player_ns_locked;
  int player_ew_locked;
  int player_attack_locked;
  int volume;
  int volume_inc;
  int volume_target;

  int num_robots;
  int num_robots_active;
  int num_robots_allocated;
  Robot **robot_list;
  Robot **robot_list_name_sorted;
  int num_scrolls;
  int num_scrolls_allocated;
  Scroll **scroll_list;
  int num_sensors;
  int num_sensors_allocated;
  Sensor **sensor_list;
};

void replace_current_board(World *mzx_world, char *name);
Board *load_board_allocate(FILE *fp, int savegame);
Board *load_board_allocate_direct(FILE *fp, int savegame);
void load_board(Board *cur_board, FILE *fp, int savegame);
void load_board_direct(Board *cur_board, FILE *fp, int savegame);
Board *create_blank_board();
void save_board_file(Board *cur_board, char *name);
int save_board(Board *cur_board, FILE *fp, int savegame);
void load_RLE2_plane(char *plane, FILE *fp, int size);
void save_RLE2_plane(char *plane, FILE *fp, int size);
void clear_board(Board *cur_board);
int find_board(World *mzx_world, char *name);
void change_board_size(Board *src_board, int new_width, int new_height);

#endif
