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

#include "world_struct.h"

#define ZIP_M_NO_COMPRESSION 0
#define ZIP_M_DEFLATE 8

#define ZIP_F_DATA_DESCRIPTOR 1<<3

enum zip_error {
  ZIP_SUCCESS = 0,
  ZIP_NULL,
  ZIP_EOF,
  ZIP_SEEK_ERROR,
  ZIP_READ_ERROR,
  ZIP_WRITE_ERROR,
  ZIP_OPEN_ERROR,
  ZIP_ALLOC_MORE_SPACE,
  ZIP_INVALID_READ_IN_WRITE_MODE,
  ZIP_INVALID_WRITE_IN_READ_MODE,
  ZIP_INVALID_RAW_READ_IN_FILE_MODE,
  ZIP_INVALID_RAW_WRITE_IN_FILE_MODE,
  ZIP_INVALID_FILE_READ_IN_RAW_MODE,
  ZIP_INVALID_DIRECTORY_READ_IN_FILE_MODE,
  ZIP_INVALID_EXPAND,
  ZIP_NO_EOCD,
  ZIP_NO_CENTRAL_DIRECTORY,
  ZIP_INCOMPLETE_CENTRAL_DIRECTORY,
  ZIP_UNSUPPORTED_MULTIPLE_DISKS,
  ZIP_UNSUPPORTED_DATA_DESCRIPTOR,
  ZIP_UNSUPPORTED_FLAGS,
  ZIP_UNSUPPORTED_COMPRESSION,
  ZIP_UNSUPPORTED_ZIP64,
  ZIP_HEADER_MISMATCH,
  ZIP_CRC32_MISMATCH,
  ZIP_INFLATE_FAILED,
  ZIP_DEFLATE_FAILED,
};

struct zip_file_header
{
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
  Uint16 pos;
  Uint8 mode;
  Uint8 closing;

  Uint32 start;
  Uint32 end;

  Uint16 files_alloc;

  Uint16 num_files;
  Uint32 size_central_directory;
  Uint32 offset_central_directory;
  Uint32 dos_date_time;

  Uint32 running_file_name_length;

  struct zip_file_header **files;

  void *fp;

  int (*hasspace)(size_t, void *);

  int (*getc)(void *);
  int (*getw)(void *);
  int (*getd)(void *);
  int (*putc)(int, void *);
  void (*putw)(int, void *);
  void (*putd)(int, void *);

  int (*read)(void *, size_t, size_t, void *);
  int (*write)(void *, size_t, size_t, void *);

  int (*seek)(void *, int, int);
  int (*tell)(void *);
  int (*error)(void *);

  int (*close)(void *);
};


int zip_bound_data_usage(char *src, int srcLen);
int zip_bound_total_header_usage(int num_files, int max_name_size);

int zip_read(struct zip_archive *zp, char *dest, int *destLen);
int zip_read_file(struct zip_archive *zp, char *name, int name_buffer_size,
 char **dest, int *destLen);

int zip_write(struct zip_archive *zp, char *src, int srcLen);
int zip_write_file(struct zip_archive *zp, char *name, char *src, int srcLen,
 int method);

int zip_read_directory(struct zip_archive *zp);
int zip_close(struct zip_archive *zp, int *final_length);

struct zip_archive *zip_open_file_read(char *file_name);
struct zip_archive *zip_open_file_write(char *file_name);
struct zip_archive *zip_open_mem_read(char *src, int len);
struct zip_archive *zip_open_mem_write(char *src, int len);

int zip_expand(struct zip_archive *zp, char **src, size_t new_size);

// FIXME REMOVE
#define TEST
#ifdef TEST
void zip_test(struct world *mzx_world);
#endif //TEST

#endif //__ZIP_H
