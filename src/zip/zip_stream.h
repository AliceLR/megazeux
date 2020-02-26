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

#ifndef __ZIP_STREAM_H
#define __ZIP_STREAM_H

#include "../compat.h"

__M_BEGIN_DECLS

#include <inttypes.h>
#include "../zip.h"

/**
 * Data for an active ZIP (de)compression stream.
 */
struct zip_stream_data
{
  // Current position and remaining size of the input buffer.
  const uint8_t *input_buffer;
  size_t input_length;

  // Current position and remaining size of the output buffer.
  uint8_t *output_buffer;
  size_t output_length;

  size_t final_input_length;
  size_t final_output_length;

  // Has this stream been initialized?
  boolean is_initialized;

  // Is this stream finished?
  boolean finished;

  // Is this a compression stream? (false for a decompression stream)
  boolean is_compression_stream;
};

// zip_stream_data structs should be allocated with at least this much extra
// space for specific compression method data.
#define ZIP_STREAM_DATA_PADDING 128
#define ZIP_STREAM_DATA_ALLOC_SIZE \
 (sizeof(struct zip_stream_data) + ZIP_STREAM_DATA_PADDING)

/**
 * Describes ZIP stream functions for a particular (de)compresson method.
 */
struct zip_method_handler
{
  // Open a decompression stream.
  void (*decompress_open)(struct zip_stream_data *, uint16_t method,
   uint16_t flags);

  // Open a compression stream.
  void (*compress_open)(struct zip_stream_data *, uint16_t method,
   uint16_t flags);

  // Frees any memory allocated by the stream. Does not free the zip_stream.
  void (*close)(struct zip_stream_data *, size_t *final_input_length,
   size_t *final_output_length);

  // Provide the next input buffer for a stream.
  boolean (*input)(struct zip_stream_data *, const void *src, size_t src_len);

  // Provide the next output buffer for a stream.
  boolean (*output)(struct zip_stream_data *, void *dest, size_t dest_len);

  // Decompress, treating the input and output as an entire file.
  enum zip_error (*decompress_file)(struct zip_stream_data *);

  // Decompress until the input is exhausted or the output is full.
  // Can be called multiple times.
  enum zip_error (*decompress_block)(struct zip_stream_data *);

  // Compress, treating the input and output as an entire file.
  enum zip_error (*compress_file)(struct zip_stream_data *);

  // Compress until the input is exhausted or the output is full.
  // Can be called multiple times.
  enum zip_error (*compress_block)(struct zip_stream_data *);
};

#define MAX_SUPPORTED_METHOD ZIP_M_DEFLATE64

/**
 * ZIP stream function registry for supported (de)compression methods.
 */
extern struct zip_method_handler *zip_method_handlers[MAX_SUPPORTED_METHOD + 1];

__M_END_DECLS

#endif /* __ZIP_STREAM_H */
