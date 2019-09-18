/* MegaZeux
 *
 * Copyright (C) 2017 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __ZIP_H
#define __ZIP_H

#include "compat.h"

__M_BEGIN_DECLS

#include <stdint.h>

// This needs to stay self-sufficient.
// Don't use core features, don't extern anything.

#include "memfile.h"
#include "vfs.h"

// Currently supported methods are 0 (Store) and 8 (DEFLATE)
enum zip_compression_method
{
  ZIP_M_NONE                    = 0, // Store
  ZIP_M_SHRUNK                  = 1,
  ZIP_M_REDUCED_1               = 2, // Reduced with compression factor 1
  ZIP_M_REDUCED_2               = 3, // Reduced with compression factor 2
  ZIP_M_REDUCED_3               = 4, // Reduced with compression factor 3
  ZIP_M_REDUCED_4               = 5, // Reduced with compression factor 4
  ZIP_M_IMPLODED                = 6,
  ZIP_M_DEFLATE                 = 8,
  ZIP_M_DEFLATE64               = 9,
  ZIP_M_DCL_IMPLODING           = 10, // Old IBM TERSE
  ZIP_M_BZIP2                   = 12,
  ZIP_M_LZMA                    = 14,
  ZIP_M_IBM_TERSE               = 18,
  ZIP_M_LZ77                    = 19,
  ZIP_M_WAVPACK                 = 97,
  ZIP_M_PPMD                    = 98,
};

enum zip_general_purpose_flag
{
  ZIP_F_ENCRYPTED           = (1<<0), // Indicates that a file is encrypted.
  ZIP_F_COMPRESSION_1       = (1<<1), // Varies.
  ZIP_F_COMPRESSION_2       = (1<<2), // Varies.
  ZIP_F_DATA_DESCRIPTOR     = (1<<3), // Sizes and CRC32 stored after file data.
  ZIP_F_ENHANCED_DEFLATE    = (1<<4), // Reserved for Deflate64.
  ZIP_F_COMPRESSED_PATCHED  = (1<<5), // ?????
  ZIP_F_STRONG_ENCRYPTION   = (1<<6), // Indicates strong encryption is used.
  // bit 07 unused
  // bit 08 unused
  // bit 09 unused
  // bit 10 unused
  ZIP_F_LANGUAGE_ENCODING   = (1<<11), // Indicates UTF-8 filename/comments.
  // bit 12 reserved
  ZIP_F_MASKED_HEADER_DATA  = (1<<13), // See: strong encryption.
  // bit 14 reserved
  // bit 15 reserved
};

// These flags are allowed for all DEFLATE and stored files we care about.
#define ZIP_F_ALLOWED   (\
  ZIP_F_DATA_DESCRIPTOR |\
  ZIP_F_COMPRESSION_1   |\
  ZIP_F_COMPRESSION_2   )

enum zip_error
{
  ZIP_SUCCESS = 0,
  ZIP_EOF,
  ZIP_NULL,
  ZIP_NULL_BUF,
  ZIP_STAT_ERROR,
  ZIP_SEEK_ERROR,
  ZIP_READ_ERROR,
  ZIP_WRITE_ERROR,
  ZIP_INVALID_READ_IN_WRITE_MODE,
  ZIP_INVALID_WRITE_IN_READ_MODE,
  ZIP_INVALID_FILE_READ_UNINITIALIZED,
  ZIP_INVALID_FILE_READ_IN_STREAM_MODE,
  ZIP_INVALID_FILE_WRITE_IN_STREAM_MODE,
  ZIP_INVALID_STREAM_READ,
  ZIP_INVALID_STREAM_WRITE,
  ZIP_NOT_MEMORY_ARCHIVE,
  ZIP_NO_EOCD,
  ZIP_NO_CENTRAL_DIRECTORY,
  ZIP_INCOMPLETE_CENTRAL_DIRECTORY,
  ZIP_UNSUPPORTED_MULTIPLE_DISKS,
  ZIP_UNSUPPORTED_FLAGS,
  ZIP_UNSUPPORTED_COMPRESSION,
  ZIP_UNSUPPORTED_ZIP64,
  ZIP_UNSUPPORTED_DEFLATE_STREAM,
  ZIP_MISSING_LOCAL_HEADER,
  ZIP_HEADER_MISMATCH,
  ZIP_CRC32_MISMATCH,
  ZIP_INFLATE_FAILED,
  ZIP_DEFLATE_FAILED,
};

struct zip_file_header
{
  uint32_t mzx_prop_id;
  uint8_t mzx_board_id;
  uint8_t mzx_robot_id;
  uint16_t flags;
  uint16_t method;
  uint32_t crc32;
  uint32_t compressed_size;
  uint32_t uncompressed_size;
  uint16_t file_name_length;
  uint32_t offset;
  char *file_name;
};

struct zip_archive
{
  uint8_t mode;

  uint16_t pos;
  uint16_t num_files;
  uint16_t files_alloc;
  uint32_t size_central_directory;
  uint32_t offset_central_directory;

  uint32_t running_file_name_length;

  struct zip_file_header **files;
  struct zip_file_header *streaming_file;
  uint32_t stream_crc_position;
  uint32_t stream_left;
  uint32_t stream_crc32;

  uint32_t start_in_file;
  uint32_t end_in_file;

  enum zip_error read_file_error;
  enum zip_error read_stream_error;
  enum zip_error write_file_error;
  enum zip_error write_stream_error;

  vfile *vf;

  boolean is_memory;
  void **external_buffer;
  size_t *external_buffer_size;
};

UTILS_LIBSPEC int zip_bound_data_usage(char *src, int srcLen);
UTILS_LIBSPEC int zip_bound_total_header_usage(int num_files, int max_name_size);

UTILS_LIBSPEC enum zip_error zread(void *destBuf, size_t readLen,
 struct zip_archive *zp);

UTILS_LIBSPEC enum zip_error zip_get_next_name(struct zip_archive *zp,
 char *name, int name_buffer_size);

UTILS_LIBSPEC enum zip_error zip_get_next_prop(struct zip_archive *zp,
 unsigned int *prop_id, unsigned int *board_id, unsigned int *robot_id);

UTILS_LIBSPEC enum zip_error zip_get_next_method(struct zip_archive *zp,
 unsigned int *method);

UTILS_LIBSPEC enum zip_error zip_get_next_uncompressed_size(
 struct zip_archive *zp, size_t *u_size);

UTILS_LIBSPEC enum zip_error zip_read_open_file_stream(struct zip_archive *zp,
 size_t *destLen);

UTILS_LIBSPEC enum zip_error zip_read_close_stream(struct zip_archive *zp);

UTILS_LIBSPEC enum zip_error zip_read_open_mem_stream(struct zip_archive *zp,
 struct memfile *mf);

UTILS_LIBSPEC enum zip_error zip_rewind(struct zip_archive *zp);

UTILS_LIBSPEC enum zip_error zip_skip_file(struct zip_archive *zp);

UTILS_LIBSPEC enum zip_error zip_read_file(struct zip_archive *zp,
 void *destBuf, size_t destLen, size_t *readLen);

UTILS_LIBSPEC enum zip_error zwrite(const void *src, size_t srcLen,
 struct zip_archive *zp);

UTILS_LIBSPEC enum zip_error zip_write_open_file_stream(struct zip_archive *zp,
 const char *name, int method);

UTILS_LIBSPEC enum zip_error zip_write_close_stream(struct zip_archive *zp);

UTILS_LIBSPEC enum zip_error zip_write_open_mem_stream(struct zip_archive *zp,
 struct memfile *mf, const char *name);

UTILS_LIBSPEC enum zip_error zip_write_close_mem_stream(struct zip_archive *zp,
 struct memfile *mf);

UTILS_LIBSPEC enum zip_error zip_write_file(struct zip_archive *zp,
 const char *name, const void *src, size_t srcLen, int method);

UTILS_LIBSPEC enum zip_error zip_close(struct zip_archive *zp,
 size_t *final_length);

UTILS_LIBSPEC struct zip_archive *zip_open_fp_read(FILE *fp);
UTILS_LIBSPEC struct zip_archive *zip_open_fp_write(FILE *fp);
UTILS_LIBSPEC struct zip_archive *zip_open_file_read(const char *file_name);
UTILS_LIBSPEC struct zip_archive *zip_open_file_write(const char *file_name);
UTILS_LIBSPEC struct zip_archive *zip_open_mem_read(const void *src, size_t len);
UTILS_LIBSPEC struct zip_archive *zip_open_mem_write(void *src, size_t len,
 size_t start_pos);
UTILS_LIBSPEC struct zip_archive *zip_open_mem_write_ext(void **external_buffer,
 size_t *external_buffer_size, size_t start_pos);

__M_END_DECLS

#endif //__ZIP_H
