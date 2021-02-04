/* MegaZeux
 *
 * Copyright (C) 2021 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "error.h"
#include "extmem.h"
#include "platform_endian.h"
#include "robot.h"
#include "util.h"
#include "world_struct.h"

#include <stdint.h>
#include <zlib.h>

#ifdef CONFIG_NDS
#include "../arch/nds/extmem.h"
#define USE_PLATFORM_EXTRAM_ALLOC
#endif

/**
 * NOTE: platform allocation and platform store/retrieve are mutually exclusive.
 * Only implement at most one variant for a particular platform and use the
 * dummy implementations for the other.
 */
#if defined(USE_PLATFORM_EXTRAM_ALLOC) && defined(USE_PLATFORM_EXTRAM_STORE)
#error Only implement one of platform_extram_alloc and platform_extram_store!
#endif

#ifndef USE_PLATFORM_EXTRAM_ALLOC
/* Attempt to allocate a memory mapped buffer in extra RAM. */
static uint32_t *platform_extram_alloc(size_t len) { return NULL; }
/* Attempt to reallocate a memory mapped buffer in extra RAM. */
static uint32_t *platform_extram_resize(void *buffer, size_t len) { return NULL; }
/* Free a memory mapped buffer in extra RAM. */
static void platform_extram_free(void *buffer) {}
#endif

#ifndef USE_PLATFORM_EXTRAM_STORE
/* Send a locally allocated buffer to non-mapped extra RAM. */
static void *platform_extram_store(void *buffer, size_t len) { return buffer; }
/* Retrieve a buffer from non-mapped extra RAM. */
static void *platform_extram_retrieve(void *buffer, size_t len) { return buffer; }
#endif

#define EXTRAM_ID ((uint32_t)(('E') | ('X' << 8) | ('T' << 16) | (0xfe << 24)))

enum extram_flags
{
  EXTRAM_PLATFORM_ALLOC   = (1 << 0), /* Buffer created by platform_extram. */
  EXTRAM_DEFLATE          = (1 << 1), /* Buffer uses DEFLATE compression. */
  EXTRAM_PAGED            = (1 << 2), /* Buffer was paged to disk. (TODO?) */
};

struct extram_block
{
  uint32_t id;
  uint32_t flags;
  uint32_t uncompressed_size;
  uint32_t compressed_size;
  uint32_t adler32;
  uint32_t data[];
};

/**
 * Perform a 32-bit aligned copy. Both the source and the dest buffers must be
 * 32-bit aligned and should be a multiple of 4 bytes long.
 */
static boolean extram_cpy(void * RESTRICT dest, const void *src, size_t len)
{
  const uint32_t *s32 = src;
  uint32_t *d32 = dest;
  size_t len32;
  size_t i;

  if((len & 3) || (((size_t)src) & 3) || (((size_t)dest) & 3))
  {
    debug("--EXTRAM-- bad copy parameters: %p %p %zu!\n", dest, src, len);
    return false;
  }

  len32 = len >> 2;
  for(i = 0; i < len32; i++)
    d32[i] = s32[i];

  return true;
}

/**
 * Get the extra memory block size for an input buffer.
 */
static size_t extram_block_size(size_t len)
{
  return sizeof(struct extram_block) + ((len + 3) & ~3);
}

/**
 * Send a buffer to extra memory.
 *
 * @param  _src pointer to buffer to store to extra RAM.
 * @param  len  size of buffer to store to extra RAM.
 * @return      `true` on success, otherwise `false`.
 */
static boolean store_buffer_to_extram(void **_src, size_t len)
{
  struct extram_block *block;
  uint8_t *src = *_src;
  void *ptr;
  uint32_t compressed_size = len;
  uint32_t flags = EXTRAM_PLATFORM_ALLOC;
  size_t alloc_size;
  z_stream z;

  trace("--EXTRAM-- store_buffer_to_extram %p %zu\n", src, len);

  if(len > 1024)
  {
    // Project compressed size...
    int res;

    memset(&z, 0, sizeof(z_stream));
    z.next_in = src;
    z.avail_in = len;

    res = deflateInit2(&z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS,
     8, Z_DEFAULT_STRATEGY);
    if(res == Z_OK)
    {
      compressed_size = deflateBound(&z, len);
      flags |= EXTRAM_DEFLATE;
    }
  }

  alloc_size = extram_block_size(compressed_size);
  block = (struct extram_block *)platform_extram_alloc(alloc_size);
  if(!block)
  {
    flags &= ~EXTRAM_PLATFORM_ALLOC;
    block = cmalloc(alloc_size);
  }

  block->id = EXTRAM_ID;
  block->flags = flags;
  block->uncompressed_size = len;
  block->adler32 = adler32_z(0L, NULL, 0);
  block->adler32 = adler32_z(block->adler32, src, len);

  if(flags & EXTRAM_DEFLATE)
  {
    // Compress.
    uint32_t tmp[2048 / 4];
    uint32_t *pos = block->data;
    size_t sz;
    size_t new_alloc_size;
    int res = Z_OK;

    z.next_in = src;
    z.avail_in = len;

    while(res != Z_STREAM_END)
    {
      z.next_out = (Bytef *)tmp;
      z.avail_out = sizeof(tmp);

      res = deflate(&z, Z_FINISH);

      if(res != Z_OK && res != Z_BUF_ERROR && res != Z_STREAM_END)
      {
        debug("--EXTRAM-- deflate failed with code %d\n", res);
        goto err;
      }

      sz = (sizeof(tmp) - z.avail_out + 3) & ~3;
      if(!extram_cpy(pos, tmp, sz))
        goto err;

      pos += sz >> 2;
    }

    trace("--EXTRAM--   compressed_size=%zu\n", (size_t)z.total_out);

    block->compressed_size = (z.total_out + 3) & ~3;
    deflateEnd(&z);

    /* Shrink the allocation to the real compressed size. */
    new_alloc_size = extram_block_size(block->compressed_size);
    if(new_alloc_size < alloc_size)
    {
      alloc_size = new_alloc_size;
      if(flags & EXTRAM_PLATFORM_ALLOC)
      {
        ptr = platform_extram_resize(block, new_alloc_size);
      }
      else
        ptr = crealloc(block, new_alloc_size);

      if(!ptr)
        goto err;
      block = (struct extram_block *)ptr;
    }
  }
  else
  {
    // Store.
    block->compressed_size = len;

    if(!extram_cpy(block->data, src, len & ~3))
      goto err;

    if(len & 3)
    {
      uint32_t buffer = 0;
      uint32_t pos;
      size_t i = len & ~3;

#if PLATFORM_BYTE_ORDER == PLATFORM_LIL_ENDIAN
      for(pos = 0; i < len; i++, pos += 8)
        buffer |= src[i] << pos;
#else
      for(pos = 24; i < len; i++, pos -= 8)
        buffer |= src[i] << pos;
#endif
      block->data[len >> 2] = buffer;
    }
  }

  ptr = platform_extram_store(block, alloc_size);
  if(!ptr)
  {
    debug("--EXTRAM-- failed to send buffer to non-mapped platform RAM.\n");
    goto err;
  }

  free(src);
  *_src = ptr;
  return true;

err:
  if(flags & EXTRAM_PLATFORM_ALLOC)
  {
    platform_extram_free(block);
  }
  else
    free(block);

  if(flags & EXTRAM_DEFLATE)
    deflateEnd(&z);

  return false;
}

/**
 * Retrieve a buffer from extra memory.
 *
 * @param  src  pointer to buffer to retrieve from extra RAM.
 * @param  len  length of buffer to retreieve from extra RAM.
 * @return      `true` on success, otherwise `false`.
 */
static boolean retrieve_buffer_from_extram(void **src, size_t len)
{
  struct extram_block *block = (struct extram_block *)(*src);
  void *ptr;
  uint32_t flags = block->flags;
  uint32_t checksum;
  size_t alloc_size;
  uint8_t *buffer;

  trace("--EXTRAM-- retrieve_buffer_from_extram %p %zu\n", *src, len);

  ptr = platform_extram_retrieve(block, len);
  if(!ptr)
  {
    debug("--EXTRAM-- failed to retrieve buffer %p from non-mapped platform RAM.\n", *src);
    return false;
  }

  /* Try to eliminate false positives arriving here... */
  if(block->id != EXTRAM_ID)
  {
    debug("--EXTRAM-- ID mismatch @ %p\n", (void *)block);
    return false;
  }

  if(len != block->uncompressed_size)
  {
    debug("--EXTRAM-- size mismatch @ %p (expected %zu, got %zu)\n",
      (void *)block, len, (size_t)block->uncompressed_size
    );
    return false;
  }

  alloc_size = (len + 3) & ~3;
  buffer = cmalloc(alloc_size);

  if(flags & EXTRAM_DEFLATE)
  {
    // Compressed.
    uint32_t tmp[2048 / 4];
    uint32_t *pos = block->data;
    size_t left = block->compressed_size;
    z_stream z;
    int res;

    memset(&z, 0, sizeof(z_stream));
    z.next_in = (Bytef *)tmp;
    z.avail_in = sizeof(tmp);

    res = inflateInit2(&z, -MAX_WBITS);
    if(res != Z_OK)
    {
      debug("--EXTRAM-- inflateInit failed @ %p\n", (void *)block);
      goto err;
    }

    z.next_out = buffer;
    z.avail_out = len;

    while((res == Z_OK || res == Z_BUF_ERROR) && left)
    {
      size_t sz = MIN(left, sizeof(tmp));
      left -= sz;
      if(!extram_cpy(tmp, pos, sz))
      {
        inflateEnd(&z);
        goto err;
      }
      pos += sz >> 2;

      z.next_in = (Bytef *)tmp;
      z.avail_in = sz;
      res = inflate(&z, 0);
    }
    inflateEnd(&z);

    if(res != Z_STREAM_END || z.total_out != len)
    {
      debug("--EXTRAM-- inflate failed @ %p with code %d\n", (void *)block, res);
      goto err;
    }
    trace("--EXTRAM--   decompressed block of size %zu to %zu\n",
     (size_t)z.total_in, (size_t)z.total_out);
  }
  else
  {
    // Stored.
    if(block->compressed_size != block->uncompressed_size)
    {
      debug("--EXTRAM-- stored size inconsistent @ %p: uncompressed=%zu, compressed=%zu\n",
        (void *)block, (size_t)block->uncompressed_size, (size_t)block->compressed_size
      );
      goto err;
    }
    if(!extram_cpy(buffer, block->data, alloc_size))
      goto err;
  }

  checksum = adler32_z(0L, NULL, 0);
  checksum = adler32_z(checksum, buffer, len);
  if(checksum != block->adler32)
  {
    debug("--EXTRAM-- checksum fail @ %p (expected %08zx, got %08zx)\n",
      (void *)block, (size_t)block->adler32, (size_t)checksum
    );
    goto err;
  }

  if(flags & EXTRAM_PLATFORM_ALLOC)
  {
    platform_extram_free(*src);
  }
  else
    free(*src);

  *src = (void *)buffer;
  return true;

err:
  free(buffer);
  return false;
}

/**
 * Move the board's memory from normal RAM to extra RAM.
 *
 * @param  board  board to send to extra RAM.
 * @param  file   __FILE__ of invoking call (via macro).
 * @param  line   __LINE__ of invoking call (via macro).
 */
void real_store_board_to_extram(struct board *board, const char *file, int line)
{
  size_t board_size = board->board_width * board->board_height;
  struct robot **robot_list = board->robot_list;
  int i;

  if(board->is_extram)
  {
    warn("--EXTRAM-- board %p is already in extram! (%s:%d)\n", (void *)board, file, line);
    return;
  }

  trace("--EXTRAM-- storing board %p (%s:%d)\n", (void *)board, file, line);
  board->is_extram = true;

  // Layer data.
  if(!store_buffer_to_extram((void **)&board->level_id, board_size))
    goto err;
  if(!store_buffer_to_extram((void **)&board->level_param, board_size))
    goto err;
  if(!store_buffer_to_extram((void **)&board->level_color, board_size))
    goto err;
  if(!store_buffer_to_extram((void **)&board->level_under_id, board_size))
    goto err;
  if(!store_buffer_to_extram((void **)&board->level_under_param, board_size))
    goto err;
  if(!store_buffer_to_extram((void **)&board->level_under_color, board_size))
    goto err;

  // Overlay.
  if(board->overlay_mode)
  {
    if(!store_buffer_to_extram((void **)&board->overlay, board_size))
      goto err;
    if(!store_buffer_to_extram((void **)&board->overlay_color, board_size))
      goto err;
  }

  // Robot programs and source.
  if(!robot_list)
  {
    debug("wtf\n");
    return;
  }
  for(i = 1; i <= board->num_robots; i++)
  {
    struct robot *cur_robot = robot_list[i];
    if(cur_robot)
    {
      if(cur_robot->program_bytecode)
      {
        if(!store_buffer_to_extram((void **)&cur_robot->program_bytecode,
         cur_robot->program_bytecode_length))
          goto err;
      }

#if defined(CONFIG_DEBYTECODE) || defined(CONFIG_EDITOR)
      if(cur_robot->program_source)
      {
        if(!store_buffer_to_extram((void **)&cur_robot->program_source,
         cur_robot->program_source_length))
          goto err;
      }

      if(cur_robot->command_map)
      {
        if(!store_buffer_to_extram((void **)&cur_robot->command_map,
         cur_robot->command_map_length * sizeof(struct command_mapping)))
          goto err;
      }
#endif
      clear_label_cache(cur_robot);
    }
  }
  return;

err:
  {
    char msg[81];
    snprintf(msg, ARRAY_SIZE(msg), "Failed to store board to extram at %s:%d", file, line);
    error(msg, ERROR_T_FATAL, ERROR_OPT_EXIT | ERROR_OPT_NO_HELP, 0);
  }
}

/**
 * Move the board's memory from extra RAM to normal RAM.
 *
 * @param  board  board to retrieve from extra RAM.
 * @param  file   __FILE__ of invoking call (via macro).
 * @param  line   __LINE__ of invoking call (via macro).
 */
void real_retrieve_board_from_extram(struct board *board, const char *file, int line)
{
  size_t board_size = board->board_width * board->board_height;
  struct robot **robot_list = board->robot_list;
  int i;

  if(!board->is_extram)
  {
    warn("--EXTRAM-- board %p isn't in extram! (%s:%d)\n", (void *)board, file, line);
    return;
  }

  trace("--EXTRAM-- retrieving board %p (%s:%d)\n", (void *)board, file, line);
  board->is_extram = false;

  // Layer data.
  if(!retrieve_buffer_from_extram((void **)&board->level_id, board_size))
    goto err;
  if(!retrieve_buffer_from_extram((void **)&board->level_param, board_size))
    goto err;
  if(!retrieve_buffer_from_extram((void **)&board->level_color, board_size))
    goto err;
  if(!retrieve_buffer_from_extram((void **)&board->level_under_id, board_size))
    goto err;
  if(!retrieve_buffer_from_extram((void **)&board->level_under_param, board_size))
    goto err;
  if(!retrieve_buffer_from_extram((void **)&board->level_under_color, board_size))
    goto err;

  // Overlay.
  if(board->overlay_mode)
  {
    if(!retrieve_buffer_from_extram((void **)&board->overlay, board_size))
      goto err;
    if(!retrieve_buffer_from_extram((void **)&board->overlay_color, board_size))
      goto err;
  }

  // Robot programs and source.
  if(!robot_list)
  {
    debug("wtf\n");
    return;
  }
  for(i = 1; i <= board->num_robots; i++)
  {
    struct robot *cur_robot = robot_list[i];
    if(cur_robot)
    {
      if(cur_robot->program_bytecode)
      {
        if(!retrieve_buffer_from_extram((void **)&cur_robot->program_bytecode,
         cur_robot->program_bytecode_length))
          goto err;
      }

#if defined(CONFIG_DEBYTECODE) || defined(CONFIG_EDITOR)
      if(cur_robot->program_source)
      {
        if(!retrieve_buffer_from_extram((void **)&cur_robot->program_source,
         cur_robot->program_source_length))
          goto err;
      }

      if(cur_robot->command_map)
      {
        if(!retrieve_buffer_from_extram((void **)&cur_robot->command_map,
         cur_robot->command_map_length * sizeof(struct command_mapping)))
          goto err;
      }
#endif
      clear_label_cache(cur_robot);
      cache_robot_labels(cur_robot);
    }
  }
  return;

err:
  {
    char msg[81];
    snprintf(msg, ARRAY_SIZE(msg), "Failed to load board from extram at %s:%d", file, line);
    error(msg, ERROR_T_FATAL, ERROR_OPT_EXIT | ERROR_OPT_NO_HELP, 0);
  }
}

#ifdef CONFIG_EDITOR

static boolean get_extram_buffer_usage(const void *buffer, size_t *compressed,
 size_t *uncompressed)
{
  const struct extram_block *block = (const struct extram_block *)buffer;
  if(block->id != EXTRAM_ID)
    return false;

  *compressed += extram_block_size(block->compressed_size);
  *uncompressed += block->uncompressed_size;
  return true;
}

/**
 * Get total actual extra RAM usage (compressed) and total uncompressed
 * extra RAM size data for a board.
 *
 * @param  board          board to get extra RAM statistics for.
 * @param  compressed     pointer to write total actual RAM usage to.
 * @param  uncompressed   pointer to write total stored data size to.
 * @return                `true` on success, otherwise `false`.
 */
boolean board_extram_usage(struct board *board, size_t *compressed,
 size_t *uncompressed)
{
  struct robot **robot_list;
  int i;

#ifdef USE_PLATFORM_EXTRAM_STORE
  /* The headers are stored in non-mapped RAM and not really accessible. */
  return false;
#endif

  if(!board || !compressed | !uncompressed)
    return false;

  *compressed = 0;
  *uncompressed = 0;

  if(!board->is_extram)
    return true;

  if(!get_extram_buffer_usage(board->level_id, compressed, uncompressed))
    return false;
  if(!get_extram_buffer_usage(board->level_param, compressed, uncompressed))
    return false;
  if(!get_extram_buffer_usage(board->level_color, compressed, uncompressed))
    return false;
  if(!get_extram_buffer_usage(board->level_under_id, compressed, uncompressed))
    return false;
  if(!get_extram_buffer_usage(board->level_under_param, compressed, uncompressed))
    return false;
  if(!get_extram_buffer_usage(board->level_under_color, compressed, uncompressed))
    return false;

  if(board->overlay_mode)
  {
    if(!get_extram_buffer_usage(board->overlay, compressed, uncompressed))
      return false;
    if(!get_extram_buffer_usage(board->overlay_color, compressed, uncompressed))
      return false;
  }

  robot_list = board->robot_list;
  for(i = 1; i <= board->num_robots; i++)
  {
    struct robot *cur_robot = robot_list[i];
    if(cur_robot)
    {
      if(cur_robot->program_bytecode)
        if(!get_extram_buffer_usage(cur_robot->program_bytecode,
         compressed, uncompressed))
          return false;

#if defined(CONFIG_DEBYTECODE) || defined(CONFIG_EDITOR)
      if(cur_robot->program_source)
        if(!get_extram_buffer_usage(cur_robot->program_source,
         compressed, uncompressed))
          return false;

      if(cur_robot->command_map)
        if(!get_extram_buffer_usage(cur_robot->command_map,
         compressed, uncompressed))
          return false;
#endif
    }
  }
  return true;
}

#endif /* CONFIG_EDITOR */
