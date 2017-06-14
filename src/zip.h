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

#include "world_struct.h"

#define ZIP_M_NONE 0
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
  ZIP_MISSING_DATA_DESCRIPTOR,
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
  uint8_t closing;

  uint16_t pos;
  uint16_t num_files;
  uint16_t files_alloc;
  uint32_t size_central_directory;
  uint32_t offset_central_directory;

  uint32_t running_file_name_length;

  struct zip_file_header **files;
  struct zip_file_header *streaming_file;
  uint32_t stream_left;
  uint32_t stream_crc32;

  uint32_t start_in_file;
  uint32_t end_in_file;

  enum zip_error read_raw_error;
  enum zip_error read_file_error;
  enum zip_error read_stream_error;
  enum zip_error write_raw_error;
  enum zip_error write_file_error;
  enum zip_error write_stream_error;

  void *fp;

  int is_memory;

  int (*hasspace)(size_t, void *);

  int (*vgetc)(void *);
  int (*vgetw)(void *);
  int (*vgetd)(void *);
  int (*vputc)(int, void *);
  void (*vputw)(int, void *);
  void (*vputd)(int, void *);

  int (*vread)(void *, size_t, size_t, void *);
  int (*vwrite)(const void *, size_t, size_t, void *);

  int (*vseek)(void *, int, int);
  int (*vtell)(void *);
  int (*verror)(void *);

  int (*vclose)(void *);
};


int zip_bound_data_usage(char *src, int srcLen);
int zip_bound_total_header_usage(int num_files, int max_name_size);

enum zip_error zseek(struct zip_archive *zp, int value, int code);

int zgetc(struct zip_archive *zp, enum zip_error *err);
int zgetw(struct zip_archive *zp, enum zip_error *err);
int zgetd(struct zip_archive *zp, enum zip_error *err);
enum zip_error zread(void *destBuf, uint32_t readLen, struct zip_archive *zp);

enum zip_error zip_get_next_name(struct zip_archive *zp,
 char *name, int name_buffer_size);

enum zip_error zip_get_next_prop(struct zip_archive *zp,
 unsigned int *prop_id, unsigned int *board_id, unsigned int *robot_id);

enum zip_error zip_get_next_uncompressed_size(struct zip_archive *zp,
 uint32_t *u_size);

enum zip_error zip_read_open_file_stream(struct zip_archive *zp,
 uint32_t *destLen);

enum zip_error zip_read_close_stream(struct zip_archive *zp);

enum zip_error zip_read_open_mem_stream(struct zip_archive *zp,
 struct memfile *mf);

enum zip_error zip_read_close_mem_stream(struct zip_archive *zp);

enum zip_error zip_rewind(struct zip_archive *zp);
enum zip_error zip_skip_file(struct zip_archive *zp);
enum zip_error zip_read_file(struct zip_archive *zp,
 void *destBuf, uint32_t destLen, uint32_t *readLen);

enum zip_error zputc(int value, struct zip_archive *zp);
enum zip_error zputw(int value, struct zip_archive *zp);
enum zip_error zputd(int value, struct zip_archive *zp);
enum zip_error zwrite(const void *src, uint32_t srcLen, struct zip_archive *zp);

enum zip_error zip_write_open_file_stream(struct zip_archive *zp,
 const char *name, int method, uint32_t prop_id, char board_id,
 char robot_id);

enum zip_error zip_write_close_stream(struct zip_archive *zp);

enum zip_error zip_write_open_mem_stream(struct zip_archive *zp,
 struct memfile *mf, const char *name, uint32_t prop_id, char board_id,
 char robot_id);

enum zip_error zip_write_close_mem_stream(struct zip_archive *zp,
 struct memfile *mf);

enum zip_error zip_write_file(struct zip_archive *zp, const char *name,
 const void *src, uint32_t srcLen, int method, uint32_t prop_id, char board_id,
 char robot_id);

enum zip_error zip_read_directory(struct zip_archive *zp);
CORE_LIBSPEC enum zip_error zip_close(struct zip_archive *zp,
 uint32_t *final_length);

struct zip_archive *zip_open_fp_read(FILE *fp);
struct zip_archive *zip_open_fp_write(FILE *fp);
struct zip_archive *zip_open_file_read(const char *file_name);
struct zip_archive *zip_open_file_write(const char *file_name);
struct zip_archive *zip_open_mem_read(const void *src, uint32_t len);
struct zip_archive *zip_open_mem_write(void *src, uint32_t len);

enum zip_error zip_expand(struct zip_archive *zp, char **src, uint32_t new_size);

__M_END_DECLS

#endif //__ZIP_H
