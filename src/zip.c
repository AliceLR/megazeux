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

#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#include <zlib.h>

// This needs to stay self-sufficient - don't use core functions.
// Including util.h for the macros only...

#include "memfile.h"
#include "util.h"
#include "zip.h"

#define ZIP_VERSION 20
#define ZIP_VERSION_MINIMUM 20

// Data descriptors are short areas after the compressed file containing the
// uncompressed size, compressed size, and CRC-32 checksum of the file, and
// are useful for platforms where seeking backward is impossible/infeasible.

// The NDS libfat library may have a bug with seeking backwards from the end of
// the file. Enable data descriptors so the save can be handled in one pass.

#ifdef CONFIG_NDS
#define ZIP_WRITE_DATA_DESCRIPTOR
#endif // CONFIG_NDS

#define ZIP_DEFAULT_NUM_FILES 4

#define ZIP_S_READ_RAW 0
#define ZIP_S_READ_FILES 1
#define ZIP_S_READ_STREAM 2
#define ZIP_S_READ_MEMSTREAM 3
#define ZIP_S_WRITE_RAW 8
#define ZIP_S_WRITE_FILES 9
#define ZIP_S_WRITE_STREAM 10
#define ZIP_S_WRITE_MEMSTREAM 11

/* This zip reader/writer is designed:
 *
 * 1) Around the needs of our world/board/robot/MZM files. These files
 * need binary headers, or at least it's nice to have them. They don't
 * need to know the zip archive metadata, except when we're extracting
 * them. However, if we need things like data subdirectories or the
 * VFS ajs wanted, it should be extendable.
 *
 * 2) With the ability to use memory blocks instead of files. This is
 * mainly for MZMs, which need to be writable both to file and to strings.
 *
 * 3) With the ability to store extra data in the zip related to the MZX world.
 *
 * While not copied from or even based on ajs's zipio, I took a few
 * pointers from it.
 */


// Extended file reading functions duplicated from util.h

// Get 2 bytes, little endian

static int _fgetw(FILE *fp)
{
  int a = fgetc(fp), b = fgetc(fp);
  if((a == EOF) || (b == EOF))
    return EOF;

  return (b << 8) | a;
}

// Get 4 bytes, little endian

static int _fgetd(FILE *fp)
{
  int a = fgetc(fp), b = fgetc(fp), c = fgetc(fp), d = fgetc(fp);
  if((a == EOF) || (b == EOF) || (c == EOF) || (d == EOF))
    return EOF;

  return (d << 24) | (c << 16) | (b << 8) | a;
}

// Put 2 bytes, little endian

static void _fputw(int src, FILE *fp)
{
  fputc(src & 0xFF, fp);
  fputc(src >> 8, fp);
}

// Put 4 bytes, little endian

static void _fputd(int src, FILE *fp)
{
  fputc(src & 0xFF, fp);
  fputc((src >> 8) & 0xFF, fp);
  fputc((src >> 16) & 0xFF, fp);
  fputc((src >> 24) & 0xFF, fp);
}

// NDS libfat fseek will always fail if a function pointer to it
// is used, so it needs to be wrapped in another function to work.

static int _fseekwrapper(void *vp, long int offset, int code)
{
  return fseek((FILE *)vp, offset, code);
}

// devkitARM's ftell has a nonstandard signature and needs to be wrapped, too.

static long _ftellwrapper(FILE *fp)
{
  return (long)ftell(fp);
}

static long int _fstatlen(FILE *fp)
{
  struct stat file_info;

  if(fstat(fileno(fp), &file_info))
  {
    return -1;
  }

  return file_info.st_size;
}

static int zip_get_dos_date_time(void)
{
  time_t current_time = time(NULL);
  struct tm *tm = localtime(&current_time);

  uint16_t time;
  uint16_t date;

  time =  (tm->tm_hour << 11);
  time |= (tm->tm_min << 5);
  time |= (tm->tm_sec >> 1);

  date =  ((tm->tm_year - 80) << 9);
  date |= ((tm->tm_mon + 1) << 5);
  date |= (tm->tm_mday);

  return (date << 16) | time;
}


static const char *zip_error_string(enum zip_error code)
{
  switch(code)
  {
    case ZIP_SUCCESS:
      return "no error";
    case ZIP_EOF:
      return "reached end of file";
    case ZIP_NULL:
      return "function received null archive";
    case ZIP_NULL_BUF:
      return "function received null buffer";
    case ZIP_STAT_ERROR:
      return "fstat failed for input file";
    case ZIP_SEEK_ERROR:
      return "could not seek to position";
    case ZIP_READ_ERROR:
      return "could not read from position";
    case ZIP_WRITE_ERROR:
      return "could not write to position";
    case ZIP_OPEN_ERROR:
      return "could not open archive";
    case ZIP_ALLOC_MORE_SPACE:
      return "archive alloc too small";
    case ZIP_INVALID_WHILE_CLOSING:
      return "invalid operation; archive is being closed";
    case ZIP_INVALID_READ_IN_WRITE_MODE:
      return "can't read in write mode";
    case ZIP_INVALID_WRITE_IN_READ_MODE:
      return "can't write in read mode";
    case ZIP_INVALID_RAW_READ_IN_FILE_MODE:
      return "can't read directly in file mode";
    case ZIP_INVALID_RAW_WRITE_IN_FILE_MODE:
      return "can't write directly in file mode";
    case ZIP_INVALID_DIRECTORY_READ_IN_FILE_MODE:
      return "directory has already been read";
    case ZIP_INVALID_FILE_READ_IN_RAW_MODE:
      return "directory has not been read";
    case ZIP_INVALID_FILE_READ_IN_STREAM_MODE:
      return "can't read file in stream mode";
    case ZIP_INVALID_FILE_WRITE_IN_STREAM_MODE:
      return "can't write file in stream mode";
    case ZIP_INVALID_STREAM_READ:
      return "can't read/close; not streaming";
    case ZIP_INVALID_STREAM_WRITE:
      return "can't write/close; not streaming";
    case ZIP_NOT_MEMORY_ARCHIVE:
      return "archive isn't a memory archive";
    case ZIP_NO_EOCD:
      return "could not find EOCD record";
    case ZIP_NO_CENTRAL_DIRECTORY:
      return "could not find central directory";
    case ZIP_INCOMPLETE_CENTRAL_DIRECTORY:
      return "central directory is missing records";
    case ZIP_UNSUPPORTED_MULTIPLE_DISKS:
      return "unsupported multiple volume archive";
    case ZIP_UNSUPPORTED_FLAGS:
      return "unsupported flags; use 0";
    case ZIP_UNSUPPORTED_COMPRESSION:
      return "unsupported method; use DEFLATE or none";
    case ZIP_UNSUPPORTED_ZIP64:
      return "unsupported ZIP64 data";
    case ZIP_UNSUPPORTED_DEFLATE_STREAM:
      return "DEFLATE: can only stream whole file";
    case ZIP_MISSING_LOCAL_HEADER:
      return "could not find file header";
    case ZIP_MISSING_DATA_DESCRIPTOR:
      return "could not find the data descriptor";
    case ZIP_HEADER_MISMATCH:
      return "local header mismatch";
    case ZIP_CRC32_MISMATCH:
      return "CRC-32 mismatch; validation failed";
    case ZIP_INFLATE_FAILED:
      return "decompression failed";
    case ZIP_DEFLATE_FAILED:
      return "compression failed";
  }
  return "UNKNOWN ERROR";
}

static void zip_error(const char *func, enum zip_error code)
{
  warn("%s: %s\n", func, zip_error_string(code));
}


static inline uint32_t zip_crc32(uint32_t crc, const void *src, uint32_t srcLen)
{
  return crc32(crc, (Bytef *) src, (uLong) srcLen);
}

static int zip_uncompress(char *dest, uint32_t *destLen, const char *src,
 uint32_t *srcLen)
{
  z_stream stream;
  int err;

  // Following uncompr.c from zlib pretty closely here...
  stream.next_in = (Bytef *) src;
  stream.avail_in = (uInt) *srcLen;

  stream.zalloc = (alloc_func)0;
  stream.zfree = (free_func)0;
  stream.opaque = (voidpf)0;

  /* The reason we can't just use uncompress() is because there's no way
   * to assign the number of window bits with it, and uncompress() expects
   * them to exist. We're just inflating raw DEFLATE data, so we set them
   * to -MAX_WBITS.
   */

  inflateInit2(&stream, -MAX_WBITS);

  stream.next_out = (Bytef *) dest;
  stream.avail_out = (uInt) *destLen;

  err = inflate(&stream, Z_FINISH);

  *srcLen = (int) stream.total_in;
  *destLen = (int) stream.total_out;

  inflateEnd(&stream);

  return err;
}

static int zip_compress(char **dest, uint32_t *destLen, const char *src,
 uint32_t *srcLen)
{
  z_stream stream;
  int _destLen;
  int err;

  stream.zalloc = (alloc_func)0;
  stream.zfree = (free_func)0;
  stream.opaque = (voidpf)0;

  /* The reason we can't just use compress() is because there's no way
   * to assign the number of window bits with it, and compress() expects
   * them to exist. We're just deflating to raw DEFLATE data, so we set
   * them to -MAX_WBITS.
   */

  // Note: aside from the windowbits, these are all defaults
  deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS,
   8, Z_DEFAULT_STRATEGY);

  stream.next_in = (Bytef *) src;
  stream.avail_in = (uInt) *srcLen;

  _destLen = deflateBound(&stream, (uLong) *srcLen);

  *dest = cmalloc(_destLen);

  stream.next_out = (Bytef *) *dest;
  stream.avail_out = (uInt) _destLen;

  err = deflate(&stream, Z_FINISH);

  *srcLen = (int) stream.total_in;
  *destLen = (int) stream.total_out;

  deflateEnd(&stream);

  return err;
}


/* Calculate an upper bound for the total compressed size of a memory block. */

int zip_bound_data_usage(char *src, int srcLen)
{
  z_stream stream;
  int bound;

  stream.next_in = (Bytef *) src;
  stream.avail_in = (uInt) srcLen;

  stream.zalloc = (alloc_func)0;
  stream.zfree = (free_func)0;
  stream.opaque = (voidpf)0;

  // Note: aside from the windowbits, these are all defaults
  deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS,
   8, Z_DEFAULT_STRATEGY);

  bound = deflateBound(&stream, (uLong) srcLen);

  deflateEnd(&stream);

  return bound;
}


/* Calculate an upper bound for the total size of headers for an archive. */

int zip_bound_total_header_usage(int num_files, int max_name_size)
{
  // Expected:
  // max_name_size = 8 for MZX world data
  //               = 5 for MZM data
  int extra = 0;

#ifdef ZIP_WRITE_DATA_DESCRIPTOR
  extra = num_files * 12;   // data descriptor size
#endif

  return num_files *
   // base + file name
   (30 + max_name_size +    // Local
    46 + max_name_size) +   // Central directory
    22 + extra;             // EOCD record
}


/* Basic checks to make sure the read functions can actually be used. */

#define zip_read_raw_mode_check(zp)                                     \
( !(zp)                           ? ZIP_NULL :                          \
  (zp)->closing                   ? ZIP_INVALID_WHILE_CLOSING :         \
  (zp)->mode == ZIP_S_READ_STREAM ? ZIP_SUCCESS :                       \
  (zp)->mode == ZIP_S_READ_RAW    ? ZIP_SUCCESS :                       \
  (zp)->mode == ZIP_S_READ_FILES  ? ZIP_INVALID_RAW_READ_IN_FILE_MODE : \
  ZIP_INVALID_READ_IN_WRITE_MODE)

#define zip_read_file_mode_check(zp)                                    \
( !(zp)                           ? ZIP_NULL :                          \
  (zp)->closing                   ? ZIP_INVALID_WHILE_CLOSING :         \
  (zp)->mode == ZIP_S_READ_FILES  ? ZIP_SUCCESS :                       \
  (zp)->mode == ZIP_S_READ_STREAM ? ZIP_INVALID_FILE_READ_IN_STREAM_MODE : \
  (zp)->mode == ZIP_S_READ_RAW    ? ZIP_INVALID_FILE_READ_IN_RAW_MODE : \
  ZIP_INVALID_READ_IN_WRITE_MODE)

#define zip_read_stream_mode_check(zp)                                  \
( !(zp)                           ? ZIP_NULL :                          \
  (zp)->closing                   ? ZIP_INVALID_WHILE_CLOSING :         \
  (zp)->mode == ZIP_S_READ_STREAM ? ZIP_SUCCESS :                       \
  (zp)->mode == ZIP_S_READ_RAW    ? ZIP_INVALID_STREAM_READ :           \
  (zp)->mode == ZIP_S_READ_FILES  ? ZIP_INVALID_STREAM_READ :           \
  ZIP_INVALID_READ_IN_WRITE_MODE)

/* Basic checks to make sure the write functions can be used. */

#define zip_write_raw_mode_check(zp)                                      \
( !(zp)                            ? ZIP_NULL :                           \
  (zp)->closing                    ? ZIP_INVALID_WHILE_CLOSING :          \
  (zp)->mode == ZIP_S_WRITE_STREAM ? ZIP_SUCCESS :                        \
  (zp)->mode == ZIP_S_WRITE_RAW    ? ZIP_SUCCESS :                        \
  (zp)->mode == ZIP_S_WRITE_FILES  ? ZIP_INVALID_RAW_WRITE_IN_FILE_MODE : \
  ZIP_INVALID_WRITE_IN_READ_MODE)

#define zip_write_file_mode_check(zp)                                     \
( !(zp)                            ? ZIP_NULL :                           \
  (zp)->closing                    ? ZIP_INVALID_WHILE_CLOSING :          \
  (zp)->mode == ZIP_S_WRITE_FILES  ? ZIP_SUCCESS :                        \
  (zp)->mode == ZIP_S_WRITE_RAW    ? ZIP_SUCCESS :                        \
  (zp)->mode == ZIP_S_WRITE_STREAM ? ZIP_INVALID_FILE_WRITE_IN_STREAM_MODE : \
  ZIP_INVALID_WRITE_IN_READ_MODE)

#define zip_write_stream_mode_check(zp)                                   \
( !(zp)                            ? ZIP_NULL :                           \
  (zp)->closing                    ? ZIP_INVALID_WHILE_CLOSING :          \
  (zp)->mode == ZIP_S_WRITE_STREAM ? ZIP_SUCCESS :                        \
  (zp)->mode == ZIP_S_WRITE_FILES  ? ZIP_INVALID_STREAM_WRITE :           \
  (zp)->mode == ZIP_S_WRITE_RAW    ? ZIP_INVALID_STREAM_WRITE :           \
  ZIP_INVALID_WRITE_IN_READ_MODE)

static inline void precalculate_read_errors(struct zip_archive *zp)
{
  zp->read_raw_error = zip_read_raw_mode_check(zp);
  zp->read_file_error = zip_read_file_mode_check(zp);
  zp->read_stream_error = zip_read_stream_mode_check(zp);
}

static inline void precalculate_write_errors(struct zip_archive *zp)
{
  zp->write_raw_error = zip_write_raw_mode_check(zp);
  zp->write_file_error = zip_write_file_mode_check(zp);
  zp->write_stream_error = zip_write_stream_mode_check(zp);
}


static char file_sig[] =
{
  0x50,
  0x4b,
  0x03,
  0x04
};

static char file_sig_central[] =
{
  0x50,
  0x4b,
  0x01,
  0x02
};

static enum zip_error zip_read_file_header(struct zip_archive *zp,
 struct zip_file_header *fh, uint32_t expected_c_size, int is_central)
{
  char buffer[42];
  struct memfile mf;

  int data_position;
  int method;
  int flags;
  int n, i;

  void *fp = zp->fp;
  int (*vgetc)(void *) = zp->vgetc;
  int (*vgetd)(void *) = zp->vgetd;
  size_t (*vread)(void *, size_t, size_t, void *) = zp->vread;
  int (*vseek)(void *, long int, int) = zp->vseek;
  int (*hasspace)(size_t, void *) = zp->hasspace;

  char *magic = is_central ? file_sig_central : file_sig;
  int header_length = is_central ? 46 : 30;

  fh->file_name = NULL;

  // Find the next file header
  i = 0;
  while(1)
  {
    // Memfiles: make sure there's enough space
    if(hasspace && !hasspace(header_length - i, fp))
      return ZIP_MISSING_LOCAL_HEADER;

    n = vgetc(fp);
    if(n < 0)
      return ZIP_MISSING_LOCAL_HEADER;

    // Match the signature
    if(n == magic[i])
    {
      i++;
      if(i == 4)
        // We've found the file header signature
        break;
    }
    else

    // Even if it's not a match, it could be the start of the signature.
    if(n == 'P')
      i = 1;

    else
      i = 0;
  }

  // We already read four
  if(!vread(buffer, header_length - 4, 1, fp))
    return ZIP_READ_ERROR;

  mfopen_static(buffer, header_length - 4, &mf);

  if(is_central)
  {
    // Version made by              2
    // Version needed to extract    2
    mf.current += 4;

    // General purpose bit flag     2

    flags = mfgetw(&mf);

    if((flags & ~ZIP_F_DATA_DESCRIPTOR) != 0)
    {
      warn("Zip using unsupported options (%d) -- use 0 or 8.\n", flags);
      return ZIP_UNSUPPORTED_FLAGS;
    }
    fh->flags = flags;

    // Compression method           2

    method = mfgetw(&mf);

    if(method < 0)
    {
      return ZIP_READ_ERROR;
    }
    else
    if(method != ZIP_M_NONE && method != ZIP_M_DEFLATE)
    {
      return ZIP_UNSUPPORTED_COMPRESSION;
    }
    fh->method = method;

    // File last modification time  2
    // File last modification date  2
    mf.current += 4;

    // CRC-32, sizes                12

    fh->crc32 = mfgetd(&mf);
    fh->compressed_size = mfgetd(&mf);
    fh->uncompressed_size = mfgetd(&mf);

    if(fh->compressed_size == 0xFFFFFFFF)
      return ZIP_UNSUPPORTED_ZIP64;

    if(fh->uncompressed_size == 0xFFFFFFFF)
      return ZIP_UNSUPPORTED_ZIP64;

    // File name length             2
    // Extra field length           2
    // File comment length          2

    n = mfgetw(&mf);
    fh->file_name_length = n;
    data_position = zp->vtell(fp) + n + mfgetw(&mf) + mfgetw(&mf);

    // Disk number of file start    2
    // Internal file attributes     2
    // External file attributes     4
    mf.current += 8;

    // Offset to local header       4 (from start of archive)
    fh->offset = mfgetd(&mf);

    // File name (n)
    fh->file_name = cmalloc(n + 1);
    vread(fh->file_name, n, 1, fp);
    fh->file_name[n] = 0;

    // Done.
  }

  else
  {
    // Local. These are generally redundant with the central directory.
    // Aside from the fields used to validate, ignore.

    // Version made by              2
    mf.current += 2;

    // General purpose bit flag     2

    flags = mfgetw(&mf);
    fh->flags = flags;

    // Compression method           2
    // File last modification time  2
    // File last modification date  2
    mf.current += 6;

    if(!(flags & ZIP_F_DATA_DESCRIPTOR))
    {
      // Normal.

      // CRC-32, sizes              12

      fh->crc32 = mfgetd(&mf);
      fh->compressed_size = mfgetd(&mf);
      fh->uncompressed_size = mfgetd(&mf);

      // File name length           2
      // Extra field length         2

      data_position = zp->vtell(fp) + mfgetw(&mf) + mfgetw(&mf);

      // Done.
    }

    else
    {
      // With data descriptor.

      // CRC-32, sizes              12
      mf.current += 12;

      // File name length           2
      // Extra field length         2

      data_position = zp->vtell(fp) + mfgetw(&mf) + mfgetw(&mf);

      // CRC-32, sizes              12

      vseek(fp, data_position + expected_c_size, SEEK_SET);

      fh->crc32 = vgetd(fp);
      fh->compressed_size = vgetd(fp);
      fh->uncompressed_size = vgetd(fp);

      // Done.
    }
  }

  if(vseek(fp, data_position, SEEK_SET))
    return ZIP_SEEK_ERROR;

  return ZIP_SUCCESS;
}


static enum zip_error zip_write_file_header(struct zip_archive *zp,
 struct zip_file_header *fh, int is_central)
{
  enum zip_error result = ZIP_SUCCESS;

  void *fp = zp->fp;

  char *magic;
  char *buffer;
  struct memfile mf;
  int header_size;
  int i;

  if(is_central)
  {
    // Position to write CRC, sizes after file write
    zp->stream_crc_position = zp->vtell(fp) + 16;
    header_size = fh->file_name_length + 46;
    magic = file_sig_central;
  }
  else
  {
    zp->stream_crc_position = zp->vtell(fp) + 14;
    header_size = fh->file_name_length + 30;
    magic = file_sig;
  }

  buffer = cmalloc(header_size);
  mfopen_static(buffer, header_size, &mf);

  // Signature
  for(i = 0; i<4; i++)
  {
    mfputc(magic[i], &mf);
  }

  // Version made by
  mfputw(ZIP_VERSION, &mf);

  // Version needed to extract (central directory only)
  if(is_central)
    mfputw(ZIP_VERSION_MINIMUM, &mf);

  // General purpose bit flag
  mfputw(fh->flags, &mf);

  // Compression method
  mfputw(fh->method, &mf);

  // File last modification time
  // File last modification date
  mfputd(zip_get_dos_date_time(), &mf);

  // note: zp->stream_crc_position should be here.

  // CRC-32
  mfputd(fh->crc32, &mf);

  // Compressed size
  mfputd(fh->compressed_size, &mf);

  // Uncompressed size
  mfputd(fh->uncompressed_size, &mf);

  // File name length
  mfputw(fh->file_name_length, &mf);

  // Extra field length
  mfputw(0, &mf);

  // (central directory only fields)
  if(is_central)
  {
    // File comment length
    mfputw(0, &mf);

    // Disk number where file starts
    mfputw(0, &mf);

    // Internal file attributes
    mfputw(0, &mf);

    // External file attributes
    mfputd(0, &mf);

    // Relative offset of local file header
    mfputd(fh->offset, &mf);
  }

  // File name
  mfwrite(fh->file_name, fh->file_name_length, 1, &mf);

  // Extra field (zero bytes)

  // File comment (zero bytes)

  if(!zp->vwrite(buffer, 1, header_size, fp))
    result = ZIP_WRITE_ERROR;

  // Memfile zips - we know there's enough space, because it was already
  // checked in zip_write_file or zip_close

  // Check for errors.
  if(zp->verror && zp->verror(fp))
    result = ZIP_WRITE_ERROR;

  free(buffer);
  return result;
}



/***************************/
/* Seeking (raw mode only) */
/***************************/

enum zip_error zseek(struct zip_archive *zp, int value, int code)
{
  if(!zp || zp->closing)
    return ZIP_SEEK_ERROR;

  if((zp->mode == ZIP_S_READ_RAW || zp->mode == ZIP_S_WRITE_RAW) &&
   !zp->vseek(zp->fp, value, code))
    return ZIP_SUCCESS;

  return ZIP_SEEK_ERROR;
}



/***********/
/* Reading */
/***********/

/* Read data from a zip archive. Only works before zip_read_directory()
 * is called or while streaming a file.
 */

int zgetc(struct zip_archive *zp, enum zip_error *err)
{
  char v;

  if((*err = zp->read_raw_error))
    return -1;

  v = zp->vgetc(zp->fp);

  if(zp->streaming_file)
  {
    if(zp->stream_left>0)
    {
      zp->stream_crc32 = zip_crc32(zp->stream_crc32, &v, 1);
      zp->stream_left -= 1;
      return v;
    }
    return -1;
  }

  return v;
}

int zgetw(struct zip_archive *zp, enum zip_error *err)
{
  if((*err = zp->read_raw_error))
    return -1;

  if(zp->streaming_file)
  {
    char v[2];

    if(!zp->vread(v, 2, 1, zp->fp))
    {
      *err = ZIP_EOF;
      return -1;
    }

    zp->stream_crc32 = zip_crc32(zp->stream_crc32, (char *)&v, 4);
    zp->stream_left -= 4;

    return (v[1] << 8) | v[0];
  }

  return zp->vgetw(zp->fp);
}

int zgetd(struct zip_archive *zp, enum zip_error *err)
{
  if((*err = zp->read_raw_error))
    return -1;

  if(zp->streaming_file)
  {
    char v[4];

    if(!zp->vread(v, 4, 1, zp->fp))
    {
      *err = ZIP_EOF;
      return -1;
    }

    zp->stream_crc32 = zip_crc32(zp->stream_crc32, (char *)&v, 4);
    zp->stream_left -= 4;

    return (v[3] << 24) | (v[2] << 16) | (v[1] << 8) | v[0];
  }

  return zp->vgetd(zp->fp);
}

enum zip_error zread(void *destBuf, size_t readLen, struct zip_archive *zp)
{
  struct zip_file_header *fh;
  char *src;

  uint32_t consumed;

  enum zip_error result;

  result = zp->read_raw_error;
  if(result)
    goto err_out;

  if(!readLen) return ZIP_SUCCESS;

  fh = zp->streaming_file;

  // No compression
  if(!fh || fh->method == ZIP_M_NONE)
  {
    if(fh)
    {
      // Can't read past the length of the file
      consumed = MIN(readLen, zp->stream_left);
    }
    else
    {
      consumed = readLen;
    }

    if(!zp->vread(destBuf, consumed, 1, zp->fp))
    {
      result = ZIP_EOF;
      goto err_out;
    }
  }
  else

  // DEFLATE compression
  if(fh->method == ZIP_M_DEFLATE)
  {
    uint32_t u_size = fh ? fh->uncompressed_size : 0;
    uint32_t c_size = fh ? fh->compressed_size : 0;

    if(!fh || readLen != u_size)
    {
      result = ZIP_UNSUPPORTED_DEFLATE_STREAM;
      goto err_out;
    }

    src = cmalloc(c_size);

    if(!zp->vread(src, c_size, 1, zp->fp))
    {
      result = ZIP_READ_ERROR;
      goto err_free_src;
    }

    consumed = c_size;
    result = zip_uncompress(destBuf, &u_size, src, &consumed);

    if(result != Z_STREAM_END || readLen != u_size || consumed != c_size)
    {
      result = ZIP_INFLATE_FAILED;
      goto err_free_src;
    }
    free(src);
  }

  // This shouldn't have made it this far...
  else
  {
    result = ZIP_UNSUPPORTED_COMPRESSION;
    goto err_out;
  }

  // Update the crc32 and streamed amount
  if(fh)
  {
    zp->stream_crc32 = zip_crc32(zp->stream_crc32, destBuf, readLen);
    zp->stream_left -= consumed;
  }

  return ZIP_SUCCESS;

err_free_src:
  free(src);

err_out:
  if(result != ZIP_EOF)
    zip_error("zread", result);

  return result;
}


/* Get the name of the next file in the archive. Only works after
 * zip_read_directory() is called.
 */

enum zip_error zip_get_next_name(struct zip_archive *zp,
 char *name, int name_buffer_size)
{
  struct zip_file_header *fh;
  enum zip_error result;

  result = zp->read_file_error;
  if(result)
    goto err_out;

  if(zp->pos >= zp->num_files)
  {
    return ZIP_EOF;
  }

  fh = zp->files[zp->pos];

  // Copy the file name, if requested
  if(name && name_buffer_size)
  {
    name_buffer_size = MIN(name_buffer_size, fh->file_name_length);
    memcpy(name, fh->file_name, name_buffer_size);
    name[name_buffer_size] = 0;
  }

  return ZIP_SUCCESS;

err_out:
  if(result != ZIP_EOF)
    zip_error("zip_get_next_name", result);
  return result;
}


/* Get the MZX properties of the next file in the archive. Only works after
 * zip_read_directory() is called.
 */

enum zip_error zip_get_next_prop(struct zip_archive *zp,
 unsigned int *prop_id, unsigned int *board_id, unsigned int *robot_id)
{
  struct zip_file_header *fh;
  enum zip_error result;

  result = zp->read_file_error;
  if(result)
    goto err_out;

  if(zp->pos >= zp->num_files)
  {
    return ZIP_EOF;
  }

  fh = zp->files[zp->pos];

  if(prop_id)
    *prop_id = fh->mzx_prop_id;

  if(board_id)
    *board_id = fh->mzx_board_id;

  if(robot_id)
    *robot_id = fh->mzx_robot_id;

  return ZIP_SUCCESS;

err_out:
  if(result != ZIP_EOF)
    zip_error("zip_get_next_prop", result);
  return result;
}


/* Get the uncompressed length of the next file in the archive. Only works after
 * zip_read_directory() is called.
 */

enum zip_error zip_get_next_uncompressed_size(struct zip_archive *zp,
 size_t *u_size)
{
  enum zip_error result;

  result = zp->read_file_error;
  if(result)
    goto err_out;

  if(zp->pos >= zp->num_files)
  {
    return ZIP_EOF;
  }

  if(u_size)
    *u_size = zp->files[zp->pos]->uncompressed_size;

  return ZIP_SUCCESS;

err_out:
  if(result != ZIP_EOF)
    zip_error("zip_get_next_u_size", result);
  return result;
}


/* Get the compression method of the next file in the archive. Only works after
 * zip_read_directory() is called.
 */

enum zip_error zip_get_next_method(struct zip_archive *zp, unsigned int *method)
{
  enum zip_error result;

  result = zp->read_file_error;
  if(result)
    goto err_out;

  if(zp->pos >= zp->num_files)
  {
    return ZIP_EOF;
  }

  if(method)
    *method = zp->files[zp->pos]->method;

  return ZIP_SUCCESS;

err_out:
  if(result != ZIP_EOF)
    zip_error("zip_get_next_method", result);
  return result;
}


/* Open a stream to read the next file from a zip archive. Only works after
 * zip_read_directory() is called. If provided, destLen will be the uncompressed
 * size of the file on return, or 0 if an error occurred. Intended for use with
 * file output; use zip_read_open_mem_stream() for memory output.
 */

enum zip_error zip_read_open_file_stream(struct zip_archive *zp,
 size_t *destLen)
{
  struct zip_file_header *central_fh;
  struct zip_file_header local_fh;
  void *fp;

  uint32_t f_crc32;
  uint32_t c_size;
  uint32_t u_size;
  uint16_t method;

  uint32_t read_pos;
  enum zip_error result;

  result = (zp ? zp->read_file_error : ZIP_NULL);
  if(result)
    goto err_out;

  if(zp->pos >= zp->num_files)
  {
    result = ZIP_EOF;
    goto err_out;
  }

  central_fh = zp->files[zp->pos];

  f_crc32 = central_fh->crc32;
  c_size = central_fh->compressed_size;
  u_size = central_fh->uncompressed_size;
  method = central_fh->method;

  // Check for unsupported methods
  if(method != ZIP_M_NONE && method != ZIP_M_DEFLATE)
  {
    result = ZIP_UNSUPPORTED_COMPRESSION;
    goto err_out;
  }

  // Seek to the start of the record
  fp = zp->fp;
  read_pos = zp->vtell(fp);
  if(read_pos != central_fh->offset)
  {
    if(zp->vseek(fp, central_fh->offset, SEEK_SET))
    {
      result = ZIP_SEEK_ERROR;
      goto err_out;
    }
  }

  // Read the local header --  give it the expected
  // compressed size just in the case there's a data descriptor.

  result = zip_read_file_header(zp, &local_fh, c_size, 0);
  if(result)
    goto err_out;

  // Verify correctness between headers
  if(f_crc32 != local_fh.crc32 ||
   c_size != local_fh.compressed_size ||
   u_size != local_fh.uncompressed_size)
  {
    result = ZIP_HEADER_MISMATCH;
    goto err_out;
  }

  // Everything looks good. Set up stream mode.
  zp->mode = ZIP_S_READ_STREAM;
  zp->streaming_file = central_fh;
  zp->stream_left = c_size;
  zp->stream_crc32 = 0;

  if(destLen)
    *destLen = u_size;

  precalculate_read_errors(zp);
  return ZIP_SUCCESS;

err_out:
  if(result != ZIP_EOF)
    zip_error("zip_read_open_file_stream", result);
  if(destLen)
    *destLen = 0;
  return result;
}


/* Close the reading stream.
 */

enum zip_error zip_read_close_stream(struct zip_archive *zp)
{
  uint32_t expected_crc32;
  uint32_t stream_crc32;
  uint32_t stream_left;
  char buffer[128];
  int size;

  size_t (*vread)(void *, size_t, size_t, void *);
  void *fp;

  enum zip_error result;

  result = (zp ? zp->read_stream_error : ZIP_NULL);
  if(result)
    goto err_out;

  vread = zp->vread;
  fp = zp->fp;

  expected_crc32 = zp->streaming_file->crc32;
  stream_crc32 = zp->stream_crc32;

  // If the stream was incomplete, finish the crc32
  stream_left = zp->stream_left;
  while(stream_left)
  {
    size = MIN(128, stream_left);
    vread(buffer, size, 1, fp);
    stream_crc32 = zip_crc32(stream_crc32, buffer, size);
    stream_left -= size;
  }

  // Increment the position and clear the streaming vars
  zp->mode = ZIP_S_READ_FILES;
  zp->streaming_file = NULL;
  zp->stream_left = 0;
  zp->stream_crc32 = 0;
  zp->pos++;

  precalculate_read_errors(zp);

  // Check the CRC-32 of the stream
  if(expected_crc32 != stream_crc32)
  {
    warn("crc check: expected %"PRIx32", got %"PRIx32"\n",
     expected_crc32, stream_crc32);
    result = ZIP_CRC32_MISMATCH;
    goto err_out;
  }

  return ZIP_SUCCESS;

err_out:
  zip_error("zip_read_close_stream", result);
  return result;
}


/* Like zip_read_open_file_stream, but allows direct memory access.
 * This is abusable, but far quicker than the alternatives.
 */

enum zip_error zip_read_open_mem_stream(struct zip_archive *zp,
 struct memfile *mf)
{
  struct zip_file_header *central_fh;
  struct zip_file_header local_fh;
  void *fp;

  uint32_t f_crc32;
  uint32_t c_size;
  uint32_t u_size;
  uint16_t method;

  uint32_t read_pos;
  enum zip_error result;

  result = (zp ? zp->read_file_error : ZIP_NULL);
  if(result)
    goto err_out;

  if(!zp->hasspace)
  {
    result = ZIP_NOT_MEMORY_ARCHIVE;
    goto err_out;
  }

  if(zp->pos >= zp->num_files)
  {
    result = ZIP_EOF;
    goto err_out;
  }

  central_fh = zp->files[zp->pos];

  f_crc32 = central_fh->crc32;
  c_size = central_fh->compressed_size;
  u_size = central_fh->uncompressed_size;
  method = central_fh->method;

  // Check for unsupported methods
  if(method == ZIP_M_DEFLATE)
  {
    result = ZIP_UNSUPPORTED_DEFLATE_STREAM;
    goto err_out;
  }
  else

  if(method != ZIP_M_NONE)
  {
    result = ZIP_UNSUPPORTED_COMPRESSION;
    goto err_out;
  }

  // Seek to the start of the record
  fp = zp->fp;
  read_pos = zp->vtell(fp);
  if(read_pos != central_fh->offset)
  {
    if(zp->vseek(fp, central_fh->offset, SEEK_SET))
    {
      result = ZIP_SEEK_ERROR;
      goto err_out;
    }
  }

  // Read the local header -- give it the expected
  // compressed size just in the case there's a data descriptor.

  result = zip_read_file_header(zp, &local_fh, c_size, 0);
  if(result)
    goto err_out;

  // Verify correctness between headers
  if(f_crc32 != local_fh.crc32 ||
   c_size != local_fh.compressed_size ||
   u_size != local_fh.uncompressed_size)
  {
    result = ZIP_HEADER_MISMATCH;
    goto err_out;
  }

  // Everything looks good. Set up memory stream mode.
  zp->mode = ZIP_S_READ_MEMSTREAM;
  zp->streaming_file = central_fh;
  zp->stream_left = 0;
  zp->stream_crc32 = 0;

  // Open the direct access file
  mfopen_static( ((struct memfile *)fp)->current, c_size, mf);

  precalculate_read_errors(zp);
  return ZIP_SUCCESS;

err_out:
  if(result != ZIP_EOF)
    zip_error("zip_read_open_mem_stream", result);

  mfopen_static(NULL, 0, mf);

  return result;
}

enum zip_error zip_read_close_mem_stream(struct zip_archive *zp)
{
  struct zip_file_header *fh;
  uint32_t f_crc32;
  uint32_t c_size;
  void *fp;

  enum zip_error result;

  result =
   !zp ? ZIP_NULL :
   zp->closing ? ZIP_INVALID_WHILE_CLOSING :
   zp->mode != ZIP_S_READ_MEMSTREAM ? ZIP_INVALID_STREAM_READ :
   ZIP_SUCCESS;

  if(result)
    goto err_out;

  fh = zp->streaming_file;
  fp = zp->fp;

  // Increment the position and clear the streaming vars
  zp->mode = ZIP_S_READ_FILES;
  zp->streaming_file = NULL;
  zp->stream_left = 0;
  zp->stream_crc32 = 0;
  zp->pos++;

  precalculate_read_errors(zp);

  // Verify the CRC-32 of the file.
  c_size = fh->compressed_size;
  f_crc32 = zip_crc32(0, ((struct memfile *)fp)->current, c_size);
  if(f_crc32 != fh->crc32)
  {
    result = ZIP_CRC32_MISMATCH;
    goto err_out;
  }

  zp->vseek(fp, (long int)c_size, SEEK_CUR);

  return ZIP_SUCCESS;

err_out:
  zip_error("zip_read_close_mem_stream", result);
  return result;
}


/* Rewind to the start of the zip archive. */

enum zip_error zip_rewind(struct zip_archive *zp)
{
  enum zip_error result;

  result = zp->read_file_error;
  if(result)
    goto err_out;

  if(zp->num_files == 0)
  {
    return ZIP_EOF;
  }

  zp->pos = 0;

  return ZIP_SUCCESS;

err_out:
  zip_error("zip_rewind", result);
  return result;
}


/* Skip the current file in the zip archive. Only works after
 * zip_read_directory() is called.
 */

enum zip_error zip_skip_file(struct zip_archive *zp)
{
  enum zip_error result;

  result = zp->read_file_error;
  if(result)
    goto err_out;

  if(zp->pos >= zp->num_files)
  {
    return ZIP_EOF;
  }

  zp->pos++;

  return ZIP_SUCCESS;

err_out:
  zip_error("zip_skip_file", result);
  return result;
}


/* Read a file from the a zip archive. Only works after zip_read_directory()
 * is called. If provided, readLen will return the actual number of bytes read.
 */

enum zip_error zip_read_file(struct zip_archive *zp,
 void *destBuf, size_t destLen, size_t *readLen)
{
  size_t u_size;
  enum zip_error result;

  // No need to check mode; the functions used here will

  // We can't accept a NULL pointer here, though
  if(!destBuf)
  {
    result = ZIP_NULL_BUF;
    goto err_out;
  }

  result = zip_read_open_file_stream(zp, &u_size);
  if(result)
    goto err_out;

  u_size = MIN(destLen, u_size);

  result = zread(destBuf, u_size, zp);
  if(result && result != ZIP_EOF)
    goto err_close;

  result = zip_read_close_stream(zp);
  if(result)
    goto err_out;

  if(readLen)
    *readLen = u_size;

  return ZIP_SUCCESS;

err_close:
  zip_read_close_stream(zp);

err_out:
  if(result != ZIP_EOF)
    zip_error("zip_read_file", result);

  if(readLen)
    *readLen = 0;

  return result;
}



/***********/
/* Writing */
/***********/

/* Write data to a zip archive. Only works in raw and stream modes.
 */

enum zip_error zputc(int value, struct zip_archive *zp)
{
  enum zip_error result;
  char v;

  if((result = zp->write_raw_error))
    return result;

  zp->vputc(value, zp->fp);

  if(zp->streaming_file)
  {
    zp->streaming_file->uncompressed_size += 1;
    zp->stream_crc32 = zip_crc32(zp->stream_crc32, &v, 1);
    zp->stream_left += 1;
  }

  return ZIP_SUCCESS;
}

enum zip_error zputw(int value, struct zip_archive *zp)
{
  enum zip_error result;

  if((result = zp->write_raw_error))
    return result;

  zp->vputw(value, zp->fp);

  if(zp->streaming_file)
  {
    char v[2];
    v[0] = value & 0xFF;
    v[1] = (value>>8) & 0xFF;
    zp->streaming_file->uncompressed_size += 2;
    zp->stream_crc32 = zip_crc32(zp->stream_crc32, (char *)&v, 2);
    zp->stream_left += 2;
  }

  return ZIP_SUCCESS;
}

enum zip_error zputd(int value, struct zip_archive *zp)
{
  enum zip_error result;

  if((result = zp->write_raw_error))
    return result;

  zp->vputd(value, zp->fp);

  if(zp->streaming_file)
  {
    char v[4];
    v[0] = value & 0xFF;
    v[1] = (value>>8) & 0xFF;
    v[2] = (value>>16) & 0xFF;
    v[3] = (value>>24) & 0xFF;
    zp->streaming_file->uncompressed_size += 4;
    zp->stream_crc32 = zip_crc32(zp->stream_crc32, (char *)&v, 4);
    zp->stream_left += 4;
  }

  return ZIP_SUCCESS;
}

enum zip_error zwrite(const void *src, size_t srcLen, struct zip_archive *zp)
{
  struct zip_file_header *fh;
  char *buffer = NULL;
  uint32_t writeLen;
  uint16_t method = ZIP_M_NONE;

  void *fp;

  enum zip_error result;

  result = zp->write_raw_error;
  if(result)
    goto err_out;

  if(!srcLen) return ZIP_SUCCESS;

  fp = zp->fp;

  fh = zp->streaming_file;
  if(fh)
  {
    method = fh->method;
  }

  // No compression
  if(method == ZIP_M_NONE)
  {
    writeLen = srcLen;

    // Test if we have the space to write
    if(zp->hasspace)
    {
      if(!zp->hasspace(srcLen, fp))
      {
        result = ZIP_ALLOC_MORE_SPACE;
        goto err_out;
      }
    }

    if(!zp->vwrite(src, srcLen, 1, fp))
    {
      result = ZIP_WRITE_ERROR;
      goto err_out;
    }
  }
  else

  // DEFLATE
  // This might cause issues if the entire file isn't written in one go. Not sure.
  if(method == ZIP_M_DEFLATE)
  {
    uint32_t consumed = srcLen;

    result = zip_compress(&buffer, &writeLen, src, &consumed);
    if(result != Z_STREAM_END)
    {
      result = ZIP_DEFLATE_FAILED;
      goto err_free;
    }

    // Test if we have the space to write
    if(zp->hasspace)
    {
      if(!zp->hasspace(writeLen, fp))
      {
        result = ZIP_ALLOC_MORE_SPACE;
        goto err_free;
      }
    }

    // Write
    if(!zp->vwrite(buffer, writeLen, 1, fp))
    {
      result = ZIP_WRITE_ERROR;
      goto err_free;
    }

    free(buffer);
  }

  else
  {
    result = ZIP_UNSUPPORTED_COMPRESSION;
    goto err_free;
  }

  if(fh)
  {
    // Update the stream
    fh->uncompressed_size += srcLen;
    zp->stream_crc32 = zip_crc32(zp->stream_crc32, src, srcLen);
    zp->stream_left += writeLen;
  }

  return ZIP_SUCCESS;

err_free:
  free(buffer);

err_out:
  zip_error("zwrite", result);
  return result;
}


/* Writes the data descriptor for a file, unless data descriptors are turned
 * off, in which case, it goes back and adds the data into the local header.
 */

static inline enum zip_error zip_write_data_descriptor(struct zip_archive *zp,
 struct zip_file_header *fh)
{
  void (*vputd)(int, void *) = zp->vputd;
  void *fp = zp->fp;

#ifdef ZIP_WRITE_DATA_DESCRIPTOR
  {
    // Write data descriptor
    vputd(fh->crc32, fp);
    vputd(fh->compressed_size, fp);
    vputd(fh->uncompressed_size, fp);
  }
#else
  {
    // Go back and write sizes and CRC32
    int (*vseek)(void *, long int, int) = zp->vseek;
    int return_position = zp->vtell(fp);

    if(vseek(fp, zp->stream_crc_position, SEEK_SET))
    {
      return ZIP_SEEK_ERROR;
    }

    vputd(fh->crc32, fp);
    vputd(fh->compressed_size, fp);
    vputd(fh->uncompressed_size, fp);

    if(vseek(fp, return_position, SEEK_SET))
    {
      return ZIP_SEEK_ERROR;
    }
  }
#endif // !ZIP_WRITE_DATA_DESCRIPTOR

  return ZIP_SUCCESS;
}


/* Open a file writing stream. This works in raw writing mode; when the file
 * write is done, the archive will be in file writing mode.
 */

enum zip_error zip_write_open_file_stream(struct zip_archive *zp,
 const char *name, int method)
{
  struct zip_file_header *fh;
  char *file_name;

  void *fp;

  enum zip_error result;

  result = (zp ? zp->write_file_error : ZIP_NULL);
  if(result)
    goto err_out;

  fp = zp->fp;

  // memfiles: make sure there's enough space for the header
  if(zp->hasspace && !zp->hasspace(strlen(name) + 30, fp))
  {
    result = ZIP_ALLOC_MORE_SPACE;
    goto err_out;
  }

  // Check to make sure we're using a valid method
  if(method != ZIP_M_NONE && method != ZIP_M_DEFLATE)
  {
    result = ZIP_UNSUPPORTED_COMPRESSION;
    goto err_out;
  }

  fh = cmalloc(sizeof(struct zip_file_header));
  file_name = cmalloc(strlen(name) + 1);

  fp = zp->fp;

  // Set up the header
#ifdef ZIP_WRITE_DATA_DESCRIPTOR
  fh->flags = ZIP_F_DATA_DESCRIPTOR;
#else
  fh->flags = 0;
#endif
  fh->method = method;
  fh->crc32 = 0;
  fh->compressed_size = 0;
  fh->uncompressed_size = 0;
  fh->offset = zp->vtell(fp);
  fh->file_name_length = strlen(name);
  fh->file_name = file_name;
  strcpy(file_name, name);

  // Write the header
  result = zip_write_file_header(zp, fh, 0);
  if(result)
    goto err_free;

  // Set up the stream
  zp->mode = ZIP_S_WRITE_STREAM;
  zp->streaming_file = fh;
  zp->stream_left = 0;
  zp->stream_crc32 = 0;

  precalculate_write_errors(zp);
  return ZIP_SUCCESS;

err_free:
  free(fh);
  free(file_name);
  zp->streaming_file = NULL;

err_out:
  zip_error("zip_write_open_file_stream", result);
  return result;
}


/* Close a file writing stream. */

enum zip_error zip_write_close_stream(struct zip_archive *zp)
{
  struct zip_file_header *fh;
  enum zip_error result;

  result = (zp ? zp->write_stream_error : ZIP_NULL);
  if(result)
    goto err_out;

  // Add missing fields to the header.
  fh = zp->streaming_file;
  fh->crc32 = zp->stream_crc32;
  fh->compressed_size = zp->stream_left;

  // Write the missing fields to the local header.
  result = zip_write_data_descriptor(zp, fh);
  if(result)
    goto err_out;

  // Put the file header into the zip archive
  if(zp->pos == zp->files_alloc)
  {
    int count = zp->files_alloc * 2;
    zp->files = crealloc(zp->files, count * sizeof(struct zip_file_header *));
    zp->files_alloc = count;
  }
  zp->running_file_name_length += fh->file_name_length;
  zp->files[zp->pos] = fh;
  zp->num_files++;
  zp->pos++;

  // Clean up the stream
  zp->mode = ZIP_S_WRITE_FILES;
  zp->streaming_file = NULL;
  zp->stream_left = 0;
  zp->stream_crc32 = 0;

  precalculate_write_errors(zp);
  return ZIP_SUCCESS;

err_out:
  zip_error("zip_write_close_stream", result);
  return result;
}


/* Open a direct write stream to the archive's memfile. The only supported
 * method is no compression/store.
 */

enum zip_error zip_write_open_mem_stream(struct zip_archive *zp,
 struct memfile *mf, const char *name)
{
  struct zip_file_header *fh;
  char *file_name;

  void *fp;

  enum zip_error result;

  result = (zp ? zp->write_file_error : ZIP_NULL);
  if(result)
    goto err_out;

  if(!zp->hasspace)
  {
    result = ZIP_NOT_MEMORY_ARCHIVE;
    goto err_out;
  }

  fp = zp->fp;

  // Make sure there's enough space for the header
  if(!zp->hasspace(strlen(name) + 30, fp))
  {
    result = ZIP_ALLOC_MORE_SPACE;
    goto err_out;
  }

  fh = cmalloc(sizeof(struct zip_file_header));
  file_name = cmalloc(strlen(name) + 1);

  fp = zp->fp;

  // Set up the header
#ifdef ZIP_WRITE_DATA_DESCRIPTOR
  fh->flags = ZIP_F_DATA_DESCRIPTOR;
#else
  fh->flags = 0;
#endif
  fh->method = ZIP_M_NONE;
  fh->crc32 = 0;
  fh->compressed_size = 0;
  fh->uncompressed_size = 0;
  fh->offset = zp->vtell(fp);
  fh->file_name_length = strlen(name);
  fh->file_name = file_name;
  strcpy(file_name, name);

  // Write the header
  result = zip_write_file_header(zp, fh, 0);
  if(result)
    goto err_free;

  // Set up the stream
  zp->mode = ZIP_S_WRITE_MEMSTREAM;
  zp->streaming_file = fh;
  zp->stream_left = 0;
  zp->stream_crc32 = 0;

  precalculate_write_errors(zp);

  mf->start = ((struct memfile *)fp)->current;
  mf->end = ((struct memfile *)fp)->end;
  mf->current = mf->start;

  return ZIP_SUCCESS;

err_free:
  free(fh);
  free(file_name);
  zp->streaming_file = NULL;

err_out:
  zip_error("zip_write_open_file_stream", result);
  mfopen_static(NULL, 0, mf);
  return result;
}

enum zip_error zip_write_close_mem_stream(struct zip_archive *zp,
 struct memfile *mf)
{
  struct zip_file_header *fh;

  unsigned char *start;
  unsigned char *end;
  uint32_t length;
  uint32_t crc32;

  enum zip_error result;

  result =
   !zp ? ZIP_NULL :
   zp->closing ? ZIP_INVALID_WHILE_CLOSING :
   zp->mode != ZIP_S_WRITE_MEMSTREAM ? ZIP_INVALID_STREAM_WRITE :
   ZIP_SUCCESS;

  if(result)
    goto err_out;

  // Get length
  start = mf->start;
  end = mf->current;
  length = end - start;

  // Calculate CRC32
  crc32 = zip_crc32(0, start, length);

  // Add missing fields to the header.
  fh = zp->streaming_file;
  fh->crc32 = crc32;
  fh->compressed_size = length;
  fh->uncompressed_size = length;

  // Increment the program position (since we used direct writing)
  ((struct memfile *)(zp->fp))->current = mf->current;

  // Write the missing fields to the local header.
  result = zip_write_data_descriptor(zp, fh);
  if(result)
    goto err_out;

  // Put the file header into the zip archive
  if(zp->pos == zp->files_alloc)
  {
    int count = zp->files_alloc * 2;
    zp->files = crealloc(zp->files, count * sizeof(struct zip_file_header *));
    zp->files_alloc = count;
  }
  zp->running_file_name_length += fh->file_name_length;
  zp->files[zp->pos] = fh;
  zp->num_files++;
  zp->pos++;

  // Clean up the stream
  zp->mode = ZIP_S_WRITE_FILES;
  zp->streaming_file = NULL;
  zp->stream_left = 0;
  zp->stream_crc32 = 0;

  precalculate_write_errors(zp);
  return ZIP_SUCCESS;

err_out:
  zip_error("zip_write_close_mem_stream", result);
  return result;
}


/* Write a file to a zip archive. The first time this is used, the archive will
 * change to file writing mode, after which zip_write() will fail.
 * Valid methods: ZIP_M_NONE, ZIP_M_DEFLATE
 */

enum zip_error zip_write_file(struct zip_archive *zp, const char *name,
 const void *src, size_t srcLen, int method)
{
  enum zip_error result;

  // No need to check mode; the functions used here will

  result = zip_write_open_file_stream(zp, name, method);

  if(result)
    goto err_out;

  result = zwrite(src, srcLen, zp);
  if(result && result != ZIP_EOF)
    goto err_close;

  result = zip_write_close_stream(zp);
  if(result)
    goto err_out;

  return ZIP_SUCCESS;

err_close:
  zip_write_close_stream(zp);

err_out:
  zip_error("zip_write_file", result);
  return result;
}


/* Reads the central directory of a zip archive. This places the archive into
 * file read mode; read files using zip_read_file(). If this fails, the input
 * is probably not actually a zip archive, or uses features we don't support.
 */

static char eocd_sig[] = {
  0x50,
  0x4b,
  0x05,
  0x06
};

static signed char eocd_tbl[] = {
  5, 5, 5, 5, 5, 3 ,4 ,5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 2 ,5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
};

static enum zip_error zip_find_eocd(struct zip_archive *zp)
{
  int (*vseek)(void *, long int, int) = zp->vseek;
  int (*vgetc)(void *) = zp->vgetc;

  void *fp = zp->fp;

  int i;
  int j;
  int n;

  // Go to the latest position the EOCD might be.
  if(vseek(fp, (long int)(zp->end_in_file - 22), SEEK_SET))
  {
    return ZIP_NO_EOCD;
  }

  // Find the end of central directory signature.
  i = 0;
  j = -22;
  do
  {
    n = vgetc(fp);
    j++;

    if(n == eocd_sig[i])
    {
      i++;
      if(i == 4)
        // We've found the EOCD signature
        break;
    }

    else
    {
      if(n < 0)
        break;

      i = i ? i + 5 : eocd_tbl[n];
      j -= i;

      if(vseek(fp, -i, SEEK_CUR))
      {
        i = 0;
        break;
      }

      i = 0;
    }
  }
  // Max length of signature + comment.
  while(j >= -65557);

  if(i < 4)
  {
    return ZIP_NO_EOCD;
  }

  return ZIP_SUCCESS;
}

enum zip_error zip_read_directory(struct zip_archive *zp)
{
  int i;
  int n;
  int result;

  void *fp;

  int (*vseek)(void *, long int, int);
  int (*vgetw)(void *);
  int (*vgetd)(void *);

  if(!zp)
  {
    result = ZIP_NULL;
    goto err_out;
  }

  if(zp->closing)
  {
    result = ZIP_INVALID_WHILE_CLOSING;
    goto err_out;
  }

  // This only works in raw reading mode
  if(zp->mode != ZIP_S_READ_RAW)
  {
    if(zp->mode == ZIP_S_READ_FILES)
    {
      result = ZIP_INVALID_DIRECTORY_READ_IN_FILE_MODE;
      goto err_out;
    }
    result = ZIP_INVALID_WRITE_IN_READ_MODE;
    goto err_out;
  }

  fp = zp->fp;
  vseek = zp->vseek;
  vgetw = zp->vgetw;
  vgetd = zp->vgetd;

  // This will take us to the start of the EOCD
  result = zip_find_eocd(zp);
  if(result)
    goto err_out;

  // Number of this disk
  n = vgetw(fp);
  if(n < 0)
  {
    result = ZIP_READ_ERROR;
    goto err_out;
  }
  else

  if(n > 0)
  {
    result = ZIP_UNSUPPORTED_MULTIPLE_DISKS;
    goto err_out;
  }

  // Disk where central directory starts
  // Number of central directory records on this disk
  if(vseek(fp, 4, SEEK_CUR))
  {
    result = ZIP_SEEK_ERROR;
    goto err_out;
  }

  // Total number of central directory records
  n = vgetw(fp);
  if(n < 0)
  {
    result = ZIP_READ_ERROR;
    goto err_out;
  }
  zp->num_files = n;

  // Size of central directory (bytes)
  zp->size_central_directory = vgetd(fp);

  // Offset of start of central directory, relative to start of file
  zp->offset_central_directory = vgetd(fp);

  // Load central directory records
  if(n)
  {
    struct zip_file_header **f = ccalloc(n, sizeof(struct zip_file_header *));
    zp->files = f;

    // Go to the start of the central directory.
    if(vseek(fp, zp->offset_central_directory, SEEK_SET))
    {
      result = ZIP_SEEK_ERROR;
      goto err_realloc;
    }

    for(i = 0; i < n; i++)
    {
      f[i] = cmalloc(sizeof(struct zip_file_header));
      zp->files_alloc++;

      result = zip_read_file_header(zp, f[i], 0, 1);
      if(result)
      {
        free(f[i]);
        f[i] = NULL;
        zp->files_alloc--;
        break;
      }
    }

    if(zp->files_alloc == 0)
    {
      result = ZIP_NO_CENTRAL_DIRECTORY;
      goto err_realloc;
    }

    if(zp->files_alloc < n)
    {
      result = ZIP_INCOMPLETE_CENTRAL_DIRECTORY;
      goto err_realloc;
    }
  }

  // We're in file read mode now.
  zp->mode = ZIP_S_READ_FILES;
  precalculate_read_errors(zp);

  // At this point, we're probably at the EOCD. Reading files will seek
  // to the start of their respective entries, so just leave it alone.
  return ZIP_SUCCESS;

err_realloc:
  zp->num_files = zp->files_alloc;

  if(zp->files_alloc)
  {
    zp->files =
     crealloc(zp->files, zp->files_alloc * sizeof(struct zip_file_header *));
  }

  else
  {
    free(zp->files);
    zp->files = NULL;
  }

  // We're in file read mode now.
  zp->mode = ZIP_S_READ_FILES;
  precalculate_read_errors(zp);

err_out:
  zip_error("zip_read_directory", result);
  return result;
}


/* Writes the EOCD during the archive close. */

static enum zip_error zip_write_eocd_record(struct zip_archive *zp)
{
  int i;

  void *fp = zp->fp;

  int (*vputc)(int, void *) = zp->vputc;
  void (*vputw)(int, void *) = zp->vputw;
  void (*vputd)(int, void *) = zp->vputd;

  // Memfiles: make sure there's enough space
  if(zp->hasspace && !zp->hasspace(22, fp))
    return ZIP_ALLOC_MORE_SPACE;

  // Signature
  for(i = 0; i<4; i++)
  {
    if(vputc(eocd_sig[i], fp) == EOF)
      return ZIP_WRITE_ERROR;
  }

  // Number of this disk
  vputw(0, fp);

  // Disk where central directory starts
  vputw(0, fp);

  // Number of central directory records on this disk
  vputw(zp->num_files, fp);

  // Total number of central directory records
  vputw(zp->num_files, fp);

  // Size of central directory
  vputd(zp->size_central_directory, fp);

  // Offset of central directory
  vputd(zp->offset_central_directory, fp);

  // Comment length
  vputw(0, fp);

  // Comment (length is zero)

  // Check for errors.
  if(zp->verror && zp->verror(fp))
    return ZIP_WRITE_ERROR;

  return ZIP_SUCCESS;
}


/* Attempts to close the zip archive, and when writing, constructs the central
 * directory and EOCD record. Upon return, *final_length will be set to either
 * the final length of the file. If ZIP_ALLOC_MORE_SPACE is returned when using
 * with a memfile, *final_length will be the projected total length of the file;
 * reallocate using zip_expand and call zip_close again.
 */

enum zip_error zip_close(struct zip_archive *zp, size_t *final_length)
{
  int result = ZIP_SUCCESS;
  int mode;
  int i;

  void *fp;
  long int (*vtell)(void *);

  if(!zp)
  {
    return ZIP_NULL;
  }

  mode = zp->mode;

  fp = zp->fp;
  vtell = zp->vtell;

  // Before initiating the close, make sure there wasn't an open write stream!
  if(zp->streaming_file && mode == ZIP_S_WRITE_STREAM)
  {
    warn("zip_close called while writing file stream!\n");
    zip_write_close_stream(zp);
  }

  if(!zp->closing)
  {
    zp->closing = 1;
    zp->pos = 0;

    precalculate_read_errors(zp);
    precalculate_write_errors(zp);

    if(mode == ZIP_S_WRITE_FILES)
      zp->offset_central_directory = vtell(fp);
  }

  if(mode == ZIP_S_WRITE_FILES)
  {
    int expected_size = 22 + 46 * zp->num_files + zp->running_file_name_length;

    // Calculate projected file size in case more space is needed
    if(final_length)
      *final_length = zp->offset_central_directory + expected_size;

    // Ensure there's enough space to close the file
    if(zp->hasspace)
    {
      if(!zp->hasspace(expected_size, fp))
      {
        result = ZIP_ALLOC_MORE_SPACE;

        zip_error("zip_close", result);
        return result;
      }
    }
  }

  for(i = zp->pos; i < zp->num_files; i++)
  {
    struct zip_file_header *fh = zp->files[i];

    if(fh)
    {
      // Write the central directory
      if(mode == ZIP_S_WRITE_FILES)
      {
        result = zip_write_file_header(zp, fh, 1);
        if(result)
        {
          // Not much that can be done at this point.
          mode = 0;
        }
      }

      free(fh->file_name);

      free(fh);
    }
  }

  // Write the end of central directory record
  if(mode == ZIP_S_WRITE_FILES || mode == ZIP_S_WRITE_RAW)
  {
    zp->size_central_directory = vtell(fp) - zp->offset_central_directory;

    result = zip_write_eocd_record(zp);

    if(final_length)
      *final_length = vtell(fp);
  }

  else
  {
    if(final_length)
      *final_length = zp->end_in_file;
  }

  zp->vclose(zp->fp);

  free(zp->files);

  free(zp);

  if(result != ZIP_SUCCESS)
    zip_error("zip_close", result);

  return result;
}


/* Perform additional setup for write mode. */

static void zip_init_for_write(struct zip_archive *zp, int num_files)
{
  struct zip_file_header **f;

  f = cmalloc(sizeof(struct zip_file_header) * num_files);

  zp->files_alloc = num_files;
  zp->files = f;

  zp->running_file_name_length = 0;

  zp->mode = ZIP_S_WRITE_RAW;
}


/* Set up a new zip archive struct. */

static struct zip_archive *zip_new_archive(void)
{
  struct zip_archive *zp = cmalloc(sizeof(struct zip_archive));

  zp->files = NULL;

  zp->start_in_file = 0;
  zp->files_alloc = 0;
  zp->closing = 0;
  zp->pos = 0;

  zp->num_files = 0;
  zp->size_central_directory = 0;
  zp->offset_central_directory = 0;

  zp->streaming_file = NULL;
  zp->stream_left = 0;
  zp->stream_crc32 = 0;

  zp->mode = ZIP_S_READ_RAW;

  return zp;
}


/* Configure the zip archive for file reading. TODO: abstract the file
 * functions further so the struct isn't as large?
 */

static struct zip_archive *zip_get_archive_file(FILE *fp)
{
  struct zip_archive *zp = zip_new_archive();
  zp->is_memory = 0;
  zp->fp = fp;
  zp->hasspace = NULL;
  zp->vgetc = (int(*)(void *)) fgetc;
  zp->vgetw = (int(*)(void *)) _fgetw;
  zp->vgetd = (int(*)(void *)) _fgetd;
  zp->vputc = (int(*)(int, void *)) fputc;
  zp->vputw = (void(*)(int, void *)) _fputw;
  zp->vputd = (void(*)(int, void *)) _fputd;
  zp->vread = (size_t(*)(void *, size_t, size_t, void *)) fread;
  zp->vwrite = (size_t(*)(const void *, size_t, size_t, void *)) fwrite;
  zp->vseek = (int(*)(void *, long int, int)) _fseekwrapper;
  zp->vtell = (long int(*)(void *)) _ftellwrapper;
  zp->verror = (int(*)(void *)) ferror;
  zp->vclose = (int(*)(void *)) fclose;
  return zp;
}


/* Open a zip archive located in a file for reading. The archive will be in
 * raw read mode, for use with zip_read(), until zip_read_directory()
 * is called. Afterward, the archive will be in file read mode.
 */

struct zip_archive *zip_open_fp_read(FILE *fp)
{
  if(fp)
  {
    struct zip_archive *zp = zip_get_archive_file(fp);
    long int file_len = _fstatlen(fp);

    if(file_len < 0)
    {
      zip_error("zip_open_fp_read", ZIP_STAT_ERROR);
      fclose(fp);
      free(zp);
      return NULL;
    }

    if(file_len > INT_MAX)
    {
      zip_error("zip_open_fp_read", ZIP_UNSUPPORTED_ZIP64);
      fclose(fp);
      free(zp);
      return NULL;
    }

    zp->end_in_file = (uint32_t)file_len;

    precalculate_read_errors(zp);
    precalculate_write_errors(zp);
    return zp;
  }

  return NULL;
}

struct zip_archive *zip_open_file_read(const char *file_name)
{
  FILE *fp = fopen_unsafe(file_name, "rb");

  return zip_open_fp_read(fp);
}


/* Open a zip archive located in a file for writing. The archive will be in
 * raw write mode, for use with zip_write(), until zip_write_file() is called.
 * Afterward, the archive will be in file write mode.
 */

struct zip_archive *zip_open_fp_write(FILE *fp)
{
  if(fp)
  {
    struct zip_archive *zp = zip_get_archive_file(fp);

    zip_init_for_write(zp, ZIP_DEFAULT_NUM_FILES);

    precalculate_read_errors(zp);
    precalculate_write_errors(zp);
    return zp;
  }

  return NULL;
}

struct zip_archive *zip_open_file_write(const char *file_name)
{
  FILE *fp = fopen_unsafe(file_name, "wb");

  return zip_open_fp_write(fp);
}



/* Configure the zip archive for memory reading. TODO: abstract the file
 * functions further so the struct isn't as large?
 */

static struct zip_archive *zip_get_archive_mem(struct memfile *mf)
{
  struct zip_archive *zp = zip_new_archive();
  zp->is_memory = 1;
  zp->fp = mf;
  zp->hasspace = (int(*)(size_t, void *)) mfhasspace;
  zp->vgetc = (int(*)(void *)) mfgetc;
  zp->vgetw = (int(*)(void *)) mfgetw;
  zp->vgetd = (int(*)(void *)) mfgetd;
  zp->vputc = (int(*)(int, void *)) mfputc;
  zp->vputw = (void(*)(int, void *)) mfputw;
  zp->vputd = (void(*)(int, void *)) mfputd;
  zp->vread = (size_t(*)(void *, size_t, size_t, void *)) mfread;
  zp->vwrite = (size_t(*)(const void *, size_t, size_t, void *)) mfwrite;
  zp->vseek = (int(*)(void *, long int, int)) mfseek;
  zp->vtell = (long int(*)(void *)) mftell;
  zp->verror = NULL;
  zp->vclose = (int(*)(void *)) mfclose;
  return zp;
}


/* Open a zip archive located in a block of memory for reading. The archive
 * will be in raw read mode, for use with zip_read(), until zip_read_directory()
 * is called. Afterward, the archive will be in file read mode.
 */

struct zip_archive *zip_open_mem_read(const void *src, size_t len)
{
  struct zip_archive *zp;
  struct memfile *mf;

  if(src && len > 0)
  {
    mf = mfopen(src, len);
    zp = zip_get_archive_mem(mf);

    zp->end_in_file = len;

    precalculate_read_errors(zp);
    precalculate_write_errors(zp);
    return zp;
  }

  return NULL;
}


/* Open a zip archive located in a block of memory for writing. The archive
 * will be in raw write mode, for use with zip_write(), until zip_write_file()
 * is called. Afterward, the archive will be in file write mode.
 */

struct zip_archive *zip_open_mem_write(void *src, size_t len)
{
  struct zip_archive *zp;
  struct memfile *mf;

  if(src && len > 0)
  {
    mf = mfopen(src, len);
    zp = zip_get_archive_mem(mf);

    zip_init_for_write(zp, ZIP_DEFAULT_NUM_FILES);

    precalculate_read_errors(zp);
    precalculate_write_errors(zp);
    return zp;
  }

  return NULL;
}


/* Expand the allocated size of a zip archive's memfile. Use when a zip
 * write function fails with ZIP_ALLOC_MORE_SPACE.
 */

enum zip_error zip_expand(struct zip_archive *zp, void **src, size_t new_size)
{
  struct memfile *mf;
  uint32_t current_offset;
  unsigned char *start;

  int result;

  if(!zp)
  {
    result = ZIP_NULL;
    goto err_out;
  }

  if(!zp->hasspace)
  {
    result = ZIP_NOT_MEMORY_ARCHIVE;
    goto err_out;
  }

  mf = zp->fp;
  start = mf->start;

  if((uint32_t)(mf->end - start) < new_size)
  {
    current_offset = mf->current - start;

    start = crealloc(start, new_size);
    mf->start = start;
    mf->current = start + current_offset;
    mf->end = start + new_size;

    *src = start;
  }

  return ZIP_SUCCESS;

err_out:
  zip_error("zip_expand", result);
  return result;
}
