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

#include "../../src/error.h"
#include "../../src/extmem.h"
#include "../../src/robot.h"
#include "../../src/util.h"

#include "dlmalloc.h"
#include "ram.h"
#include "extmem.h"

// CRC-32 values can optionally be computed to verify that the block of memory
// retrieved from a RAM cart is the same as the block originally stored. This
// is useful for debugging but makes extram operations slower.
//#define ENABLE_CRCS

#ifdef ENABLE_CRCS
#define CRC_SIZE 4
#include <zlib.h>
#else
#define CRC_SIZE 0
#endif

static boolean has_extmem = false;
static u16 *base;
static size_t capacity;
static mspace nds_mspace;

enum extram_result
{
  SUCCESS = 0,
  OUT_OF_NORMAL_MEMORY,
  OUT_OF_EXTRA_MEMORY,
  CHECKSUM_FAILED
};

static const char *extram_result_string(enum extram_result res)
{
  switch(res)
  {
    case SUCCESS:               return "No error";
    case OUT_OF_NORMAL_MEMORY:  return "Out of memory";
    case OUT_OF_EXTRA_MEMORY:   return "Out of extra memory";
    case CHECKSUM_FAILED:       return "Extra memory checksum failed";
    default:                    return "Unknown error";
  }
}

#define extram_check_error(e) real_extram_check_error(e, __FILE__, __LINE__)

static void real_extram_check_error(enum extram_result err, const char *file,
 int line)
{
  char msgbuf[128];

  if(err)
  {
    snprintf(msgbuf, sizeof(msgbuf), "%s at %s:%d",
     extram_result_string(err), file, line);
    msgbuf[sizeof(msgbuf)-1] = '\0';
    error(msgbuf, ERROR_T_FATAL, ERROR_OPT_EXIT|ERROR_OPT_NO_HELP, 0);
  }
}

static void nds_ext_print_info(void)
{
  static boolean has_printed = false;
  if(has_printed) return;
  has_printed = true;

  if(has_extmem)
    info("Using '%s' RAM expansion with capacity of %d (base %p).\n",
     ram_type_string(), capacity, (void *)base);
  else
    info("No RAM expansion detected.\n");
}

static void *nds_ext_malloc(size_t bytes)
{
  void *retval;

  if(has_extmem)
    retval = mspace_malloc(nds_mspace, bytes);
  else
    retval = NULL;

  return retval;
}

static inline void *nds_ext_realloc(void *mem, size_t bytes)
{
  void *retval;

  if(has_extmem)
    retval = mspace_realloc(nds_mspace, mem, bytes);
  else
    retval = NULL;

  return retval;
}

static void nds_ext_free(void *mem)
{
  if(has_extmem)
    mspace_free(nds_mspace, mem);
}

static void nds_ext_unlock()
{
  if(has_extmem)
    ram_unlock();
}

static void nds_ext_lock()
{
  if(has_extmem)
    ram_lock();
}

#define ALLOW_32BIT_WRITES

// Copy with no 8-bit writes.
static void nds_ext_copy(void *dest, void *src, size_t size)
{
#ifdef ALLOW_32BIT_WRITES
  u32 *pd = (u32 *)dest;
  u32 *ps = (u32 *)src;

  // Optimizers want to make this do a single byte write to the dest if this
  // isn't volatile, and single byte writes to extra RAM don't work here.
  volatile u16 *pd16;
  u16 *ps16;

  while(size >= 4)
  {
    *(pd++) = *(ps++);
    size -= 4;
  }

  pd16 = (u16 *)pd;
  ps16 = (u16 *)ps;

  switch(size)
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
  {
    // Optimizers want to make this do a single byte write to the dest if this
    // isn't volatile, and single byte writes to extra RAM don't work here.
    volatile u16 *pdv = pd;
    *pdv = (*pd & 0xFF00) | (*ps & 0x00FF);
  }
#endif
}

// Transfer buffer to extra RAM.
static enum extram_result nds_ext_in(void *normal_buffer_ptr, size_t size)
{
  void **normal_buffer = (void **)normal_buffer_ptr;
  enum extram_result retval = SUCCESS;

  if(*normal_buffer)
  {
    char *extra_buffer;
    nds_ext_unlock();
    extra_buffer = nds_ext_malloc(CRC_SIZE + size);
    if(extra_buffer != NULL)
    {
#ifdef ENABLE_CRCS
      u32 crc = crc32(0, *normal_buffer, size);
      *((u32 *)extra_buffer) = crc;
#endif
      nds_ext_copy(extra_buffer + CRC_SIZE, *normal_buffer, size);
      free(*normal_buffer);
      *normal_buffer = extra_buffer;
    }
    else
      retval = OUT_OF_EXTRA_MEMORY;

    nds_ext_lock();
  }

  return retval;
}

// Transfer buffer from extra RAM.
static enum extram_result nds_ext_out(void *extra_buffer_ptr, size_t size)
{
  void **extra_buffer = (void **)extra_buffer_ptr;
  enum extram_result retval = SUCCESS;

  if(*extra_buffer)
  {
    char *normal_buffer;
    normal_buffer = malloc(size);
    if(normal_buffer != NULL)
    {
#ifdef ENABLE_CRCS
      u32 initial_crc;
      u32 crc;
#endif
      nds_ext_unlock();
#ifdef ENABLE_CRCS
      initial_crc = *((u32 *)*extra_buffer);
#endif
      memcpy(normal_buffer, ((char *)*extra_buffer) + CRC_SIZE, size);
      nds_ext_free(*extra_buffer);
      *extra_buffer = normal_buffer;
      nds_ext_lock();

#ifdef ENABLE_CRCS
      crc = crc32(0, (void *)normal_buffer, size);
      if(initial_crc != crc)
      {
        FILE *fp = fopen_unsafe("__DUMP", "wb");
        fwrite(normal_buffer, size, 1, fp);
        fclose(fp);

        warn("Checksum error: expected %lx, got %lx (size %zu)\n",
         initial_crc, crc, size);
        retval = CHECKSUM_FAILED;
      }
#endif
    }
    else
      retval = OUT_OF_NORMAL_MEMORY;
  }

  return retval;
}

boolean nds_ram_init(RAM_TYPE type)
{
  boolean retval = false;

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

// Move the robot's memory from normal RAM to extra RAM.
static void store_robot_to_extram(struct robot *robot)
{
  enum extram_result ret;
  if(!robot)
    return;

  // Store the stack in extram.
  ret = nds_ext_in(&robot->stack, robot->stack_size * sizeof(int));
  extram_check_error(ret);

  // Store the program in extram, noting the address change.
  ret = nds_ext_in(&robot->program_bytecode, robot->program_bytecode_length);
  extram_check_error(ret);

  clear_label_cache(robot);
}

// Move the robot's memory from extra RAM to normal RAM.
static void retrieve_robot_from_extram(struct robot *robot)
{
  enum extram_result ret;
  if(!robot)
    return;

  ret = nds_ext_out(&robot->stack, robot->stack_size * sizeof(int));
  extram_check_error(ret);

  ret = nds_ext_out(&robot->program_bytecode, robot->program_bytecode_length);
  extram_check_error(ret);

  clear_label_cache(robot);
  cache_robot_labels(robot);
}

// Move the board's memory from normal RAM to extra RAM.
void store_board_to_extram(struct board *board)
{
  enum extram_result ret;
  size_t i, num_fields;
  int size, j;

  char **ptr_list[] =
  {
    &board->level_id, &board->level_param, &board->level_color,
    &board->level_under_id, &board->level_under_param,
    &board->level_under_color, &board->overlay, &board->overlay_color
  };

  nds_ext_print_info();
  if(!has_extmem)
    return;

  // Skip the last 2 pointers if overlays are disabled.
  num_fields = sizeof(ptr_list)/sizeof(*ptr_list);
  if(!board->overlay_mode)
    num_fields -= 2;

  size = board->board_width * board->board_height;
  for(i = 0; i < num_fields; i++)
  {
    ret = nds_ext_in((void**)ptr_list[i], size);
    extram_check_error(ret);
  }

  // Copy the robots if necessary.
  if(board->robot_list)
    for(j = 1; j <= board->num_robots; j++)
      store_robot_to_extram(board->robot_list[j]);
}

// Move the board's memory from normal RAM to extra RAM.
void retrieve_board_from_extram(struct board *board)
{
  enum extram_result ret;
  size_t i, num_fields;
  int j, size;

  char **ptr_list[] =
  {
    &board->level_id, &board->level_param, &board->level_color,
    &board->level_under_id, &board->level_under_param,
    &board->level_under_color, &board->overlay, &board->overlay_color
  };

  nds_ext_print_info();
  if(!has_extmem)
    return;

  // Skip the last 2 pointers if overlays are disabled.
  num_fields = sizeof(ptr_list)/sizeof(*ptr_list);
  if(!board->overlay_mode)
    num_fields -= 2;

  size = board->board_width * board->board_height;
  for(i = 0; i < num_fields; i++)
  {
    ret = nds_ext_out((void**)ptr_list[i], size);
    extram_check_error(ret);
  }

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
