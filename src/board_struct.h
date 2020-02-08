/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef __BOARD_STRUCT_H
#define __BOARD_STRUCT_H

#include "compat.h"

__M_BEGIN_DECLS

#include "robot_struct.h"

struct board
{
  int size;
  int world_version;

  char board_name[32];

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

  char mod_playing[MAX_PATH];
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
  int reset_on_entry;
  int time_limit;
  int last_key;
  int num_input;
  size_t input_size;
  char input_string[ROBOT_MAX_TR];
  int player_last_dir;
  char bottom_mesg[ROBOT_MAX_TR];
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
  char charset_path[MAX_PATH];
  char palette_path[MAX_PATH];

  int num_robots;
  int num_robots_active;
  int num_robots_allocated;
  struct robot **robot_list;
  struct robot **robot_list_name_sorted;
  int num_scrolls;
  int num_scrolls_allocated;
  struct scroll **scroll_list;
  int num_sensors;
  int num_sensors_allocated;
  struct sensor **sensor_list;
#ifdef DEBUG
  boolean is_extram;
#endif
};

__M_END_DECLS

#endif // __BOARD_STRUCT_H
