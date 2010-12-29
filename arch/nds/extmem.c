/* MegaZeux
 *
 * Copyright (C) 2007 Kevin Vance <kvance@kvance.com>
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

/* This file is only used on NDS, and is not even compiled on
 * other systems. It handles transferral of main-memory data structures
 * into slower external memory to reduce memory pressure.
 */

#include "extmem.h"
#include "dlmalloc.h"
#include "robot.h"

// Move the robot's memory from normal RAM to extra RAM.
static void store_robot_to_extram(struct robot *robot)
{
  if(!robot)
    return;

  // Store the stack in extram.
  if(!nds_ext_in(&robot->stack, robot->stack_size * sizeof(int))) {}
    // TODO: handle out-of-memory

  // Store the program in extram, noting the address change.
  if(!nds_ext_in(&robot->program_bytecode, robot->program_bytecode_length)) {}
    // TODO: handle out-of-memory

  if(robot->used)
  {
    clear_label_cache(robot->label_list, robot->num_labels);
    robot->label_list = cache_robot_labels(robot, &robot->num_labels);
  }
}

// Move the robot's memory from extra RAM to normal RAM.
static void retrieve_robot_from_extram(struct robot *robot)
{
  if(!robot)
    return;

  if(!nds_ext_out(&robot->stack, robot->stack_size * sizeof(int))) {}
    // TODO: handle out-of-memory

  if(!nds_ext_out(&robot->program_bytecode, robot->program_bytecode_length)) {}
    // TODO: handle out-of-memory

  if(robot->used)
  {
    clear_label_cache(robot->label_list, robot->num_labels);
    robot->label_list = cache_robot_labels(robot, &robot->num_labels);
  }
}

// Move the board's memory from normal RAM to extra RAM.
void store_board_to_extram(struct board *board)
{
  size_t i, num_fields;
  int size, j;

  char **ptr_list[] =
  {
    &board->level_id, &board->level_param, &board->level_color,
    &board->level_under_id, &board->level_under_param,
    &board->level_under_color, &board->overlay, &board->overlay_color
  };

  // Skip the last 2 pointers if overlays are disabled.
  num_fields = sizeof(ptr_list)/sizeof(*ptr_list);
  if(!board->overlay_mode)
    num_fields -= 2;

  size = board->board_width * board->board_height;
  for(i = 0; i < num_fields; i++)
    if(!nds_ext_in((void**)ptr_list[i], size)) {}
      // TODO: handle out-of-memory

  // Copy the robots if necessary.
  if(board->robot_list)
    for(j = 1; j <= board->num_robots; j++)
      store_robot_to_extram(board->robot_list[j]);
}

// Move the board's memory from normal RAM to extra RAM.
void retrieve_board_from_extram(struct board *board)
{
  size_t i, num_fields;
  int j, size;

  char **ptr_list[] =
  {
    &board->level_id, &board->level_param, &board->level_color,
    &board->level_under_id, &board->level_under_param,
    &board->level_under_color, &board->overlay, &board->overlay_color
  };

  // Skip the last 2 pointers if overlays are disabled.
  num_fields = sizeof(ptr_list)/sizeof(*ptr_list);
  if(!board->overlay_mode)
    num_fields -= 2;

  size = board->board_width * board->board_height;
  for(i = 0; i < num_fields; i++)
    if(!nds_ext_out((void**)ptr_list[i], size)) {}
      // TODO: handle out-of-memory

  // Copy the robots if necessary.
  if(board->robot_list)
    for(j = 1; j <= board->num_robots; j++)
      retrieve_robot_from_extram(board->robot_list[j]);
}


// Set the current board to cur_board, in regular memory.
void set_current_board(struct world *mzx_world, struct board *cur_board)
{
  if(mzx_world->current_board)
    store_board_to_extram(mzx_world->current_board);
  mzx_world->current_board = cur_board;
}

// Set the current board to cur_board, in extra memory.
void set_current_board_ext(struct world *mzx_world, struct board *cur_board)
{
  set_current_board(mzx_world, cur_board);
  retrieve_board_from_extram(cur_board);
}
