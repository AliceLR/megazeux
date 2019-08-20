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

#include "../../src/extmem.h"
#include "../../src/robot.h"

#include "dlmalloc.h"
#include "ram.h"
#include "extmem.h"

// NDS-specific interface
// Kevin Vance
static bool has_extmem = false;
static u16 *base;
static size_t capacity;
static mspace nds_mspace;

bool nds_ram_init(RAM_TYPE type)
{
  bool retval = false;

  // Check for RAM expansion.
  if(ram_init(type))
  {
    // Initialize an mspace for this RAM.
    has_extmem = true;
    base = (u16 *)ram_unlock();
    capacity = ram_size();
    nds_mspace = create_mspace_with_base(base, capacity, 0);
    retval = true;
    nds_ext_lock();
  }

  return retval;
}

void *nds_ext_malloc(size_t bytes)
{
  void *retval;

  if(has_extmem)
    retval = mspace_malloc(nds_mspace, bytes);
  else
    retval = NULL;

  return retval;
}

void *nds_ext_realloc(void *mem, size_t bytes)
{
  void *retval;

  if(has_extmem)
    retval = mspace_realloc(nds_mspace, mem, bytes);
  else
    retval = NULL;

  return retval;
}

void nds_ext_free(void *mem)
{
  if(has_extmem)
    mspace_free(nds_mspace, mem);
}

void nds_ext_unlock()
{
  if(has_extmem)
    ram_unlock();
}

void nds_ext_lock()
{
  if(has_extmem)
    ram_lock();
}

#define ALLOW_32BIT_WRITES

// Copy with no 8-bit writes.
void nds_ext_copy(void *dest, void *src, size_t size)
{
#ifdef ALLOW_32BIT_WRITES
  u32 *pd = (u32 *)dest;
  u32 *ps = (u32 *)src;
  u16 *pd16, *ps16;

  while (size >= 4)
  {
    *(pd++) = *(ps++);
    size -= 4;
  }

  pd16 = (u16 *)pd;
  ps16 = (u16 *)ps;

  switch (size)
  {
    case 0: break;
    case 1: *pd16 = (*pd16 & 0xFF00) | (*ps16 & 0x00FF); break;
    case 2: *pd16 = *ps16; break;
    case 3: *pd = (*pd & 0xFF000000) | (*ps & 0x00FFFFFF); break;
  }
#else
  u16 *pd = (u16 *)dest;
  u16 *ps = (u16 *)src;

  while(size >= 2)
  {
    *(pd++) = *(ps++);
    size -= 2;
  }

  if(size != 0)
    *pd = (*pd & 0xFF00) | (*ps & 0x00FF);
#endif
}

// Transfer buffer to extra RAM.
bool nds_ext_in(void *normal_buffer_ptr, size_t size)
{
  void **normal_buffer = (void**)normal_buffer_ptr;
  bool retval = false;

  if(*normal_buffer == NULL)
  {
    retval = true;
  }
  else
  {
    char *extra_buffer;
    nds_ext_unlock();
    extra_buffer = nds_ext_malloc(size);
    if(extra_buffer != NULL)
    {
      nds_ext_copy(extra_buffer, *normal_buffer, size);
      free(*normal_buffer);
      *normal_buffer = extra_buffer;
      retval = true;
    }
    nds_ext_lock();
  }

  return retval;
}

// Transfer buffer from extra RAM.
bool nds_ext_out(void *extra_buffer_ptr, size_t size)
{
  void **extra_buffer = (void**)extra_buffer_ptr;
  bool retval = false;

  if(*extra_buffer == NULL)
  {
    retval = true;
  }
  else
  {
    char *normal_buffer;
    normal_buffer = malloc(size);
    if(normal_buffer != NULL)
    {
      nds_ext_unlock();
      memcpy(normal_buffer, *extra_buffer, size);
      nds_ext_free(*extra_buffer);
      *extra_buffer = normal_buffer;
      retval = true;
      nds_ext_lock();
    }
  }

  return retval;
}

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

  clear_label_cache(robot);
  cache_robot_labels(robot);
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

  clear_label_cache(robot);
  cache_robot_labels(robot);
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
  if(has_extmem && mzx_world->current_board)
    store_board_to_extram(mzx_world->current_board);
  mzx_world->current_board = cur_board;
}

// Set the current board to cur_board, in extra memory.
void set_current_board_ext(struct world *mzx_world, struct board *cur_board)
{
  set_current_board(mzx_world, cur_board);
  if(has_extmem)
    retrieve_board_from_extram(cur_board);
}
