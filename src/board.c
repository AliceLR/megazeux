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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "const.h"
#include "error.h"
#include "extmem.h"
#include "legacy_board.h"
#include "world.h"
#include "util.h"


__editor_maybe_static
int load_board_direct(struct world *mzx_world, struct board *cur_board,
 FILE *fp, int data_size, int savegame, int file_version)
{
  // FIXME
  return legacy_load_board_direct(mzx_world, cur_board, fp, data_size, savegame,
   file_version);
}


struct board *load_board_allocate(struct world *mzx_world, FILE *fp,
 int savegame, int file_version)
{
  // FIXME
  return legacy_load_board_allocate(mzx_world, fp, savegame, file_version);
}



int save_board(struct world *mzx_world, struct board *cur_board, FILE *fp,
 int savegame, int file_version)
{
  // FIXME
  return legacy_save_board(mzx_world, cur_board, fp, savegame, file_version);
}


void dummy_board(struct board *cur_board)
{
  // Dummy out an existing board struct without existing elements
  int size = 2000;
  cur_board->overlay_mode = 0;
  cur_board->board_width = 80;
  cur_board->board_height = 25;

  cur_board->level_id = ccalloc(1, size);
  cur_board->level_color = ccalloc(1, size);
  cur_board->level_param = ccalloc(1, size);
  cur_board->level_under_id = ccalloc(1, size);
  cur_board->level_under_color = ccalloc(1, size);
  cur_board->level_under_param = ccalloc(1, size);

  cur_board->level_id[0] = 127;

  strcpy(cur_board->mod_playing, "");
  cur_board->viewport_x = 0;
  cur_board->viewport_y = 0;
  cur_board->viewport_width = 80;
  cur_board->viewport_height = 25;
  cur_board->can_shoot = 0;
  cur_board->can_bomb = 0;
  cur_board->fire_burn_brown = 0;
  cur_board->fire_burn_space = 0;
  cur_board->fire_burn_fakes = 0;
  cur_board->fire_burn_trees = 0;
  cur_board->explosions_leave = 0;
  cur_board->save_mode = 1;
  cur_board->forest_becomes = 0;
  cur_board->collect_bombs = 0;
  cur_board->fire_burns = 0;
  cur_board->board_dir[0] = 0;
  cur_board->board_dir[1] = 0;
  cur_board->board_dir[2] = 0;
  cur_board->board_dir[3] = 0;
  cur_board->restart_if_zapped = 0;
  cur_board->time_limit = 0;
  cur_board->last_key = 0;
  cur_board->num_input = 0;
  cur_board->input_size = 0;
  strcpy(cur_board->input_string, "");
  cur_board->player_last_dir = 0;
  strcpy(cur_board->bottom_mesg, "");
  cur_board->b_mesg_timer = 0;
  cur_board->lazwall_start = 0;
  cur_board->b_mesg_row = 24;
  cur_board->b_mesg_col = -1;
  cur_board->scroll_x = 0;
  cur_board->scroll_y = 0;
  cur_board->locked_x = 0;
  cur_board->locked_y = 0;
  cur_board->player_ns_locked = 0;
  cur_board->player_ew_locked = 0;
  cur_board->player_attack_locked = 0;
  cur_board->volume = 0;
  cur_board->volume_inc = 0;
  cur_board->volume_target = 0;

  cur_board->num_robots = 0;
  cur_board->num_robots_active = 0;
  cur_board->num_robots_allocated = 0;
  cur_board->robot_list = ccalloc(1, sizeof(struct robot *));
  cur_board->robot_list_name_sorted = ccalloc(1, sizeof(struct robot *));
  cur_board->num_scrolls = 0;
  cur_board->num_scrolls_allocated = 0;
  cur_board->scroll_list = ccalloc(1, sizeof(struct scroll *));
  cur_board->num_sensors = 0;
  cur_board->num_sensors_allocated = 0;
  cur_board->sensor_list = ccalloc(1, sizeof(struct sensor *));
}

void clear_board(struct board *cur_board)
{
  int i;
  int num_robots_active = cur_board->num_robots_active;
  int num_scrolls = cur_board->num_scrolls;
  int num_sensors = cur_board->num_sensors;
  struct robot **robot_list = cur_board->robot_list;
  struct robot **robot_name_list = cur_board->robot_list_name_sorted;
  struct scroll **scroll_list = cur_board->scroll_list;
  struct sensor **sensor_list = cur_board->sensor_list;

  free(cur_board->level_id);
  free(cur_board->level_param);
  free(cur_board->level_color);
  free(cur_board->level_under_id);
  free(cur_board->level_under_param);
  free(cur_board->level_under_color);

  if(cur_board->overlay_mode)
  {
    free(cur_board->overlay);
    free(cur_board->overlay_color);
  }

  for(i = 0; i < num_robots_active; i++)
    if(robot_name_list[i])
      clear_robot(robot_name_list[i]);

  free(robot_name_list);
  free(robot_list);

  for(i = 1; i <= num_scrolls; i++)
    if(scroll_list[i])
      clear_scroll(scroll_list[i]);

  free(scroll_list);

  for(i = 1; i <= num_sensors; i++)
    if(sensor_list[i])
      clear_sensor(sensor_list[i]);

  free(sensor_list);
  free(cur_board);
}

// Just a linear search. Boards aren't addressed by name very often.
int find_board(struct world *mzx_world, char *name)
{
  struct board **board_list = mzx_world->board_list;
  struct board *current_board;
  int num_boards = mzx_world->num_boards;
  int i;
  for(i = 0; i < num_boards; i++)
  {
    current_board = board_list[i];
    if(current_board && !strcasecmp(name, current_board->board_name))
      break;
  }

  if(i != num_boards)
  {
    return i;
  }
  else
  {
    return NO_BOARD;
  }
}
