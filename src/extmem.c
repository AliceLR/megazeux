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

#ifndef EXTRAM_DEFLATE_THRESHOLD
/* Minimum size (in bytes) for memory blocks to be deflated. */
#define EXTRAM_DEFLATE_THRESHOLD 1024
#endif

#ifndef EXTRAM_DEFLATE_BUFFER
/* Size (in uint32_t) of extra memory deflate buffer. */
#define EXTRAM_DEFLATE_BUFFER (4096 / sizeof(uint32_t))
#endif

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

struct extram_data
{
  z_stream z;
  int status;
  boolean initialized;
};

/**
 * Initialize a deflate stream.
 */
static boolean extram_deflate_init(struct extram_data *data)
{
  int window_bits = -MAX_WBITS;
  int mem_level = 8;

  if(data->status && data->status != Z_OK)
    return false;

  if(data->initialized)
  {
    data->status = deflateReset(&data->z);
    return (data->status == Z_OK);
  }

  while(mem_level > 4 || window_bits < -8)
  {
    int res = deflateInit2(&data->z, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
     window_bits, mem_level, Z_DEFAULT_STRATEGY);

    data->status = res;
    if(res == Z_OK)
    {
      data->initialized = true;
      return true;
    }

    /* Try progressively worse settings to see if one will stick... */
    /* This works out to (1 << (mem_level + 9)) = 8192 minimum. */
    if(mem_level > 4)
      mem_level--;
    /* This works out to (1 << (window_bits + 2)) = 2048 minimum. */
    if(window_bits < -8)
      window_bits++;
  }
  return false;
}

/**
 * Destroy a deflate stream.
 */
static void extram_deflate_destroy(struct extram_data *data)
{
  if(data->initialized)
    deflateEnd(&data->z);
}

/**
 * Initialize an inflate stream.
 */
static boolean extram_inflate_init(struct extram_data *data)
{
  if(data->status && data->status != Z_OK)
    return false;

  if(data->initialized)
  {
    data->status = inflateReset(&data->z);
    return (data->status == Z_OK);
  }

  /* This needs to be -MAX_WBITS (32k buffer). */
  data->status = inflateInit2(&data->z, -MAX_WBITS);
  if(data->status == Z_OK)
  {
    data->initialized = true;
    return true;
  }
  return false;
}

/**
 * Destroy an inflate stream.
 */
static void extram_inflate_destroy(struct extram_data *data)
{
  if(data->initialized)
    inflateEnd(&data->z);
}

/**
 * Perform a 32-bit aligned copy. Both the source and the dest buffers must be
 * 32-bit aligned. The destination buffer additionally should be large enough
 * to fit `len` rounded up to a multiple of 4 bytes (use `extram_alloc_size`).
 */
static boolean extram_copy(void * RESTRICT dest, const void *src, size_t len)
{
  const uint32_t *s32 = src;
  uint32_t *d32 = dest;
  size_t len32;
  size_t i;

  if(((size_t)src & 3) || ((size_t)dest & 3))
  {
    debug("--EXTRAM-- bad copy parameters: %p %p %zu!\n", dest, src, len);
    return false;
  }

  len32 = len >> 2;
  for(i = 0; i < len32; i++)
    d32[i] = s32[i];

  if(len & 3)
  {
    const uint8_t *src8 = (const uint8_t *)src;
    uint32_t buffer = 0;
    uint32_t pos;
    i <<= 2;

#if PLATFORM_BYTE_ORDER == PLATFORM_LIL_ENDIAN
    for(pos = 0; i < len; i++, pos += 8)
      buffer |= src8[i] << pos;
#else
    for(pos = 24; i < len; i++, pos -= 8)
      buffer |= src8[i] << pos;
#endif
    d32[len >> 2] = buffer;
  }
  return true;
}

/**
 * Get the rounded allocation size for a buffer.
 */
static size_t extram_alloc_size(size_t len)
{
  return (len + 3) & ~3;
}

/**
 * Get the extra memory block size for an input buffer.
 */
static size_t extram_block_size(size_t len)
{
  return sizeof(struct extram_block) + extram_alloc_size(len);
}

/**
 * Send a buffer to extra memory.
 *
 * @param  data structure including z_stream information.
 * @param  _src pointer to buffer to store to extra RAM.
 * @param  len  size of buffer to store to extra RAM.
 * @return      `true` on success, otherwise `false`.
 */
static boolean store_buffer_to_extram(struct extram_data *data,
 char **_src, size_t len)
{
  struct extram_block *block;
  uint8_t *src = (uint8_t *)*_src;
  void *ptr;
  uint32_t compressed_size = len;
  uint32_t flags = EXTRAM_PLATFORM_ALLOC;
  size_t alloc_size;

  trace("--EXTRAM-- store_buffer_to_extram %p %zu\n", src, len);

  if(len >= EXTRAM_DEFLATE_THRESHOLD)
  {
    // Project compressed size...
    data->z.next_in = src;
    data->z.avail_in = len;

    if(extram_deflate_init(data))
    {
      compressed_size = deflateBound(&data->z, len);
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
    uint32_t tmp[EXTRAM_DEFLATE_BUFFER];
    uint32_t *pos = block->data;
    size_t sz;
    size_t new_alloc_size;
    int res = Z_OK;

    data->z.next_in = (Bytef *)src;
    data->z.avail_in = len;

    while(res != Z_STREAM_END)
    {
      data->z.next_out = (Bytef *)tmp;
      data->z.avail_out = sizeof(tmp);

      res = deflate(&data->z, Z_FINISH);

      if(res != Z_OK && res != Z_BUF_ERROR && res != Z_STREAM_END)
      {
        debug("--EXTRAM-- deflate failed with code %d\n", res);
        goto err;
      }

      sz = sizeof(tmp) - data->z.avail_out;
      if(!extram_copy(pos, tmp, sz))
        goto err;

      pos += sz >> 2;
    }

    trace("--EXTRAM--   compressed_size=%zu\n", (size_t)data->z.total_out);

    block->compressed_size = data->z.total_out;

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

    if(!extram_copy(block->data, src, len))
      goto err;
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

  return false;
}

/**
 * Retrieve a buffer from extra memory.
 *
 * @param  data structure including z_stream information.
 * @param  src  pointer to buffer to retrieve from extra RAM.
 * @param  len  length of buffer to retreieve from extra RAM.
 * @return      `true` on success, otherwise `false`.
 */
static boolean retrieve_buffer_from_extram(struct extram_data *data,
 char **src, size_t len)
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

  alloc_size = extram_alloc_size(len);
  buffer = cmalloc(alloc_size);

  if(flags & EXTRAM_DEFLATE)
  {
    // Compressed.
    uint32_t tmp[EXTRAM_DEFLATE_BUFFER];
    uint32_t *pos = block->data;
    size_t left = extram_alloc_size(block->compressed_size);
    int res = Z_OK;

    data->z.next_in = (Bytef *)tmp;
    data->z.avail_in = sizeof(tmp);

    if(!extram_inflate_init(data))
    {
      debug("--EXTRAM-- inflateInit failed @ %p\n", (void *)block);
      goto err;
    }

    data->z.next_out = buffer;
    data->z.avail_out = len;

    while((res == Z_OK || res == Z_BUF_ERROR) && left)
    {
      size_t sz = MIN(left, sizeof(tmp));
      left -= sz;
      if(!extram_copy(tmp, pos, sz))
        goto err;

      pos += sz >> 2;

      data->z.next_in = (Bytef *)tmp;
      data->z.avail_in = sz;
      res = inflate(&data->z, 0);
    }

    if(res != Z_STREAM_END || data->z.total_out != len)
    {
      debug("--EXTRAM-- inflate failed @ %p with code %d\n", (void *)block, res);
      goto err;
    }
    trace("--EXTRAM--   decompressed block of size %zu to %zu\n",
     (size_t)data->z.total_in, (size_t)data->z.total_out);
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
    if(!extram_copy(buffer, block->data, alloc_size))
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
    platform_extram_free(block);
  }
  else
    free(block);

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
  struct extram_data data;
  int i;

  if(board->is_extram)
  {
    warn("--EXTRAM-- board %p is already in extram! (%s:%d)\n", (void *)board, file, line);
    return;
  }

  trace("--EXTRAM-- storing board %p (%s:%d)\n", (void *)board, file, line);
  board->is_extram = true;

  memset(&data, 0, sizeof(struct extram_data));

  // Layer data.
  if(!store_buffer_to_extram(&data, &board->level_id, board_size))
    goto err;
  if(!store_buffer_to_extram(&data, &board->level_param, board_size))
    goto err;
  if(!store_buffer_to_extram(&data, &board->level_color, board_size))
    goto err;
  if(!store_buffer_to_extram(&data, &board->level_under_id, board_size))
    goto err;
  if(!store_buffer_to_extram(&data, &board->level_under_param, board_size))
    goto err;
  if(!store_buffer_to_extram(&data, &board->level_under_color, board_size))
    goto err;

  // Overlay.
  if(board->overlay_mode)
  {
    if(!store_buffer_to_extram(&data, &board->overlay, board_size))
      goto err;
    if(!store_buffer_to_extram(&data, &board->overlay_color, board_size))
      goto err;
  }

  // Robot programs and source.
  for(i = 1; robot_list && i <= board->num_robots; i++)
  {
    struct robot *cur_robot = robot_list[i];
    if(cur_robot)
    {
      if(cur_robot->program_bytecode)
      {
        if(!store_buffer_to_extram(&data, &cur_robot->program_bytecode,
         cur_robot->program_bytecode_length))
          goto err;
      }

#if defined(CONFIG_DEBYTECODE) || defined(CONFIG_EDITOR)
      if(cur_robot->program_source)
      {
        if(!store_buffer_to_extram(&data, &cur_robot->program_source,
         cur_robot->program_source_length))
          goto err;
      }

      if(cur_robot->command_map)
      {
        if(!store_buffer_to_extram(&data, (char **)&cur_robot->command_map,
         cur_robot->command_map_length * sizeof(struct command_mapping)))
          goto err;
      }
#endif
      clear_label_cache(cur_robot);
    }
  }
  extram_deflate_destroy(&data);
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
  struct extram_data data;
  int i;

  if(!board->is_extram)
  {
    warn("--EXTRAM-- board %p isn't in extram! (%s:%d)\n", (void *)board, file, line);
    return;
  }

  trace("--EXTRAM-- retrieving board %p (%s:%d)\n", (void *)board, file, line);
  board->is_extram = false;

  memset(&data, 0, sizeof(struct extram_data));

  // Layer data.
  if(!retrieve_buffer_from_extram(&data, &board->level_id, board_size))
    goto err;
  if(!retrieve_buffer_from_extram(&data, &board->level_param, board_size))
    goto err;
  if(!retrieve_buffer_from_extram(&data, &board->level_color, board_size))
    goto err;
  if(!retrieve_buffer_from_extram(&data, &board->level_under_id, board_size))
    goto err;
  if(!retrieve_buffer_from_extram(&data, &board->level_under_param, board_size))
    goto err;
  if(!retrieve_buffer_from_extram(&data, &board->level_under_color, board_size))
    goto err;

  // Overlay.
  if(board->overlay_mode)
  {
    if(!retrieve_buffer_from_extram(&data, &board->overlay, board_size))
      goto err;
    if(!retrieve_buffer_from_extram(&data, &board->overlay_color, board_size))
      goto err;
  }

  // Robot programs and source.
  for(i = 1; robot_list && i <= board->num_robots; i++)
  {
    struct robot *cur_robot = robot_list[i];
    if(cur_robot)
    {
      if(cur_robot->program_bytecode)
      {
        if(!retrieve_buffer_from_extram(&data, &cur_robot->program_bytecode,
         cur_robot->program_bytecode_length))
          goto err;
      }

#if defined(CONFIG_DEBYTECODE) || defined(CONFIG_EDITOR)
      if(cur_robot->program_source)
      {
        if(!retrieve_buffer_from_extram(&data, &cur_robot->program_source,
         cur_robot->program_source_length))
          goto err;
      }

      if(cur_robot->command_map)
      {
        if(!retrieve_buffer_from_extram(&data, (char **)&cur_robot->command_map,
         cur_robot->command_map_length * sizeof(struct command_mapping)))
          goto err;
      }
#endif
      clear_label_cache(cur_robot);
      cache_robot_labels(cur_robot);
    }
  }
  extram_inflate_destroy(&data);
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
