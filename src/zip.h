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

#include "platform.h"
#include "world_struct.h"

#define ZIP_M_NO_COMPRESSION 0
#define ZIP_M_DEFLATE 8

#define ZIP_F_DATA_DESCRIPTOR 1<<3

enum zip_error {
  ZIP_SUCCESS = 0,
  ZIP_EOF,
  ZIP_NULL,
  ZIP_NULL_BUF,
  ZIP_SEEK_ERROR,
  ZIP_READ_ERROR,
  ZIP_WRITE_ERROR,
  ZIP_OPEN_ERROR,
  ZIP_ALLOC_MORE_SPACE,
  ZIP_INVALID_WHILE_CLOSING,
  ZIP_INVALID_READ_IN_WRITE_MODE,
  ZIP_INVALID_WRITE_IN_READ_MODE,
  ZIP_INVALID_RAW_READ_IN_FILE_MODE,
  ZIP_INVALID_RAW_WRITE_IN_FILE_MODE,
  ZIP_INVALID_DIRECTORY_READ_IN_FILE_MODE,
  ZIP_INVALID_FILE_READ_IN_RAW_MODE,
  ZIP_INVALID_FILE_READ_IN_STREAM_MODE,
  ZIP_INVALID_FILE_WRITE_IN_STREAM_MODE,
  ZIP_INVALID_STREAM_READ,
  ZIP_INVALID_STREAM_WRITE,
  ZIP_INVALID_EXPAND,
  ZIP_NO_EOCD,
  ZIP_NO_CENTRAL_DIRECTORY,
  ZIP_INCOMPLETE_CENTRAL_DIRECTORY,
  ZIP_UNSUPPORTED_MULTIPLE_DISKS,
  ZIP_UNSUPPORTED_FLAGS,
  ZIP_UNSUPPORTED_COMPRESSION,
  ZIP_UNSUPPORTED_ZIP64,
  ZIP_UNSUPPORTED_DEFLATE_STREAM,
  ZIP_MISSING_LOCAL_HEADER,
  ZIP_MISSING_DATA_DESCRIPTOR,
  ZIP_HEADER_MISMATCH,
  ZIP_CRC32_MISMATCH,
  ZIP_INFLATE_FAILED,
  ZIP_DEFLATE_FAILED,
};

struct zip_file_header
{
  Uint16 flags;
  Uint16 method;
  Uint32 crc32;
  Uint32 compressed_size;
  Uint32 uncompressed_size;
  Uint16 file_name_length;
  Uint32 offset;
  char *file_name;
};

struct zip_archive
{
  Uint8 mode;
  Uint8 closing;

  Uint16 pos;
  Uint16 num_files;
  Uint16 files_alloc;
  Uint32 size_central_directory;
  Uint32 offset_central_directory;

  Uint32 running_file_name_length;

  struct zip_file_header **files;
  struct zip_file_header *streaming_file;
  Uint32 stream_left;
  Uint32 stream_crc32;

  Uint32 start_in_file;
  Uint32 end_in_file;

  void *fp;

  int (*hasspace)(size_t, void *);

  int (*vgetc)(void *);
  int (*vgetw)(void *);
  int (*vgetd)(void *);
  int (*vputc)(int, void *);
  void (*vputw)(int, void *);
  void (*vputd)(int, void *);

  int (*vread)(void *, size_t, size_t, void *);
  int (*vwrite)(void *, size_t, size_t, void *);

  int (*vseek)(void *, int, int);
  int (*vtell)(void *);
  int (*verror)(void *);

  int (*vclose)(void *);
};


int zip_bound_data_usage(char *src, int srcLen);
int zip_bound_total_header_usage(int num_files, int max_name_size);

int zgetc(struct zip_archive *zp, enum zip_error *err);
int zgetw(struct zip_archive *zp, enum zip_error *err);
int zgetd(struct zip_archive *zp, enum zip_error *err);
enum zip_error zread(char *destBuf, Uint32 readLen, struct zip_archive *zp);

enum zip_error zip_next_file_name(struct zip_archive *zp, char *name,
 int name_buffer_size);
enum zip_error zip_read_open_file_stream(struct zip_archive *zp,
 Uint32 *destLen);
enum zip_error zip_read_close_stream(struct zip_archive *zp);

enum zip_error zip_read_file(struct zip_archive *zp, char *name,
 int name_buffer_size, char **dest, Uint32 *destLen);

enum zip_error zputc(int value, struct zip_archive *zp);
enum zip_error zputw(int value, struct zip_archive *zp);
enum zip_error zputd(int value, struct zip_archive *zp);
enum zip_error zwrite(char *src, Uint32 srcLen, struct zip_archive *zp);

enum zip_error zip_write_open_file_stream(struct zip_archive *zp, char *name,
 int method);
enum zip_error zip_write_close_stream(struct zip_archive *zp);

enum zip_error zip_write_file(struct zip_archive *zp, char *name, char *src,
 Uint32 srcLen, int method);

enum zip_error zip_read_directory(struct zip_archive *zp);
enum zip_error zip_close(struct zip_archive *zp, Uint32 *final_length);

struct zip_archive *zip_open_file_read(char *file_name);
struct zip_archive *zip_open_file_write(char *file_name);
struct zip_archive *zip_open_mem_read(char *src, Uint32 len);
struct zip_archive *zip_open_mem_write(char *src, Uint32 len);

enum zip_error zip_expand(struct zip_archive *zp, char **src, Uint32 new_size);

// FIXME REMOVE
#define TEST
#ifdef TEST
void zip_test(struct world *mzx_world);
#endif //TEST

#endif //__ZIP_H
