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

#include <stdio.h>
#include <time.h>

#include <zlib.h>

#include "zip.h"

#include "error.h"
#include "platform.h"
#include "world.h"
#include "world_struct.h"
#include "util.h"


#define ZIP_VERSION 20
#define ZIP_VERSION_MINIMUM 20

#define ZIP_DEFAULT_NUM_FILES 4

#define ZIP_S_READ_RAW 0
#define ZIP_S_READ_FILES 1
#define ZIP_S_READ_STREAM 2
#define ZIP_S_WRITE_RAW 3
#define ZIP_S_WRITE_FILES 4
#define ZIP_S_WRITE_STREAM 5

#define ZIP_MZX_PROP_MAGIC  0x785A  // "Zx"
#define ZIP_MZX_PROP_SIZE   8       // [prop x4][board x2][robot x2]

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


static int zip_get_dos_date_time(void)
{
  time_t current_time = time(NULL);
  struct tm *tm = localtime(&current_time);

  Uint16 time;
  Uint16 date;

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
    case ZIP_INVALID_EXPAND:
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
  char buffer[80];
  snprintf(buffer, 80, "%s: %s", func, zip_error_string(code));
  error_message(E_ZIP, 0, buffer);
}


static inline Uint32 zip_crc32(Uint32 crc, const char *src, Uint32 srcLen)
{
  return crc32(crc, (Bytef *) src, (uLong) srcLen);
}

static int zip_uncompress(char *dest, Uint32 *destLen, const char *src,
 Uint32 *srcLen)
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

static int zip_compress(char **dest, Uint32 *destLen, const char *src,
 Uint32 *srcLen)
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

  return num_files *
   // base + file name     extra field header + data
   (30 + max_name_size +   4 + ZIP_MZX_PROP_SIZE +    // Local
    46 + max_name_size +   4 + ZIP_MZX_PROP_SIZE) +   // Central directory
    22;                                               // EOCD record
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
 struct zip_file_header *fh, int is_central)
{
  int n, m, k, i;
  int method;
  int flags;

  void *fp = zp->fp;

  int (*vseek)(void *, int, int) = zp->vseek;
  int (*vgetc)(void *) = zp->vgetc;
  int (*vgetw)(void *) = zp->vgetw;
  int (*vgetd)(void *) = zp->vgetd;
  int (*vread)(void *, size_t, size_t, void *) = zp->vread;

  char *magic = is_central ? file_sig_central : file_sig;

  fh->file_name = NULL;

  // Find the next file header
  i = 0;
  while(1)
  {
    // I don't really like this solution, but...
    // Memfiles: make sure there's enough space
    if(zp->hasspace && !zp->hasspace(30-i, fp))
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
    if(n == magic[0])
      i = 1;

    else
      i = 0;
  }

  // version made by
  if(vseek(fp, 2, SEEK_CUR))
    return ZIP_SEEK_ERROR;

  // version needed to extract (Central-only)
  if(is_central)
    if(vseek(fp, 2, SEEK_CUR))
      return ZIP_SEEK_ERROR;

  // general purpose bit flag
  flags = vgetw(fp);
  if(flags < 0)
  {
    return ZIP_READ_ERROR;
  }
  else
  if(flags & ZIP_F_DATA_DESCRIPTOR)
  {
    // Supported flag - don't error out
  }
  else
  if(flags != 0)
  {
    warn("Zip using unsupported options (%d) -- use 0.\n", flags);
    return ZIP_UNSUPPORTED_FLAGS;
  }
  fh->flags = flags;

  // compression method
  method = vgetw(fp);
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
  
  // file last modification time (2)
  // file last modification date (2)
  if(vseek(fp, 4, SEEK_CUR))
    return ZIP_SEEK_ERROR;

  // crc32
  fh->crc32 = vgetd(fp);

  // compressed size
  fh->compressed_size = vgetd(fp);
  if(fh->compressed_size == 0xFFFFFFFF)
    return ZIP_UNSUPPORTED_ZIP64;

  // uncompressed size
  fh->uncompressed_size = vgetd(fp);
  if(fh->uncompressed_size == 0xFFFFFFFF)
    return ZIP_UNSUPPORTED_ZIP64;

  // file name length
  n = vgetw(fp);
  fh->file_name_length = n;

  // extra field length
  m = vgetw(fp);

  // Central record-only fields
  if(is_central)
  {
    // file comment length
    k = vgetw(fp);

    // disk number where file starts (2)
    // internal file attributes (2)
    // external file attributes (4)
    if(vseek(fp, 8, SEEK_CUR))
      return ZIP_SEEK_ERROR;

    // offset to local header from start of archive
    fh->offset = vgetd(fp);

    // File name
    fh->file_name = cmalloc(n + 1);
    vread(fh->file_name, n, 1, fp);
    fh->file_name[n] = 0;

    // Extra field (m)
    if(m)
    {
      int left = m;
      int len;
      int id;

      fh->mzx_prop_id = 0;
      fh->mzx_board_id = 0;
      fh->mzx_robot_id = 0;

      while(left>0)
      {
        if(left < 4)
          break;

        id = vgetw(fp);
        len = vgetw(fp);
        left -= 4;

        if(len > left)
          break;

        if(id == ZIP_MZX_PROP_MAGIC)
        {
          fh->mzx_prop_id = vgetd(fp);
          fh->mzx_board_id = vgetw(fp);
          fh->mzx_robot_id = vgetw(fp);
          left -= ZIP_MZX_PROP_SIZE;
          break;
        }

        // else skip and continue
        vseek(fp, len, SEEK_CUR);
        left -= len;
      }

      // Correct our position to the start of the comment
      vseek(fp, left, SEEK_CUR);
    }

    // File comment (k)
    if(vseek(fp, k, SEEK_CUR))
      return ZIP_SEEK_ERROR;
  }

  // Local record
  else
  {
    // File name (n) (we might want this for better validation later...)
    // Extra field (m)
    if(vseek(fp, n+m, SEEK_CUR))
      return ZIP_SEEK_ERROR;

    // We should be at the start of the data now.
    // We might need to look at the data descriptor, though.
    if(flags & ZIP_F_DATA_DESCRIPTOR)
    {
      Uint32 expected_c_size = fh->compressed_size;

      // Find the data descriptor
      if(vseek(fp, expected_c_size, SEEK_CUR))
        return ZIP_MISSING_DATA_DESCRIPTOR;

      // crc32
      fh->crc32 = vgetd(fp);

      // compressed size
      fh->compressed_size = vgetd(fp);
      if(fh->compressed_size == 0xFFFFFFFF)
        return ZIP_UNSUPPORTED_ZIP64;

      // uncompressed size
      fh->uncompressed_size = vgetd(fp);
      if(fh->uncompressed_size == 0xFFFFFFFF)
        return ZIP_UNSUPPORTED_ZIP64;

      if(zp->verror && zp->verror(fp))
        return ZIP_READ_ERROR;

      // Go back to the start of the file.
      vseek(fp, -expected_c_size-12, SEEK_CUR);
    }
  }

  return ZIP_SUCCESS;
}


static enum zip_error zip_write_file_header(struct zip_archive *zp,
 struct zip_file_header *fh, int is_central)
{
  int i;

  void *fp = zp->fp;

  int (*vputc)(int, void *) = zp->vputc;
  void (*vputw)(int, void *) = zp->vputw;
  void (*vputd)(int, void *) = zp->vputd;
  int (*vwrite)(const void *, size_t, size_t, void *) = zp->vwrite;

  char *magic = is_central ? file_sig_central : file_sig;

  // Memfiles - we know there's enough space, because it was already
  // checked in zip_write_file or zip_close

  // Signature
  for(i = 0; i<4; i++)
  {
    if(vputc(magic[i], fp) == EOF)
      return ZIP_WRITE_ERROR;
  }

  // Version made by
  vputw(ZIP_VERSION, fp);

  // Version needed to extract (central directory only)
  if(is_central)
    vputw(ZIP_VERSION_MINIMUM, fp);

  // General purpose bit flag
  vputw(fh->flags, fp);

  // Compression method
  vputw(fh->method, fp);

  // File last modification time
  // File last modification date
  vputd(zip_get_dos_date_time(), fp);

  // CRC-32
  vputd(fh->crc32, fp);

  // Compressed size
  vputd(fh->compressed_size, fp);

  // Uncompressed size
  vputd(fh->uncompressed_size, fp);

  // File name length
  vputw(fh->file_name_length, fp);

  // Extra field length
  vputw(4 + ZIP_MZX_PROP_SIZE, fp);

  // (central directory only fields)
  if(is_central)
  {
    // File comment length
    vputw(0, fp);

    // Disk number where file starts
    vputw(0, fp);

    // Internal file attributes
    vputw(0, fp);

    // External file attributes
    vputd(0, fp);

    // Relative offset of local file header
    vputd(fh->offset, fp);
  }

  // File name
  if(!vwrite(fh->file_name, fh->file_name_length, 1, fp))
    return ZIP_WRITE_ERROR;

  // Extra field
  vputw(ZIP_MZX_PROP_MAGIC, fp);
  vputw(ZIP_MZX_PROP_SIZE, fp);
  vputd(fh->mzx_prop_id, fp);
  vputw(fh->mzx_board_id, fp);
  vputw(fh->mzx_robot_id, fp);

  // File commend (is zero bytes)

  // Check for errors.
  if(zp->verror && zp->verror(fp))
    return ZIP_WRITE_ERROR;

  return ZIP_SUCCESS;
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

enum zip_error zread(void *destBuf, Uint32 readLen, struct zip_archive *zp)
{
  struct zip_file_header *fh;
  char *src;

  Uint32 consumed;

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
    Uint32 u_size = fh ? fh->uncompressed_size : 0;
    Uint32 c_size = fh ? fh->compressed_size : 0;

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


/* Get the MZX properties of the next file in the archive. Only works after
 * zip_read_directory() is called.
 */

enum zip_error zip_get_next_prop(struct zip_archive *zp,
 Uint32 *prop_id, char *board_id, char *robot_id)
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
  zip_error("zip_get_next_mzx_prop", result);
  return result;
}


/* Get the uncompressed length of the next file in the archive. Only works after
 * zip_read_directory() is called.
 */

enum zip_error zip_get_next_uncompressed_size(struct zip_archive *zp,
 Uint32 *u_size)
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
  zip_error("zip_get_next_u_size", result);
  return result;
}


/* Open a stream to read the next file from a zip archive. Only works after
 * zip_read_directory() is called. If provided, destLen will be the uncompressed
 * size of the file on return, or 0 if an error occurred.
 */

enum zip_error zip_read_open_file_stream(struct zip_archive *zp,
 char *name, int name_buffer_size, Uint32 *destLen)
{
  struct zip_file_header *central_fh;
  struct zip_file_header local_fh;
  void *fp;

  Uint32 f_crc32;
  Uint32 c_size;
  Uint32 u_size;
  Uint16 method;

  Uint32 read_pos;
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

  // Copy the file name, if requested
  if(name && name_buffer_size)
  {
    name_buffer_size = MIN(name_buffer_size, central_fh->file_name_length);
    memcpy(name, central_fh->file_name, name_buffer_size);
    name[name_buffer_size] = 0;
  }

  f_crc32 = central_fh->crc32;
  c_size = central_fh->compressed_size;
  u_size = central_fh->uncompressed_size;
  method = central_fh->method;

  // Check for unsupported methods
  if(method == ZIP_M_NONE)
  {
    //debug("File: %s type: no compression\n", central_fh->file_name);
  }
  else if (method == ZIP_M_DEFLATE)
  {
    //debug("File: %s type: DEFLATE\n", central_fh->file_name);
  }
  else
  {
    result = ZIP_UNSUPPORTED_COMPRESSION;
    goto err_out;
  }

  // Seek to the start of the record
  fp = zp->fp;
  read_pos = zp->vtell(fp);
  if(read_pos != central_fh->offset)
  {
    if(zp->vseek(fp, central_fh->offset - read_pos, SEEK_CUR))
    {
      result = ZIP_SEEK_ERROR;
      goto err_out;
    }
  }

  // Read the local header -- but first, give it the expected
  // compressed size just in the case there's a data descriptor.

  local_fh.compressed_size = c_size;
  result = zip_read_file_header(zp, &local_fh, 0);
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
  Uint32 expected_crc32;
  Uint32 stream_crc32;
  Uint32 stream_left;
  char buffer[128];
  int size;

  int (*vread)(void *, size_t, size_t, void *);
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
    warn("crc check: expected %x, got %x\n", expected_crc32, stream_crc32);
    result = ZIP_CRC32_MISMATCH;
    goto err_out;
  }

  return ZIP_SUCCESS;

err_out:
  zip_error("zip_read_close_stream", result);
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
  zip_error("zip_skip", result);
  return result;
}


/* Read a file from the a zip archive. Only works after zip_read_directory()
 * is called. If provided, readLen will return the actual number of bytes read.
 */

enum zip_error zip_read_file(struct zip_archive *zp, char *name,
 int nameLen, void *destBuf, Uint32 destLen, Uint32 *readLen)
{
  Uint32 u_size;
  enum zip_error result;

  // No need to check mode; the functions used here will

  // We can't accept a NULL pointer here, though
  if(!destBuf)
  {
    result = ZIP_NULL_BUF;
    goto err_out;
  }

  result = zip_read_open_file_stream(zp, name, nameLen, &u_size);
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

enum zip_error zwrite(const void *src, Uint32 srcLen, struct zip_archive *zp)
{
  struct zip_file_header *fh;
  char *buffer;
  Uint32 writeLen;
  Uint16 method = ZIP_M_NONE;

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
    Uint32 consumed = srcLen;

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


/* Open a file writing stream. This works in raw writing mode; when the file
 * write is done, the archive will be in file writing mode.
 */

enum zip_error zip_write_open_file_stream(struct zip_archive *zp,
 const char *name, int method, Uint32 prop_id, char board_id,
 char robot_id)
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
  if(zp->hasspace && !zp->hasspace(strlen(name) + 30 + 4 + ZIP_MZX_PROP_SIZE, fp))
  {
    result = ZIP_ALLOC_MORE_SPACE;
    goto err_out;
  }

  // Check to make sure we're using a valid method
  // No compression
  if(method == ZIP_M_NONE)
  {
    //debug("File: %s type: no compression\n", name);
  }

  else
  // DEFLATE
  if(method == ZIP_M_DEFLATE)
  {
    //debug("File: %s type: DEFLATE\n", name);
  }

  else
  {
    result = ZIP_UNSUPPORTED_COMPRESSION;
    goto err_out;
  }

  fh = cmalloc(sizeof(struct zip_file_header));
  file_name = cmalloc(strlen(name) + 1);

  fp = zp->fp;

  // Set up the header
  fh->mzx_prop_id = prop_id;
  fh->mzx_board_id = board_id;
  fh->mzx_robot_id = robot_id;

  fh->flags = 0;
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

  void *fp;
  void (*vputd)(int, void *);
  int (*vseek)(void *, int, int);
  int seek_value;

  enum zip_error result;

  result = (zp ? zp->write_stream_error : ZIP_NULL);
  if(result)
    goto err_out;

  // Add missing fields to the header.
  fh = zp->streaming_file;
  fh->crc32 = zp->stream_crc32;
  fh->compressed_size = zp->stream_left;

  // Go back and write sizes and CRC32
  // Could use a data descriptor instead...

  fp = zp->fp;
  vputd = zp->vputd;
  vseek = zp->vseek;

  // Compressed data + file name
  seek_value = fh->compressed_size + fh->file_name_length
  // + extra field + extra field length + file name length
   + (4 + ZIP_MZX_PROP_SIZE) + 2 + 2;

  if(vseek(fp, -12-seek_value, SEEK_CUR))
  {
    result = ZIP_SEEK_ERROR;
    goto err_out;
  }

  vputd(fh->crc32, fp);
  vputd(fh->compressed_size, fp);
  vputd(fh->uncompressed_size, fp);

  if(vseek(fp, seek_value, SEEK_CUR))
  {
    result = ZIP_SEEK_ERROR;
    goto err_out;
  }

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


/* Write a file to a zip archive. The first time this is used, the archive will
 * change to file writing mode, after which zip_write() will fail.
 * Valid methods: ZIP_M_NONE, ZIP_M_DEFLATE
 */

enum zip_error zip_write_file(struct zip_archive *zp, const char *name,
 const void *src, Uint32 srcLen, int method, Uint32 prop_id, char board_id,
 char robot_id)
{
  enum zip_error result;

  // No need to check mode; the functions used here will

  result = zip_write_open_file_stream(zp, name, method, prop_id, board_id,
   robot_id);

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
,
};

static int _zip_header_cmp(const void *a, const void *b)
{
  struct zip_file_header *A = *(struct zip_file_header **)a;
  struct zip_file_header *B = *(struct zip_file_header **)b;
  int ab = A->mzx_board_id;
  int bb = B->mzx_board_id;
  int ar = A->mzx_robot_id;
  int br = B->mzx_robot_id;

  return  (ab!=bb) ? (ab-bb) :
          (ar!=br) ? (ar-br) : (int)A->mzx_prop_id - (int)B->mzx_prop_id;
}

/*
static int _zip_header_cmp(const void *a, const void *b)
{
  return strcmp(
   (*(struct zip_file_header **)a)->file_name,
   (*(struct zip_file_header **)b)->file_name
  );
}
*/

enum zip_error zip_read_directory(struct zip_archive *zp)
{
  int n, i;
  int result;

  void *fp;

  int (*vseek)(void *, int, int);
  int (*vgetc)(void *);
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
  vgetc = zp->vgetc;
  vgetw = zp->vgetw;
  vgetd = zp->vgetd;

  // Go to the end of the file.
  vseek(fp, 0, SEEK_END);

  // Go back to the latest position the EOCD might be.
  if(vseek(fp, -22, SEEK_CUR))
  {
    result = ZIP_NO_EOCD;
    goto err_out;
  }

  // Find the end of central directory signature.
  i = 0;
  while(1)
  {
    n = vgetc(fp);
    if(n < 0)
    {
      result = ZIP_NO_EOCD;
      goto err_out;
    }

    if(n == eocd_sig[i])
    {
      i++;
      if(i == 4)
        // We've found the EOCD signature
        break;
    }

    else
    {
      /* Backtrack (simplified Boyer-Moore)
       * any matched -> jump back 1 minus # matched minus signature length
       * n = 'K' -> jump back -1-1
       * n = 5 -> jump back -1-2
       * n = 6 -> jump back -1-3
       * else  -> jump back -1-4
       */

      i = i      ? -i-1-4 :
          n=='K' ?   -1-1 :
          n==5   ?   -1-2 :
          n==6   ?   -1-3 : -1-4;

      if(vseek(fp, i, SEEK_CUR))
      {
        result = ZIP_NO_EOCD;
        goto err_out;
      }

      i = 0;
    }
  }

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
    struct zip_file_header **f = cmalloc(sizeof(struct zip_file_header *) * n);
    zp->files = f;

    memset(f, 0, sizeof(struct zip_file_header *)*n);

    // Go to the start of the central directory.
    if(vseek(fp, zp->offset_central_directory, SEEK_SET))
    {
      result = ZIP_SEEK_ERROR;
      goto err_out;
    }
      
    for(i = 0; i < n; i++)
    {
      f[i] = cmalloc(sizeof(struct zip_file_header));
      zp->files_alloc++;

      result = zip_read_file_header(zp, f[i], 1);
      if(result)
        goto err_out;
    }

    if(zp->files_alloc == 0)
    {
      result = ZIP_NO_CENTRAL_DIRECTORY;
      goto err_out;
    }

    if(zp->files_alloc < n)
    {
      result = ZIP_INCOMPLETE_CENTRAL_DIRECTORY;
      goto err_out;
    }

    // We want a directory sorted by board and property criteria
    qsort(zp->files, n, sizeof(struct zip_file_header *),
     _zip_header_cmp);
  }

  // We're in file read mode now.
  zp->mode = ZIP_S_READ_FILES;

  // At this point, we're probably at the EOCD. Reading files will seek
  // to the start of their respective entries, so just leave it alone.
  precalculate_read_errors(zp);
  return ZIP_SUCCESS;

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

enum zip_error zip_close(struct zip_archive *zp, Uint32 *final_length)
{
  int result = ZIP_SUCCESS;
  int mode;
  int i;

  void *fp;
  int (*vtell)(void *);

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
    //debug("Finalizing and closing zip file.\n");
    zp->closing = 1;
    zp->pos = 0;

    precalculate_read_errors(zp);
    precalculate_write_errors(zp);

    if(mode == ZIP_S_WRITE_FILES)
      zp->offset_central_directory = vtell(fp);
  }

  if(mode == ZIP_S_WRITE_FILES)
  {
    int expected_size = 22 + (ZIP_MZX_PROP_SIZE+4+46)*zp->num_files
     + zp->running_file_name_length;

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

      if(fh->file_name)
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

  if(zp->files)
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
  zp->fp = fp;
  zp->hasspace = NULL;
  zp->vgetc = (int(*)(void *)) fgetc;
  zp->vgetw = (int(*)(void *)) fgetw;
  zp->vgetd = (int(*)(void *)) fgetd;
  zp->vputc = (int(*)(int, void *)) fputc;
  zp->vputw = (void(*)(int, void *)) fputw;
  zp->vputd = (void(*)(int, void *)) fputd;
  zp->vread = (int(*)(void *, size_t, size_t, void *)) fread;
  zp->vwrite = (int(*)(const void *, size_t, size_t, void *)) fwrite;
  zp->vseek = (int(*)(void *, int, int)) fseek;
  zp->vtell = (int(*)(void *)) ftell;
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
    zp->end_in_file = ftell_and_rewind(fp);

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
  zp->fp = mf;
  zp->hasspace = (int(*)(size_t, void *)) mfhasspace;
  zp->vgetc = (int(*)(void *)) mfgetc;
  zp->vgetw = (int(*)(void *)) mfgetw;
  zp->vgetd = (int(*)(void *)) mfgetd;
  zp->vputc = (int(*)(int, void *)) mfputc;
  zp->vputw = (void(*)(int, void *)) mfputw;
  zp->vputd = (void(*)(int, void *)) mfputd;
  zp->vread = (int(*)(void *, size_t, size_t, void *)) mfread;
  zp->vwrite = (int(*)(const void *, size_t, size_t, void *)) mfwrite;
  zp->vseek = (int(*)(void *, int, int)) mfseek;
  zp->vtell = (int(*)(void *)) mftell;
  zp->verror = NULL;
  zp->vclose = (int(*)(void *)) mfclose;
  return zp;
}


/* Open a zip archive located in a block of memory for reading. The archive
 * will be in raw read mode, for use with zip_read(), until zip_read_directory()
 * is called. Afterward, the archive will be in file read mode.
 */

struct zip_archive *zip_open_mem_read(char *src, Uint32 len)
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

struct zip_archive *zip_open_mem_write(char *src, Uint32 len)
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

enum zip_error zip_expand(struct zip_archive *zp, char **src, Uint32 new_size)
{
  struct memfile *mf;
  Uint32 current_offset;
  char *start;

  int result;

  if(!zp)
  {
    result = ZIP_NULL;
    goto err_out;
  }

  if(!zp->hasspace)
  {
    result = ZIP_INVALID_EXPAND;
    goto err_out;
  }

  mf = zp->fp;
  start = mf->start;

  if((Uint32)(mf->end - start) < new_size)
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


// FIXME REMOVE
#ifdef TEST
void zip_test(struct world *mzx_world)
{
  FILE *fp;
  struct zip_archive *zp;
  char name[32];
  char *zip_file;
  char *data;
  char *off;
  Uint32 zip_len = 0;
  Uint32 len = 32;
  
  enum zip_error result = 0;

  // Read an arbitrary zip from file
  zp = zip_open_file_read("_mzmstrings.zip");

  if(zread(name, len, zp) == ZIP_SUCCESS)
  {
    name[31] = 0;
    debug("Some raw data: %s\n", name);
  }
  result = zip_read_directory(zp);
  if(result == ZIP_SUCCESS)
  {
    while(1)
    {
      result = zip_get_next_uncompressed_size(zp, &len);
      if(result != ZIP_SUCCESS)
      {
        if(result == ZIP_EOF)
          debug("No more files.\n");
        break;
      }

      data = cmalloc(len);
      result = zip_read_file(zp, name, 31, data, len, NULL);

      off = data;
      if(memsafegets(name, 31, &off, data+len))
        debug("Preview: %s\n", name);
      free(data);
    }
  }
  zip_close(zp, NULL);
  printf("\n");

  // Write a zip to memory
  sprintf(name, "Some data.txt");
  len = strlen(name);

  zip_len = zip_bound_total_header_usage(1, len);
  zip_len += len;
  //zip_len += zip_bound_data_usage(name, strlen(name) + 1);
  zip_file = cmalloc(zip_len);

  zp = zip_open_mem_write(zip_file, zip_len);
  
  //debug("(note: writing raw data to throw off the calculated bound)\n");
  //zip_write(zp, name, len);

  zip_write_file(zp, name, name, len, ZIP_M_NONE, 0, 0, 0);

  result = zip_close(zp, &zip_len);
  if(result == ZIP_ALLOC_MORE_SPACE)
  {
    debug("Attempting to expand to %d bytes\n", (int)zip_len);
    zip_expand(zp, &zip_file, zip_len);
    result = zip_close(zp, &zip_len);
    if(result)
      debug("Close failed.\n");
    else
      debug("Successfully closed file.\n");
  }
  printf("\n");

  fp = fopen_unsafe("memtest1", "wb");
  fwrite(zip_file, zip_len, 1, fp);
  fclose(fp);

  // Read a zip from memory
  data = NULL;
  zp = zip_open_mem_read(zip_file, zip_len);
  if(!zip_read_directory(zp))
  {
    result = zip_read_open_file_stream(zp, name, 31, &len);
    if(!result)
    {
      char *pos;

      data = cmalloc(len);
      pos = data;

      while(zread(pos, 1, zp) == ZIP_SUCCESS)
        pos++;

      result = zip_read_close_stream(zp);
      if(result == ZIP_SUCCESS)
      {
        *pos = 0;
        debug("Read file '%s' from stream: %s\n", name, data);
      }
    }
  }
  zip_close(zp, NULL);
  printf("\n");

  free(zip_file);

  // Write a zip to file
  zp = zip_open_file_write((char *)"output.zip");
  zip_write_file(zp, name, data, len, ZIP_M_DEFLATE, 0, 0, 0);
  zip_close(zp, NULL);
  printf("\n");

  free(data);
}
#endif //TEST
