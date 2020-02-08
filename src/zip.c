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
// The 3DS has incredibly slow file access that seems to be negatively impacted
// by backwards seeks in particular, so enable data descriptors for it too.
// The Switch may similarly benefit with this.

#if defined(CONFIG_NDS) || defined(CONFIG_3DS) || defined(CONFIG_SWITCH)
#define ZIP_WRITE_DATA_DESCRIPTOR
#define DATA_DESCRIPTOR_LEN 12
#endif

#define ZIP_DEFAULT_NUM_FILES 4

#define ZIP_S_READ_UNINITIALIZED 0
#define ZIP_S_READ_FILES 1
#define ZIP_S_READ_STREAM 2
#define ZIP_S_READ_MEMSTREAM 3
#define ZIP_S_WRITE_UNINITIALIZED 8
#define ZIP_S_WRITE_FILES 9
#define ZIP_S_WRITE_STREAM 10
#define ZIP_S_WRITE_MEMSTREAM 11
#define ZIP_S_ERROR 99

#define LOCAL_FILE_HEADER_LEN 30
#define CENTRAL_FILE_HEADER_LEN 46
#define EOCD_RECORD_LEN 22

/**
 * This zip reader/writer was designed:
 *
 * 1) Around the needs of our world/board/MZM files. These files need
 * binary headers (or at least it's nice to have them). The way they're
 * split up also requires fairly fast zip reading and rewriting that
 * minimizes file IO function usage.
 *
 * 2) With the ability to use a memory buffer instead of a file. This is
 * mainly for MZMs, which need to be writable to memory.
 *
 * While not copied from or even based on ajs's zipio, a few pointers were
 * taken from it. While zips can't currently be read and modified at the
 * same time, this would be a potentially useful future addition with
 * implications for downver or accessing files/worlds from inside of a zip.
 */

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
    case ZIP_INVALID_READ_IN_WRITE_MODE:
      return "can't read in write mode";
    case ZIP_INVALID_WRITE_IN_READ_MODE:
      return "can't write in read mode";
    case ZIP_INVALID_FILE_READ_UNINITIALIZED:
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
      return "could not find or read central directory";
    case ZIP_INCOMPLETE_CENTRAL_DIRECTORY:
      return "central directory is missing records";
    case ZIP_UNSUPPORTED_MULTIPLE_DISKS:
      return "unsupported multiple volume archive";
    case ZIP_UNSUPPORTED_FLAGS:
      return "unsupported flags";
    case ZIP_UNSUPPORTED_COMPRESSION:
      return "unsupported method; use DEFLATE or none";
    case ZIP_UNSUPPORTED_ZIP64:
      return "unsupported ZIP64 data";
    case ZIP_UNSUPPORTED_DEFLATE_STREAM:
      return "DEFLATE: can only stream whole file";
    case ZIP_MISSING_LOCAL_HEADER:
      return "could not find file header";
    case ZIP_HEADER_MISMATCH:
      return "local header mismatch";
    case ZIP_CRC32_MISMATCH:
      return "CRC-32 mismatch; validation failed";
    case ZIP_INFLATE_FAILED:
      return "decompression failed";
    case ZIP_DEFLATE_FAILED:
      return "compression failed";
  }
  warn("zip_error_string: received unknown error code %d!\n", code);
  return "UNKNOWN ERROR";
}

static void zip_error(const char *func, enum zip_error code)
{
  warn("%s: %s\n", func, zip_error_string(code));
}

/**
 * Wrappers for zlib functions.
 */

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
  memset(&stream, 0, sizeof(z_stream));
  stream.next_in = (Bytef *) src;
  stream.avail_in = (uInt) *srcLen;

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

  memset(&stream, 0, sizeof(z_stream));

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

/**
 * Calculate an upper bound for the total compressed size of a memory block.
 */
int zip_bound_data_usage(char *src, int srcLen)
{
  z_stream stream;
  int bound;

  memset(&stream, 0, sizeof(z_stream));
  stream.next_in = (Bytef *) src;
  stream.avail_in = (uInt) srcLen;

  // Note: aside from the windowbits, these are all defaults
  deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS,
   8, Z_DEFAULT_STRATEGY);

  bound = deflateBound(&stream, (uLong) srcLen);

  deflateEnd(&stream);

  return bound;
}

/**
 * Calculate an upper bound for the total size of headers for an archive.
 */
int zip_bound_total_header_usage(int num_files, int max_name_size)
{
  // Expected:
  // max_name_size = 8 for MZX world data
  //               = 5 for MZM data
  int extra = 0;

#ifdef ZIP_WRITE_DATA_DESCRIPTOR
  extra = num_files * DATA_DESCRIPTOR_LEN;      // data descriptor size
#endif

  return num_files *
   // base + file name
   (LOCAL_FILE_HEADER_LEN + max_name_size +     // Local
    CENTRAL_FILE_HEADER_LEN + max_name_size) +  // Central directory
    EOCD_RECORD_LEN + extra;                    // EOCD record
}


/* Basic checks to make sure various functions can actually be used. */

static inline enum zip_error zip_read_file_mode_check(struct zip_archive *zp)
{
  switch(zp->mode)
  {
    case ZIP_S_READ_FILES:          return ZIP_SUCCESS;
    case ZIP_S_READ_STREAM:         return ZIP_INVALID_FILE_READ_IN_STREAM_MODE;
    case ZIP_S_READ_MEMSTREAM:      return ZIP_INVALID_FILE_READ_IN_STREAM_MODE;
    case ZIP_S_READ_UNINITIALIZED:  return ZIP_INVALID_FILE_READ_UNINITIALIZED;
    default:                        return ZIP_INVALID_READ_IN_WRITE_MODE;
  }
}

static inline enum zip_error zip_read_stream_mode_check(struct zip_archive *zp)
{
  switch(zp->mode)
  {
    case ZIP_S_READ_STREAM:         return ZIP_SUCCESS;
    case ZIP_S_READ_FILES:          return ZIP_INVALID_STREAM_READ;
    case ZIP_S_READ_MEMSTREAM:      return ZIP_INVALID_STREAM_READ;
    case ZIP_S_READ_UNINITIALIZED:  return ZIP_INVALID_FILE_READ_UNINITIALIZED;
    default:                        return ZIP_INVALID_READ_IN_WRITE_MODE;
  }
}

static inline enum zip_error zip_write_file_mode_check(struct zip_archive *zp)
{
  switch(zp->mode)
  {
    case ZIP_S_WRITE_FILES:         return ZIP_SUCCESS;
    case ZIP_S_WRITE_STREAM:        return ZIP_INVALID_FILE_WRITE_IN_STREAM_MODE;
    case ZIP_S_WRITE_MEMSTREAM:     return ZIP_INVALID_FILE_WRITE_IN_STREAM_MODE;
    case ZIP_S_WRITE_UNINITIALIZED: return ZIP_SUCCESS;
    default:                        return ZIP_INVALID_WRITE_IN_READ_MODE;
  }
}

static inline enum zip_error zip_write_stream_mode_check(struct zip_archive *zp)
{
  switch(zp->mode)
  {
    case ZIP_S_WRITE_STREAM:        return ZIP_SUCCESS;
    case ZIP_S_WRITE_FILES:         return ZIP_INVALID_STREAM_WRITE;
    case ZIP_S_WRITE_MEMSTREAM:     return ZIP_INVALID_STREAM_WRITE;
    case ZIP_S_WRITE_UNINITIALIZED: return ZIP_INVALID_STREAM_WRITE;
    default:                        return ZIP_INVALID_WRITE_IN_READ_MODE;
  }
}

static inline void precalculate_read_errors(struct zip_archive *zp)
{
  zp->read_file_error = zip_read_file_mode_check(zp);
  zp->read_stream_error = zip_read_stream_mode_check(zp);
}

static inline void precalculate_write_errors(struct zip_archive *zp)
{
  zp->write_file_error = zip_write_file_mode_check(zp);
  zp->write_stream_error = zip_write_stream_mode_check(zp);
}


static char file_sig_local[] =
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

static uint32_t data_descriptor_sig = 0x08074b50;

static enum zip_error zip_read_file_header_signature(struct zip_archive *zp,
 boolean is_central)
{
  vfile *vf = zp->vf;
  int n, i;

  char *magic = is_central ? file_sig_central : file_sig_local;

  // Find the next file header
  i = 0;
  while(1)
  {
    n = vfgetc(vf);
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

  return ZIP_SUCCESS;
}

static enum zip_error zip_read_central_file_header(struct zip_archive *zp,
 struct zip_file_header *central_fh)
{
  enum zip_error result;
  char buffer[CENTRAL_FILE_HEADER_LEN];
  struct memfile mf;

  int skip_length;
  int method;
  int flags;
  int n;

  central_fh->file_name = NULL;

  result = zip_read_file_header_signature(zp, true);
  if(result)
    return result;

  // We already read four
  if(!vfread(buffer, CENTRAL_FILE_HEADER_LEN - 4, 1, zp->vf))
    return ZIP_READ_ERROR;

  mfopen(buffer, CENTRAL_FILE_HEADER_LEN - 4, &mf);

  // Version made by              2
  // Version needed to extract    2
  mf.current += 4;

  // General purpose bit flag     2

  flags = mfgetw(&mf);

  if((flags & ~ZIP_F_ALLOWED) != 0)
  {
    warn(
      "Zip using unsupported options "
      "(allowing %d, found %d -- unsupported: %d).\n",
      ZIP_F_ALLOWED,
      flags,
      flags & ~ZIP_F_ALLOWED
    );
    return ZIP_UNSUPPORTED_FLAGS;
  }
  central_fh->flags = flags;

  // Compression method           2

  method = mfgetw(&mf);

  if(method < 0)
    return ZIP_READ_ERROR;

  if(method != ZIP_M_NONE && method != ZIP_M_DEFLATE)
    return ZIP_UNSUPPORTED_COMPRESSION;

  central_fh->method = method;

  // File last modification time  2
  // File last modification date  2
  mf.current += 4;

  // CRC-32, sizes                12

  central_fh->crc32 = mfgetd(&mf);
  central_fh->compressed_size = mfgetd(&mf);
  central_fh->uncompressed_size = mfgetd(&mf);

  if(central_fh->compressed_size == 0xFFFFFFFF)
    return ZIP_UNSUPPORTED_ZIP64;

  if(central_fh->uncompressed_size == 0xFFFFFFFF)
    return ZIP_UNSUPPORTED_ZIP64;

  // File name length             2
  // Extra field length           2
  // File comment length          2

  n = mfgetw(&mf);
  central_fh->file_name_length = n;
  skip_length = mfgetw(&mf) + mfgetw(&mf);

  // Disk number of file start    2
  // Internal file attributes     2
  // External file attributes     4
  mf.current += 8;

  // Offset to local header       4 (from start of archive)
  central_fh->offset = mfgetd(&mf);

  // File name (n)
  central_fh->file_name = cmalloc(n + 1);
  vfread(central_fh->file_name, n, 1, zp->vf);
  central_fh->file_name[n] = 0;

  // Done. Skip to the position where the next header should be.
  if(skip_length && vfseek(zp->vf, skip_length, SEEK_CUR))
    return ZIP_SEEK_ERROR;

  return ZIP_SUCCESS;
}

static enum zip_error zip_verify_local_file_header(struct zip_archive *zp,
 struct zip_file_header *central_fh)
{
  enum zip_error result;
  char buffer[LOCAL_FILE_HEADER_LEN];
  struct memfile mf;

  uint32_t crc32;
  uint32_t compressed_size;
  uint32_t uncompressed_size;
  int data_position;
  int flags;

  result = zip_read_file_header_signature(zp, false);
  if(result)
    return result;

  // We already read four
  if(!vfread(buffer, LOCAL_FILE_HEADER_LEN - 4, 1, zp->vf))
    return ZIP_READ_ERROR;

  mfopen(buffer, LOCAL_FILE_HEADER_LEN - 4, &mf);

  // Version made by              2
  mf.current += 2;

  // General purpose bit flag     2
  flags = mfgetw(&mf);

  // Compression method           2
  // File last modification time  2
  // File last modification date  2
  mf.current += 6;

  if(!(flags & ZIP_F_DATA_DESCRIPTOR))
  {
    // Normal.

    // CRC-32, sizes              12
    crc32 = mfgetd(&mf);
    compressed_size = mfgetd(&mf);
    uncompressed_size = mfgetd(&mf);

    // File name length           2
    // Extra field length         2

    data_position = vftell(zp->vf) + mfgetw(&mf) + mfgetw(&mf);

    // Done.
  }

  else
  {
    // With data descriptor.
    char dd_buffer[16];
    struct memfile dd_mf;

    // CRC-32, sizes              12
    mf.current += 12;

    // File name length           2
    // Extra field length         2

    data_position = vftell(zp->vf) + mfgetw(&mf) + mfgetw(&mf);

    // CRC-32, sizes              12

    vfseek(zp->vf, data_position + central_fh->compressed_size, SEEK_SET);
    vfread(dd_buffer, 16, 1, zp->vf);
    mfopen(dd_buffer, 16, &dd_mf);

    // The data descriptor may or may not have an optional signature field,
    // meaning it may be either 12 or 16 bytes long.
    crc32 = mfgetd(&dd_mf);
    compressed_size = mfgetd(&dd_mf);
    uncompressed_size = mfgetd(&dd_mf);

    if(crc32 == data_descriptor_sig &&
     compressed_size == central_fh->crc32 &&
     uncompressed_size == central_fh->compressed_size)
    {
      // If the first value is the data descriptor signature and we can verify
      // that we've read the expected crc32 and compressed size where they
      // should be, it's safe to assume this is a 16-byte data descriptor.
      // In this case, shift the values.
      crc32 = compressed_size;
      compressed_size = uncompressed_size;
      uncompressed_size = mfgetd(&dd_mf);
    }

    // Done.
  }

  // Verify values are correct.
  if(crc32 != central_fh->crc32 ||
   compressed_size != central_fh->compressed_size ||
   uncompressed_size != central_fh->uncompressed_size)
    return ZIP_HEADER_MISMATCH;

  if(vfseek(zp->vf, data_position, SEEK_SET))
    return ZIP_SEEK_ERROR;

  return ZIP_SUCCESS;
}

static enum zip_error zip_write_file_header(struct zip_archive *zp,
 struct zip_file_header *fh, int is_central)
{
  enum zip_error result = ZIP_SUCCESS;

  char *magic;
  char *buffer;
  struct memfile mf;
  int header_size;
  int i;

  if(is_central)
  {
    // Position to write CRC, sizes after file write
    zp->stream_crc_position = vftell(zp->vf) + 16;
    header_size = fh->file_name_length + CENTRAL_FILE_HEADER_LEN;
    magic = file_sig_central;
  }
  else
  {
    zp->stream_crc_position = vftell(zp->vf) + 14;
    header_size = fh->file_name_length + LOCAL_FILE_HEADER_LEN;
    magic = file_sig_local;
  }

  buffer = cmalloc(header_size);
  mfopen(buffer, header_size, &mf);

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

  if(!vfwrite(buffer, header_size, 1, zp->vf))
    result = ZIP_WRITE_ERROR;

  // Memfile zips - we know there's enough space, because it was already
  // checked in zip_write_file or zip_close

  free(buffer);
  return result;
}

/***********/
/* Reading */
/***********/

/**
 * Read data from a zip archive. Only works while streaming a file.
 */
enum zip_error zread(void *destBuf, size_t readLen, struct zip_archive *zp)
{
  struct zip_file_header *fh;
  char *src;

  uint32_t consumed;

  enum zip_error result;

  result = zp->read_stream_error;
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

    if(!vfread(destBuf, consumed, 1, zp->vf))
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

    if(!vfread(src, c_size, 1, zp->vf))
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

/**
 * Get the name of the next file in the archive.
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

/**
 * Get the MZX properties of the next file in the archive.
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

/**
 * Get the uncompressed length of the next file in the archive.
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

/**
 * Get the compression method of the next file in the archive.
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

/**
 * Common function for zip stream opening.
 * mode should be either ZIP_S_READ_STREAM or ZIP_S_READ_MEMSTREAM.
 */
static enum zip_error zip_read_stream_open(struct zip_archive *zp, uint8_t mode)
{
  struct zip_file_header *central_fh;

  uint32_t c_size;
  uint16_t method;

  uint32_t read_pos;
  enum zip_error result;

  result = (zp ? zp->read_file_error : ZIP_NULL);
  if(result)
    return result;

  if(zp->pos >= zp->num_files)
    return ZIP_EOF;

  central_fh = zp->files[zp->pos];

  c_size = central_fh->compressed_size;
  method = central_fh->method;

  // Special mem stream checks.
  if(mode == ZIP_S_READ_MEMSTREAM)
  {
    if(!zp->is_memory)
      return ZIP_NOT_MEMORY_ARCHIVE;

    if(method == ZIP_M_DEFLATE)
      return ZIP_UNSUPPORTED_DEFLATE_STREAM;
  }

  // Check for unsupported methods
  if(method != ZIP_M_NONE && method != ZIP_M_DEFLATE)
    return ZIP_UNSUPPORTED_COMPRESSION;

  // Seek to the start of the record
  read_pos = vftell(zp->vf);
  if(read_pos != central_fh->offset)
  {
    if(vfseek(zp->vf, central_fh->offset, SEEK_SET))
      return ZIP_SEEK_ERROR;
  }

  // Verify the local header matches the central directory header.
  result = zip_verify_local_file_header(zp, central_fh);
  if(result)
    return result;

  // Everything looks good. Set up stream mode.
  zp->mode = mode;
  zp->streaming_file = central_fh;
  zp->stream_left = c_size;
  zp->stream_crc32 = 0;

  precalculate_read_errors(zp);
  return ZIP_SUCCESS;
}

/**
 * Open a stream to read the next file from a zip archive. If provided, destLen
 * will be the uncompressed size of the file on return (or 0 upon error).
 */
enum zip_error zip_read_open_file_stream(struct zip_archive *zp,
 size_t *destLen)
{
  enum zip_error result = zip_read_stream_open(zp, ZIP_S_READ_STREAM);
  if(result)
    goto err_out;

  if(destLen)
    *destLen = zp->streaming_file->uncompressed_size;

  return ZIP_SUCCESS;

err_out:
  if(result != ZIP_EOF)
    zip_error("zip_read_open_file_stream", result);
  if(destLen)
    *destLen = 0;
  return result;
}

/**
 * Like zip_read_open_file_stream, but allows the direct reading of
 * uncompressed files. This is abusable, but quicker than the alternatives.
 *
 * ZIP_NOT_MEMORY_ARCHIVE or ZIP_UNSUPPORTED_DEFLATE_STREAM will be silently
 * returned if the current archive or file doesn't support this; use regular
 * functions instead in this case.
 */
enum zip_error zip_read_open_mem_stream(struct zip_archive *zp,
 struct memfile *mf)
{
  enum zip_error result = zip_read_stream_open(zp, ZIP_S_READ_MEMSTREAM);
  if(result)
    goto err_out;

  mfopen(vfile_get_memfile(zp->vf)->current,
   zp->streaming_file->compressed_size, mf);
  return ZIP_SUCCESS;

err_out:
  if(result != ZIP_EOF &&
   result != ZIP_NOT_MEMORY_ARCHIVE &&
   result != ZIP_UNSUPPORTED_DEFLATE_STREAM)
    zip_error("zip_read_open_mem_stream", result);
  mfopen(NULL, 0, mf);
  return result;
}

/**
 * Shared function for closing both types of zip read streams.
 */
enum zip_error zip_read_close_stream(struct zip_archive *zp)
{
  uint32_t expected_crc32;
  uint32_t stream_crc32;
  uint32_t stream_left;
  char buffer[128];
  int size;

  enum zip_error result;

  // mem streams need special cleanup but are mostly the same.
  if(zp && zp->mode == ZIP_S_READ_MEMSTREAM)
  {
    struct memfile *mf = vfile_get_memfile(zp->vf);
    uint32_t c_size = zp->streaming_file->compressed_size;

    zp->read_stream_error = ZIP_SUCCESS;
    zp->stream_left = 0;
    zp->stream_crc32 = zip_crc32(0, mf->current, c_size);
  }

  result = (zp ? zp->read_stream_error : ZIP_NULL);
  if(result)
    goto err_out;

  expected_crc32 = zp->streaming_file->crc32;
  stream_crc32 = zp->stream_crc32;

  // If the stream was incomplete, finish the crc32
  stream_left = zp->stream_left;
  while(stream_left)
  {
    size = MIN(128, stream_left);
    vfread(buffer, size, 1, zp->vf);
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

/**
 * Rewind to the start of the zip archive.
 */
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

/**
 * Skip the current file in the zip archive.
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

/**
 * Read a file from the a zip archive. If provided, the value of readLen will
 * be set to the number of bytes read into the buffer.
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

/**
 * Check if there's enough room in a memory archive to write a number of bytes.
 * If there isn't and the buffer is expandable, expand it to fit the required
 * size. Otherwise, return ZIP_EOF.
 */
static enum zip_error zip_ensure_capacity(size_t len, struct zip_archive *zp)
{
  struct memfile *mf = vfile_get_memfile(zp->vf);
  void *external_buffer;
  size_t external_buffer_size;
  size_t size_required;

  if(mfhasspace(len, mf))
    return ZIP_SUCCESS;

  if(!zp->external_buffer || !zp->external_buffer_size)
    return ZIP_EOF;

  size_required = mftell(mf) + len;
  external_buffer = *(zp->external_buffer);
  external_buffer_size = *(zp->external_buffer_size);

  while(external_buffer_size < size_required)
    external_buffer_size *= 2;

  external_buffer = crealloc(external_buffer, external_buffer_size);
  *(zp->external_buffer) = external_buffer;
  *(zp->external_buffer_size) = external_buffer_size;

  mfmove(external_buffer, external_buffer_size, mf);
  return ZIP_SUCCESS;
}

/**
 * Write data to a zip archive. Only works in stream mode.
 */
enum zip_error zwrite(const void *src, size_t srcLen, struct zip_archive *zp)
{
  struct zip_file_header *fh;
  char *buffer = NULL;
  uint32_t writeLen;
  uint16_t method = ZIP_M_NONE;

  enum zip_error result;

  result = (zp ? zp->write_stream_error : ZIP_NULL);
  if(result)
    goto err_out;

  if(!srcLen) return ZIP_SUCCESS;

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
    if(zp->is_memory && zip_ensure_capacity(srcLen, zp))
    {
      result = ZIP_EOF;
      goto err_out;
    }

    if(!vfwrite(src, srcLen, 1, zp->vf))
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
    if(zp->is_memory && zip_ensure_capacity(writeLen, zp))
    {
      result = ZIP_EOF;
      goto err_free;
    }

    // Write
    if(!vfwrite(buffer, writeLen, 1, zp->vf))
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

/**
 * Writes the data descriptor for a file. If data descriptors are turned
 * off, this function will seek back and add the data into the local header.
 */
static inline enum zip_error zip_write_data_descriptor(struct zip_archive *zp,
 struct zip_file_header *fh)
{
  char buffer[12];
  struct memfile mf;

  mfopen(buffer, 12, &mf);
  mfputd(fh->crc32, &mf);
  mfputd(fh->compressed_size, &mf);
  mfputd(fh->uncompressed_size, &mf);

#ifdef ZIP_WRITE_DATA_DESCRIPTOR
  {
    // Write data descriptor
    if(zp->is_memory && zip_ensure_capacity(DATA_DESCRIPTOR_LEN, zp))
      return ZIP_EOF;

    vfwrite(buffer, 12, 1, zp->vf);
  }
#else
  {
    // Go back and write sizes and CRC32
    int return_position = vftell(zp->vf);

    if(vfseek(zp->vf, zp->stream_crc_position, SEEK_SET))
      return ZIP_SEEK_ERROR;

    if(!vfwrite(buffer, 12, 1, zp->vf))
      return ZIP_WRITE_ERROR;

    if(vfseek(zp->vf, return_position, SEEK_SET))
      return ZIP_SEEK_ERROR;
  }
#endif // !ZIP_WRITE_DATA_DESCRIPTOR

  return ZIP_SUCCESS;
}

/**
 * Common function for zip write stream opening.
 * mode should be either ZIP_S_WRITE_STREAM or ZIP_S_WRITE_MEMSTREAM.
 */
static enum zip_error zip_write_open_stream(struct zip_archive *zp,
 const char *name, int method, uint8_t mode)
{
  struct zip_file_header *fh;
  char *file_name;
  uint16_t file_name_len;

  enum zip_error result;

  result = (zp ? zp->write_file_error : ZIP_NULL);
  if(result)
    return result;

  // Special mem stream checks.
  if(mode == ZIP_S_WRITE_MEMSTREAM)
  {
    if(!zp->is_memory)
      return ZIP_NOT_MEMORY_ARCHIVE;

    // Sanity check (this shouldn't actually happen ever).
    if(method == ZIP_M_DEFLATE)
      return ZIP_UNSUPPORTED_DEFLATE_STREAM;
  }

  // memfiles: make sure there's enough space for the header
  if(zp->is_memory && zip_ensure_capacity(strlen(name) + 30, zp))
    return ZIP_EOF;

  // Check to make sure this is a valid method.
  if(method != ZIP_M_NONE && method != ZIP_M_DEFLATE)
    return ZIP_UNSUPPORTED_COMPRESSION;

  fh = cmalloc(sizeof(struct zip_file_header));
  file_name_len = strlen(name);
  file_name = cmalloc(file_name_len + 1);

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
  fh->offset = vftell(zp->vf);
  fh->file_name_length = file_name_len;
  fh->file_name = file_name;
  memcpy(file_name, name, file_name_len + 1);

  // Write the header
  result = zip_write_file_header(zp, fh, 0);
  if(result)
  {
    free(fh);
    free(file_name);
    zp->streaming_file = NULL;
    return result;
  }

  // Set up the stream
  zp->mode = mode;
  zp->streaming_file = fh;
  zp->stream_left = 0;
  zp->stream_crc32 = 0;

  precalculate_write_errors(zp);
  return ZIP_SUCCESS;
}

/**
 * Open a file writing stream to write the next file in the zip archive.
 */
enum zip_error zip_write_open_file_stream(struct zip_archive *zp,
 const char *name, int method)
{
  enum zip_error result = zip_write_open_stream(zp, name, method,
   ZIP_S_WRITE_STREAM);
  if(result)
    goto err_out;

  return ZIP_SUCCESS;

err_out:
  zip_error("zip_write_open_file_stream", result);
  return result;
}

/**
 * Like zip_write_open_file_stream, but allows the direct writing of
 * uncompressed files to memory (deflate is not supported by this method).
 * This is abusable, but quicker than the alternatives.
 *
 * ZIP_NOT_MEMORY_ARCHIVE will be silently returned if the current archive
 * doesn't support this; use regular functions instead in this case.
 */
enum zip_error zip_write_open_mem_stream(struct zip_archive *zp,
 struct memfile *mf, const char *name)
{
  enum zip_error result = zip_write_open_stream(zp, name, ZIP_M_NONE,
   ZIP_S_WRITE_MEMSTREAM);
  struct memfile *zp_mf;
  if(result)
    goto err_out;

  zp_mf = vfile_get_memfile(zp->vf);

  mf->start = zp_mf->current;
  mf->end = zp_mf->end;
  mf->current = mf->start;
  return ZIP_SUCCESS;

err_out:
  if(result != ZIP_NOT_MEMORY_ARCHIVE &&
   result != ZIP_UNSUPPORTED_DEFLATE_STREAM)
    zip_error("zip_write_open_file_stream", result);
  mfopen(NULL, 0, mf);
  return result;
}

/**
 * Close a file writing stream.
 */
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

/**
 * Close a memory writing stream. Unlike with reading, this currently needs its
 * own function still (to receive the memfile pointer indicating the write).
 */
enum zip_error zip_write_close_mem_stream(struct zip_archive *zp,
 struct memfile *mf)
{
  unsigned char *start = mf->start;
  unsigned char *end = mf->current;
  uint32_t length = end - start;

  enum zip_error result =
   !zp ? ZIP_NULL :
   zp->mode != ZIP_S_WRITE_MEMSTREAM ? ZIP_INVALID_STREAM_WRITE :
   ZIP_SUCCESS;

  if(result)
    goto err_out;

  // Compute some things that a regular stream would have here but are missing
  // from mem streams.
  zp->stream_crc32 = zip_crc32(0, start, length);
  zp->stream_left = length;
  zp->streaming_file->uncompressed_size = length;

  // Increment the program position (since direct writing was used)
  vfseek(zp->vf, mf->current - mf->start, SEEK_CUR);

  // Now the regular function can be used...
  zp->write_stream_error = ZIP_SUCCESS;
  return zip_write_close_stream(zp);

err_out:
  zip_error("zip_write_close_mem_stream", result);
  return result;
}

/**
 * Write a file to a zip archive.
 * Currently handled methods: ZIP_M_NONE, ZIP_M_DEFLATE
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

/**
 * Reads the central directory of a zip archive. This places the archive into
 * file read mode; read files using zip_read_file(). If this fails, the input
 * is probably not actually a zip archive or uses features we don't support.
 */

static char eocd_sig[] = {
  0x50,
  0x4b,
  0x05,
  0x06
};

static enum zip_error zip_find_eocd(struct zip_archive *zp)
{
  vfile *vf = zp->vf;
  int i;
  int j;
  int n;

  // Go to the latest position the EOCD might be.
  if(vfseek(vf, (long int)(zp->end_in_file - EOCD_RECORD_LEN), SEEK_SET))
  {
    return ZIP_NO_EOCD;
  }

  // Find the end of central directory signature.
  i = 0;
  j = -EOCD_RECORD_LEN;
  do
  {
    n = vfgetc(vf);
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

      i = i ? i + 5 :
       (n == 0x06 ? 4 :
        n == 0x05 ? 3 :
        n == 0x4b ? 2 : 5);
      j -= i;

      if(vfseek(vf, -i, SEEK_CUR))
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

/**
 * Read the EOCD record and central directory of the zip to memory.
 * This is required before the zip can be properly read.
 */
static enum zip_error zip_read_directory(struct zip_archive *zp)
{
  char buffer[EOCD_RECORD_LEN];
  struct memfile mf;
  int i;
  int n;
  int result;

  // Find the EOCD record signature                     4
  result = zip_find_eocd(zp);
  if(result)
    goto err_out;

  // Already read the first 4 signature bytes.
  if(!vfread(buffer, EOCD_RECORD_LEN - 4, 1, zp->vf))
  {
    result = ZIP_READ_ERROR;
    goto err_out;
  }
  mfopen(buffer, EOCD_RECORD_LEN - 4, &mf);

  // Number of this disk                                2
  n = mfgetw(&mf);
  if(n > 0)
  {
    result = ZIP_UNSUPPORTED_MULTIPLE_DISKS;
    goto err_out;
  }

  // Disk where central directory starts                2
  // Number of central directory records on this disk   2
  mf.current += 4;

  // Total number of central directory records          2
  n = mfgetw(&mf);
  zp->num_files = n;

  // Size of central directory (bytes)                  4
  zp->size_central_directory = mfgetd(&mf);

  // Offset to central directory from start of file     4
  zp->offset_central_directory = mfgetd(&mf);

  // Comment length (ignore)                            2

  // Load central directory records
  if(n)
  {
    struct zip_file_header **f = ccalloc(n, sizeof(struct zip_file_header *));
    zp->files = f;

    // Go to the start of the central directory.
    if(vfseek(zp->vf, zp->offset_central_directory, SEEK_SET))
    {
      result = ZIP_SEEK_ERROR;
      goto err_realloc;
    }

    for(i = 0; i < n; i++)
    {
      f[i] = cmalloc(sizeof(struct zip_file_header));
      zp->files_alloc++;

      result = zip_read_central_file_header(zp, f[i]);
      if(result)
      {
        free(f[i]);
        f[i] = NULL;
        zp->files_alloc--;
        zip_error("error reading central directory record", result);
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
      warn("expected %d central directory records but only found %d\n",
       n, zp->files_alloc);
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

/**
 * Write the EOCD during the archive close.
 */
static enum zip_error zip_write_eocd_record(struct zip_archive *zp)
{
  char buffer[EOCD_RECORD_LEN];
  struct memfile mf;
  int i;

  // Memfiles: make sure there's enough space
  if(zp->is_memory && zip_ensure_capacity(EOCD_RECORD_LEN, zp))
    return ZIP_EOF;

  mfopen(buffer, EOCD_RECORD_LEN, &mf);

  // Signature                                          4
  for(i = 0; i<4; i++)
    mfputc(eocd_sig[i], &mf);

  // Number of this disk                                2
  mfputw(0, &mf);

  // Disk where central directory starts                2
  mfputw(0, &mf);

  // Number of central directory records on this disk   2
  mfputw(zp->num_files, &mf);

  // Total number of central directory records          2
  mfputw(zp->num_files, &mf);

  // Size of central directory                          4
  mfputd(zp->size_central_directory, &mf);

  // Offset of central directory                        4
  mfputd(zp->offset_central_directory, &mf);

  // Comment length                                     2
  mfputw(0, &mf);

  // Comment (length is zero)

  if(!vfwrite(buffer, EOCD_RECORD_LEN, 1, zp->vf))
    return ZIP_WRITE_ERROR;

  return ZIP_SUCCESS;
}

/**
 * Attempts to close the zip archive, and when writing, constructs the central
 * directory and EOCD record. Upon returning ZIP_SUCCESS, *final_length will be
 * set to the final length of the file.
 */
enum zip_error zip_close(struct zip_archive *zp, size_t *final_length)
{
  int result = ZIP_SUCCESS;
  int mode;
  int i;

  if(!zp)
  {
    return ZIP_NULL;
  }

  mode = zp->mode;

  // Before initiating the close, make sure there wasn't an open write stream!
  if(zp->streaming_file && mode == ZIP_S_WRITE_STREAM)
  {
    warn("zip_close called while writing file stream!\n");
    zip_write_close_stream(zp);
    mode = ZIP_S_WRITE_FILES;
  }

  if(mode == ZIP_S_WRITE_FILES)
  {
    int expected_size = 22 + 46 * zp->num_files + zp->running_file_name_length;

    zp->offset_central_directory = vftell(zp->vf);

    // Calculate projected file size in case more space is needed
    if(final_length)
      *final_length = zp->offset_central_directory + expected_size;

    // Ensure there's enough space to close the file
    if(zp->is_memory && zip_ensure_capacity(expected_size, zp))
    {
      result = ZIP_EOF;
      mode = ZIP_S_ERROR;
    }
  }

  zp->pos = 0;
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
          mode = ZIP_S_ERROR;
        }
      }

      free(fh->file_name);
      free(fh);
    }
  }

  // Write the end of central directory record
  if(mode == ZIP_S_WRITE_FILES || mode == ZIP_S_WRITE_UNINITIALIZED)
  {
    size_t end_pos;
    zp->size_central_directory = vftell(zp->vf) - zp->offset_central_directory;

    result = zip_write_eocd_record(zp);
    end_pos = vftell(zp->vf);

    if(final_length)
      *final_length = end_pos;

    // Reduce the size of the buffer if this is an expandable memory zip.
    if(zp->is_memory && zp->external_buffer && zp->external_buffer_size &&
     *(zp->external_buffer_size) > end_pos)
    {
      *(zp->external_buffer) = crealloc(*(zp->external_buffer), end_pos);
      *(zp->external_buffer_size) = end_pos;
    }
  }

  else
  {
    if(final_length)
      *final_length = zp->end_in_file;
  }

  vfclose(zp->vf);

  free(zp->files);
  free(zp);

  if(result != ZIP_SUCCESS)
    zip_error("zip_close", result);

  return result;
}

/**
 * Perform additional setup for write mode.
 */
static void zip_init_for_write(struct zip_archive *zp, int num_files)
{
  struct zip_file_header **f;

  f = cmalloc(sizeof(struct zip_file_header) * num_files);

  zp->files_alloc = num_files;
  zp->files = f;

  zp->running_file_name_length = 0;

  zp->mode = ZIP_S_WRITE_UNINITIALIZED;
}

/**
 * Set up a new zip archive struct.
 */
static struct zip_archive *zip_new_archive(void)
{
  struct zip_archive *zp = cmalloc(sizeof(struct zip_archive));
  zp->is_memory = false;

  zp->files = NULL;

  zp->start_in_file = 0;
  zp->files_alloc = 0;
  zp->pos = 0;

  zp->num_files = 0;
  zp->size_central_directory = 0;
  zp->offset_central_directory = 0;

  zp->streaming_file = NULL;
  zp->stream_left = 0;
  zp->stream_crc32 = 0;

  zp->external_buffer = NULL;
  zp->external_buffer_size = NULL;

  zp->mode = ZIP_S_READ_UNINITIALIZED;

  return zp;
}

/**
 * Open a zip archive located in a file for reading. Returns a zip_archive
 * pointer if this archive is ready for file reading; otherwise, returns
 * NULL. On failure, this will also close the file pointer.
 */
struct zip_archive *zip_open_fp_read(FILE *fp)
{
  if(fp)
  {
    struct zip_archive *zp = zip_new_archive();
    long int file_len = _fstatlen(fp);

    zp->vf = vfile_init_fp(fp, "rb");

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

    if(ZIP_SUCCESS != zip_read_directory(zp))
    {
      zip_close(zp, NULL);
      return NULL;
    }

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

/**
 * Open a zip archive located in a file for writing. The archive will be in
 * raw write mode, for use with zip_write(), until zip_write_file() is called.
 * Afterward, the archive will be in file write mode.
 */
struct zip_archive *zip_open_fp_write(FILE *fp)
{
  if(fp)
  {
    struct zip_archive *zp = zip_new_archive();

    zp->vf = vfile_init_fp(fp, "wb");

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

/**
 * Open a zip archive located in memory for reading. Returns a zip_archive
 * pointer if this archive is ready for file reading; otherwise, returns
 * NULL.
 */
struct zip_archive *zip_open_mem_read(const void *src, size_t len)
{
  if(src && len > 0)
  {
    struct zip_archive *zp = zip_new_archive();

    zp->vf = vfile_init_mem((void *)src, len, "rb");
    zp->is_memory = true;
    zp->end_in_file = len;

    if(ZIP_SUCCESS != zip_read_directory(zp))
    {
      zip_close(zp, NULL);
      return NULL;
    }

    precalculate_read_errors(zp);
    precalculate_write_errors(zp);
    return zp;
  }
  return NULL;
}

/**
 * Open a zip archive for writing to a block of memory. Returns a zip_archive
 * upon success; otherwise, returns NULL. An optional offset can be specified
 * indicated the position in the file to begin writing ZIP data.
 */
struct zip_archive *zip_open_mem_write(void *src, size_t len, size_t start_pos)
{
  if(src && len > 0 && start_pos < len)
  {
    struct zip_archive *zp = zip_new_archive();

    zp->vf = vfile_init_mem(src, len, "wb");
    zp->is_memory = true;

    zip_init_for_write(zp, ZIP_DEFAULT_NUM_FILES);
    vfseek(zp->vf, start_pos, SEEK_SET);

    precalculate_read_errors(zp);
    precalculate_write_errors(zp);
    return zp;
  }
  return NULL;
}

/**
 * Open a zip archive for writing to a block of memory. See the above function.
 *
 * The locations described by external_buffer and external_buffer_size must be
 * initialized before this function is called. If this archive exceeds the size
 * of its buffer, it will automatically attempt to reallocate the buffer.
 */
struct zip_archive *zip_open_mem_write_ext(void **external_buffer,
 size_t *external_buffer_size, size_t start_pos)
{
  struct zip_archive *zp =
   zip_open_mem_write(*external_buffer, *external_buffer_size, start_pos);

  if(zp)
  {
    zp->external_buffer = external_buffer;
    zp->external_buffer_size = external_buffer_size;
  }

  return zp;
}
