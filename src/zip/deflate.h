/* MegaZeux
 *
 * Copyright (C) 2020 Alice Rowan <petrifiedrowan@gmail.com>
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

/**
 * Deflate (zip compression method 8) compressor and decompressor via zlib.
 */

#ifndef __ZIP_DEFLATE_H
#define __ZIP_DEFLATE_H

#include "../compat.h"

__M_BEGIN_DECLS

#include <assert.h>
#include <zlib.h>

#include "zip_stream.h"

struct deflate_stream
{
  struct zip_stream zs;
  z_stream z;
};

static inline void inflate_open(struct zip_stream *zs, uint16_t m, uint16_t f)
{
  assert(ZIP_STREAM_ALLOC_SIZE >= sizeof(struct deflate_stream));
  memset(zs, 0, sizeof(struct deflate_stream));
}

static inline void deflate_open(struct zip_stream *zs, uint16_t m, uint16_t f)
{
  assert(ZIP_STREAM_ALLOC_SIZE >= sizeof(struct deflate_stream));
  memset(zs, 0, sizeof(struct deflate_stream));
  zs->is_compression_stream = true;
}

static inline void deflate_close(struct zip_stream *zs,
 size_t *final_input_length, size_t *final_output_length)
{
  if(final_input_length)
    *final_input_length = zs->final_input_length;

  if(final_output_length)
    *final_output_length = zs->final_output_length;
}

static inline boolean deflate_input(struct zip_stream *zs, const void *src,
 size_t src_len)
{
  struct deflate_stream *ds = ((struct deflate_stream *)zs);
  ds->z.next_in = (Bytef *)src;
  ds->z.avail_in = (uInt)src_len;
  return true;
}

static inline boolean deflate_output(struct zip_stream *zs, void *dest,
 size_t dest_len)
{
  struct deflate_stream *ds = ((struct deflate_stream *)zs);
  ds->z.next_out = (Bytef *)dest;
  ds->z.avail_out = (uInt)dest_len;
  return true;
}

static inline enum zip_error inflate_init(struct zip_stream *zs)
{
  struct deflate_stream *ds = ((struct deflate_stream *)zs);

  if(zs->is_initialized)
    return ZIP_SUCCESS;

  if(ds->z.avail_in)
  {
    // This is a raw deflate stream, so use -MAX_WBITS.
    inflateInit2(&(ds->z), -MAX_WBITS);
    zs->is_initialized = true;
    return ZIP_SUCCESS;
  }
  return ZIP_INPUT_EMPTY;
}

static inline enum zip_error inflate_file(struct zip_stream *zs)
{
  struct deflate_stream *ds = ((struct deflate_stream *)zs);

  enum zip_error result = inflate_init(zs);
  int err;

  if(result)
    return result;

  if(!ds->z.avail_out)
    return ZIP_OUTPUT_FULL;

  err = inflate(&(ds->z), Z_FINISH);

  zs->final_input_length = ds->z.total_in;
  zs->final_output_length = ds->z.total_out;

  inflateEnd(&(ds->z));

  if(err == Z_STREAM_END)
    return ZIP_SUCCESS;

  return ZIP_DECOMPRESS_FAILED;
}

static inline enum zip_error deflate_init(struct zip_stream *zs)
{
  struct deflate_stream *ds = ((struct deflate_stream *)zs);

  if(!zs->is_initialized)
  {
    // This is a raw deflate stream, so use -MAX_WBITS.
    // Note: aside from the windowbits, these are all defaults.
    deflateInit2(&(ds->z), Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS,
     8, Z_DEFAULT_STRATEGY);
    zs->is_initialized = true;
  }
  return ZIP_SUCCESS;
}

static inline enum zip_error deflate_file(struct zip_stream *zs)
{
  struct deflate_stream *ds = ((struct deflate_stream *)zs);

  enum zip_error result = deflate_init(zs);
  int err;

  if(result)
    return result;

  if(!ds->z.avail_in)
    return ZIP_INPUT_EMPTY;

  if(!ds->z.avail_out)
    return ZIP_OUTPUT_FULL;

  err = deflate(&(ds->z), Z_FINISH);

  zs->final_input_length = ds->z.total_in;
  zs->final_output_length = ds->z.total_out;

  deflateEnd(&(ds->z));
  inflateEnd(&(ds->z));

  if(err == Z_STREAM_END)
    return ZIP_SUCCESS;

  return ZIP_DECOMPRESS_FAILED;
}

__M_END_DECLS

#endif /* __ZIP_DEFLATE_H */
