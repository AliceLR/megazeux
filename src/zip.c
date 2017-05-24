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

#include "world.h"
#include "world_struct.h"
#include "util.h"
#include "zip.h"


#define ZIP_VERSION 20
#define ZIP_VERSION_MINIMUM 20

#define ZIP_DEFAULT_NUM_FILES 4

#define ZIP_S_READ_RAW 0
#define ZIP_S_READ_FILES 1
#define ZIP_S_READ_STREAM 2
#define ZIP_S_WRITE_RAW 3
#define ZIP_S_WRITE_FILES 4
#define ZIP_S_WRITE_STREAM 5


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
      return "No error";
    case ZIP_EOF:
      return "Reached end of file";
    case ZIP_NULL:
      return "Function received null archive pointer";
    case ZIP_NULL_BUF:
      return "Function received null buffer pointer";
    case ZIP_SEEK_ERROR:
      return "Could not seek to requested position";
    case ZIP_READ_ERROR:
      return "Could not read from requested position";
    case ZIP_WRITE_ERROR:
      return "Could not write to requested position";
    case ZIP_OPEN_ERROR:
      return "Could not open archive";
    case ZIP_ALLOC_MORE_SPACE:
      return "Input memory block is not large enough to complete the operation";
    case ZIP_INVALID_WHILE_CLOSING:
      return "Invalid operation; archive is currently being closed";
    case ZIP_INVALID_READ_IN_WRITE_MODE:
      return "Archive is in write mode and can not be read from";
    case ZIP_INVALID_WRITE_IN_READ_MODE:
      return "Archive is in read mode and can not be written to";
    case ZIP_INVALID_RAW_READ_IN_FILE_MODE:
      return "Archive is in file mode and can not be directly read from";
    case ZIP_INVALID_RAW_WRITE_IN_FILE_MODE:
      return "Archive is in file mode and can not be directly written to";
    case ZIP_INVALID_DIRECTORY_READ_IN_FILE_MODE:
      return "Directory has already been read";
    case ZIP_INVALID_FILE_READ_IN_RAW_MODE:
      return "Archive is in raw mode; use zip_read_directory before writing files";
    case ZIP_INVALID_FILE_READ_IN_STREAM_MODE:
      return "Archive is in stream mode; close the stream before reading new files";
    case ZIP_INVALID_FILE_WRITE_IN_STREAM_MODE:
      return "Archive is in stream mode; close the stream before writing new files";
    case ZIP_INVALID_STREAM_READ:
      return "Archive is not in stream mode; can't read from or close stream";
    case ZIP_INVALID_STREAM_WRITE:
      return "Archive is not in stream mode; can't write to or close stream";
    case ZIP_INVALID_EXPAND:
      return "Expand can be used on zip archives in memory only";
    case ZIP_NO_EOCD:
      return "Could not find EOCD record; file is not a zip archive or is corrupt";
    case ZIP_NO_CENTRAL_DIRECTORY:
      return "Could not find central directory";
    case ZIP_INCOMPLETE_CENTRAL_DIRECTORY:
      return "Central directory has fewer records than expected";
    case ZIP_UNSUPPORTED_MULTIPLE_DISKS:
      return "Multiple disks archives are unsupported";
    case ZIP_UNSUPPORTED_FLAGS:
      return "File uses unsupported flags; use 0";
    case ZIP_UNSUPPORTED_COMPRESSION:
      return "File uses unsupported compression method; use DEFLATE or no compression";
    case ZIP_UNSUPPORTED_ZIP64:
      return "File contains unsupported ZIP64 data";
    case ZIP_UNSUPPORTED_DEFLATE_STREAM:
      return "Streaming DEFLATE currently only supports using the full file length";
    case ZIP_MISSING_LOCAL_HEADER:
      return "Could not find the local file header";
    case ZIP_MISSING_DATA_DESCRIPTOR:
      return "Could not find the data descriptor";
    case ZIP_HEADER_MISMATCH:
      return "Local header does not match central directory header";
    case ZIP_CRC32_MISMATCH:
      return "CRC-32 mismatch; validation failed";
    case ZIP_INFLATE_FAILED:
      return "Decompression failed";
    case ZIP_DEFLATE_FAILED:
      return "Compression failed";
  }
  return "UNKNOWN ERROR CODE";
}

static void zip_error(const char *func, enum zip_error code)
{
  printf("ERROR: in %s: %s\n", func, zip_error_string(code));
}


static Uint32 zip_crc32(Uint32 crc, char *src, Uint32 srcLen)
{
  return crc32(crc, (Bytef *) src, (uLong) srcLen);
}

static int zip_uncompress(char *dest, Uint32 *destLen, char *src, Uint32 *srcLen)
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

static int zip_compress(char **dest, Uint32 *destLen, char *src, Uint32 *srcLen)
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
   (30 + max_name_size +  // Local header
    46 + max_name_size) + // Central directory header
   22;                    // EOCD record
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

  int (*seek)(void *, int, int) = zp->seek;
  int (*getc)(void *) = zp->getc;
  int (*getw)(void *) = zp->getw;
  int (*getd)(void *) = zp->getd;
  int (*read)(void *, size_t, size_t, void *) = zp->read;

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

    n = getc(fp);
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
  if(seek(fp, 2, SEEK_CUR))
    return ZIP_SEEK_ERROR;

  // version needed to extract (Central-only)
  if(is_central)
    if(seek(fp, 2, SEEK_CUR))
      return ZIP_SEEK_ERROR;

  // general purpose bit flag
  flags = getw(fp);
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
  method = getw(fp);
  if(method < 0)
  {
    return ZIP_READ_ERROR;
  }
  else
  if(method != ZIP_M_NO_COMPRESSION && method != ZIP_M_DEFLATE)
  {
    return ZIP_UNSUPPORTED_COMPRESSION;
  }
  fh->method = method;
  
  // file last modification time (2)
  // file last modification date (2)
  if(seek(fp, 4, SEEK_CUR))
    return ZIP_SEEK_ERROR;

  if(flags & ZIP_F_DATA_DESCRIPTOR)
  {
    // crc32
    // compressed size
    // uncompressed size
    seek(fp, 12, SEEK_CUR);
  }

  else
  {
    // crc32
    fh->crc32 = getd(fp);

    // compressed size
    fh->compressed_size = getd(fp);
    if(fh->compressed_size == 0xFFFFFFFF)
      return ZIP_UNSUPPORTED_ZIP64;

    // uncompressed size
    fh->uncompressed_size = getd(fp);
    if(fh->uncompressed_size == 0xFFFFFFFF)
      return ZIP_UNSUPPORTED_ZIP64;
  }

  // file name length
  n = getw(fp);
  fh->file_name_length = n;

  // extra field length
  m = getw(fp);

  // Central record-only fields
  if(is_central)
  {
    // file comment length
    k = getw(fp);

    // disk number where file starts (2)
    // internal file attributes (2)
    // external file attributes (4)
    if(seek(fp, 8, SEEK_CUR))
      return ZIP_SEEK_ERROR;

    // offset to local header from start of archive
    fh->offset = getd(fp);

    // File name
    fh->file_name = cmalloc(n + 1);
    read(fh->file_name, n, 1, fp);
    fh->file_name[n] = 0;

    // Extra field (m)
    // File comment (k)
    if(seek(fp, m+k, SEEK_CUR))
      return ZIP_SEEK_ERROR;
  }

  // Local record
  else
  {
    // File name (n) (we might want this for better validation later...)
    // Extra field (m)
    if(seek(fp, n+m, SEEK_CUR))
      return ZIP_SEEK_ERROR;
  }

  // We should be at the start of the data or next record now.
  // We might need to look at the data descriptor, though.
  if(flags & ZIP_F_DATA_DESCRIPTOR)
  {
    Uint32 expected_c_size = fh->compressed_size;

    // Find the data descriptor
    if(seek(fp, expected_c_size, SEEK_CUR))
      return ZIP_MISSING_DATA_DESCRIPTOR;

    // crc32
    fh->crc32 = getd(fp);

    // compressed size
    fh->compressed_size = getd(fp);
    if(fh->compressed_size == 0xFFFFFFFF)
      return ZIP_UNSUPPORTED_ZIP64;

    // uncompressed size
    fh->uncompressed_size = getd(fp);
    if(fh->uncompressed_size == 0xFFFFFFFF)
      return ZIP_UNSUPPORTED_ZIP64;

    if(zp->error && zp->error(fp))
      return ZIP_READ_ERROR;

    // Go back to the start of the file.
    seek(fp, -expected_c_size-12, SEEK_CUR);
  }

  return ZIP_SUCCESS;
}


static enum zip_error zip_write_file_header(struct zip_archive *zp,
 struct zip_file_header *fh, int is_central)
{
  int i;

  void *fp = zp->fp;

  int (*putc)(int, void *) = zp->putc;
  void (*putw)(int, void *) = zp->putw;
  void (*putd)(int, void *) = zp->putd;
  int (*write)(void *, size_t, size_t, void *) = zp->write;

  char *magic = is_central ? file_sig_central : file_sig;

  // Memfiles - we know there's enough space, because it was already
  // checked in zip_write_file or zip_close

  // Signature
  for(i = 0; i<4; i++)
  {
    if(putc(magic[i], fp) == EOF)
      return ZIP_WRITE_ERROR;
  }

  // Version made by
  putw(ZIP_VERSION, fp);

  // Version needed to extract (central directory only)
  if(is_central)
    putw(ZIP_VERSION_MINIMUM, fp);

  // General purpose bit flag
  putw(fh->flags, fp);

  // Compression method
  putw(fh->method, fp);

  // File last modification time
  // File last modification date
  putd(zip_get_dos_date_time(), fp);

  // CRC-32
  putd(fh->crc32, fp);

  // Compressed size
  putd(fh->compressed_size, fp);

  // Uncompressed size
  putd(fh->uncompressed_size, fp);

  // File name length
  putw(fh->file_name_length, fp);

  // File comment length
  putw(0, fp);

  // (central directory only fields)
  if(is_central)
  {
    // File comment length
    putw(0, fp);

    // Disk number where file starts
    putw(0, fp);

    // Internal file attributes
    putw(0, fp);

    // External file attributes
    putd(0, fp);

    // Relative offset of local file header
    putd(fh->offset, fp);
  }

  // File name
  if(!write(fh->file_name, fh->file_name_length, 1, fp))
    return ZIP_WRITE_ERROR;

  // Extra field (is zero bytes)
  // File commend (is zero bytes)

  // Check for errors.
  if(zp->error && zp->error(fp))
    return ZIP_WRITE_ERROR;

  return ZIP_SUCCESS;
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


/* Read data from a zip archive. Only works before zip_read_directory()
 * is called or while streaming a file.
 */

static int _zget_stream(char v, struct zip_archive *zp)
{
  if(!zp->stream_left)
    return -1;
  zp->stream_crc32 = zip_crc32(zp->stream_crc32, &v, 1);
  zp->stream_left -= 1;
  return v;
}

int zgetc(struct zip_archive *zp, enum zip_error *err)
{
  char v;

  if((*err = zip_read_raw_mode_check(zp)))
    return -1;

  v = zp->getc(zp->fp);

  if(zp->streaming_file)
    return _zget_stream(v, zp);

  return v;
}

int zgetw(struct zip_archive *zp, enum zip_error *err)
{
  if((*err = zip_read_raw_mode_check(zp)))
    return -1;

  if(zp->streaming_file)
  {
    int (*getc)(void *) = zp->getc;
    void *fp = zp->fp;
    int v1;
    int v2;

    v1 = _zget_stream(getc(fp), zp);
    v2 = _zget_stream(getc(fp), zp);
    if(v1 < 0 || v2 < 0)
    {
      *err = ZIP_EOF;
      return -1;
    }

    return (v2 << 8) + v1;
  }

  return zp->getw(zp->fp);
}

int zgetd(struct zip_archive *zp, enum zip_error *err)
{
  if((*err = zip_read_raw_mode_check(zp)))
    return -1;

  if(zp->streaming_file)
  {
    int (*getc)(void *) = zp->getc;
    void *fp = zp->fp;
    int v1;
    int v2;
    int v3;
    int v4;

    v1 = _zget_stream(getc(fp), zp);
    v2 = _zget_stream(getc(fp), zp);
    v3 = _zget_stream(getc(fp), zp);
    v4 = _zget_stream(getc(fp), zp);

    if(v1 < 0 || v2 < 0 || v3 < 0 || v4 < 0)
    {
      *err = ZIP_EOF;
      return -1;
    }
    return (v4 << 24) + (v3 << 16) + (v2 << 8) + v1;
  }

  return zp->getd(zp->fp);
}

enum zip_error zread(char *destBuf, size_t readLen, struct zip_archive *zp)
{
  struct zip_file_header *fh;
  char *src;
  void *fp;

  Uint32 u_size;
  Uint32 c_size;
  Uint32 consumed;
  Uint16 method = ZIP_M_NO_COMPRESSION;

  enum zip_error result;

  result = zip_read_raw_mode_check(zp);
  if(result)
    goto err_out;

  if(!destBuf)
  {
    result = ZIP_NULL_BUF;
    goto err_out;
  }

  fp = zp->fp;
  fh = zp->streaming_file;

  if(fh)
  {
    c_size = fh->compressed_size;
    u_size = fh->uncompressed_size;
    method = fh->method;
  }

  // No compression
  if(method == ZIP_M_NO_COMPRESSION)
  {
    if(fh)
    {
      // Can't read past the length of the file
      readLen = MIN(readLen, zp->stream_left);
    }

    // Can't read past the end of the archive
    readLen = MIN(readLen, (zp->end_in_file - zp->tell(fp)));
    consumed = readLen;

    if(readLen <= 0)
    {
      result = ZIP_EOF;
      goto err_out;
    }

    if(!zp->read(destBuf, readLen, 1, fp))
    {
      result = ZIP_READ_ERROR;
      goto err_out;
    }
  }
  else

  // DEFLATE compression
  if(method == ZIP_M_DEFLATE)
  {
    if(!fh || readLen != u_size)
    {
      result = ZIP_UNSUPPORTED_DEFLATE_STREAM;
      goto err_out;
    }

    src = cmalloc(c_size);

    if(!zp->read(src, c_size, 1, fp))
    {
      result = ZIP_READ_ERROR;
      goto err_free_src;
    }

    consumed = c_size;
    result = zip_uncompress(destBuf, &u_size, src, &consumed);

    if(result != Z_STREAM_END || readLen != u_size || consumed != c_size)
    {
      warn("Uncompress result %d. Expected %u, got %u (consumed %u)\n",
       result, readLen, u_size, consumed);
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


/* Get the filename of the next file in the archive. Only works after
 * zip_read_directory() is called.
 */

enum zip_error zip_next_file_name(struct zip_archive *zp, char *name,
 int name_buffer_size)
{
  struct zip_file_header *fh;
  enum zip_error result;

  result = zip_read_file_mode_check(zp);
  if(result)
    goto err_out;

  if(zp->pos >= zp->num_files)
  {
    result = ZIP_EOF;
    goto err_out;
  }

  fh = zp->files[zp->pos];

  name_buffer_size = MIN(name_buffer_size, fh->file_name_length);
  memcpy(name, fh->file_name, name_buffer_size);
  name[name_buffer_size] = 0;

  return ZIP_SUCCESS;

err_out:
  zip_error("zip_next_file_name", result);
  return result;
}


/* Open a stream to read the next file from a zip archive. Only works after
 * zip_read_directory() is called. On return, destLen will be the uncompressed
 * size of the file.
 */

enum zip_error zip_read_open_file_stream(struct zip_archive *zp,
 size_t *destLen)
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

  result = zip_read_file_mode_check(zp);
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
  if(method == ZIP_M_NO_COMPRESSION)
  {
    debug("File: %s type: no compression\n", central_fh->file_name);
  }
  else if (method == ZIP_M_DEFLATE)
  {
    debug("File: %s type: DEFLATE\n", central_fh->file_name);
  }
  else
  {
    result = ZIP_UNSUPPORTED_COMPRESSION;
    goto err_out;
  }

  // Seek to the start of the record
  fp = zp->fp;
  read_pos = zp->tell(fp);
  if(read_pos != central_fh->offset)
  {
    if(zp->seek(fp, central_fh->offset - read_pos, SEEK_CUR))
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

  *destLen = u_size;

  return ZIP_SUCCESS;

err_out:
  zip_error("zip_read_open_file_stream", result);
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
  char v;

  int (*getc)(void *);
  void *fp;

  enum zip_error result;

  result = zip_read_stream_mode_check(zp);
  if(result)
    goto err_out;

  getc = zp->getc;
  fp = zp->fp;

  expected_crc32 = zp->streaming_file->crc32;
  stream_crc32 = zp->stream_crc32;

  // If the stream was incomplete, finish the crc32
  stream_left = zp->stream_left;
  while(stream_left)
  {
    v = getc(fp);
    stream_crc32 = zip_crc32(stream_crc32, &v, 1);
    stream_left--;
  }

  // Increment the position and clear the streaming vars
  zp->mode = ZIP_S_READ_FILES;
  zp->streaming_file = NULL;
  zp->stream_left = 0;
  zp->stream_crc32 = 0;
  zp->pos++;

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


/* Read a file from the a zip archive. Only works after zip_read_directory()
 * is called. On return, **dest will point to a newly allocated block of data
 * and *destLen will contain the length of this block.
 */

enum zip_error zip_read_file(struct zip_archive *zp, char *name,
 int name_buffer_size, char **dest, size_t *destLen)
{
  Uint32 u_size;
  enum zip_error result;

  // No need to check mode; the functions used here will

  // Optional; this may have already been checked.
  if(name)
  {
    result = zip_next_file_name(zp, name, name_buffer_size);
    if(result)
      goto err_out;
  }

  result = zip_read_open_file_stream(zp, &u_size);
  if(result)
    goto err_out;

  *dest = cmalloc(u_size);
  *destLen = u_size;

  result = zread(*dest, *destLen, zp);
  if(result && result != ZIP_EOF)
    goto err_close;

  result = zip_read_close_stream(zp);
  if(result)
    goto err_free_dest;

  return ZIP_SUCCESS;

err_close:
  zip_read_close_stream(zp);

err_free_dest:
  free(*dest);

err_out:
  if(result != ZIP_EOF)
    zip_error("zip_read_file", result);

  *dest = NULL;
  *destLen = 0;
  return result;
}


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


/* Write data to a zip archive. Only works in raw and stream modes.
 */

enum zip_error zputc(int value, struct zip_archive *zp)
{
  enum zip_error result;
  char v;

  if((result = zip_write_raw_mode_check(zp)))
    return result;

  zp->putc(value, zp->fp);

  if(zp->streaming_file)
  {
    zp->stream_crc32 = zip_crc32(zp->stream_crc32, &v, 1);
    zp->stream_left += 1;
  }

  return ZIP_SUCCESS;
}

enum zip_error zputw(int value, struct zip_archive *zp)
{
  enum zip_error result;

  if((result = zip_write_raw_mode_check(zp)))
    return result;

  zp->putw(value, zp->fp);

  if(zp->streaming_file)
  {
    char v[2];
    v[0] = value & 0xFF;
    v[1] = (value>>8) & 0xFF;
    zp->stream_crc32 = zip_crc32(zp->stream_crc32, (char *)&v, 2);
    zp->stream_left += 2;
  }

  return ZIP_SUCCESS;
}

enum zip_error zputd(int value, struct zip_archive *zp)
{
  enum zip_error result;

  if((result = zip_write_raw_mode_check(zp)))
    return result;

  zp->putd(value, zp->fp);

  if(zp->streaming_file)
  {
    char v[4];
    v[0] = value & 0xFF;
    v[1] = (value>>8) & 0xFF;
    v[2] = (value>>16) & 0xFF;
    v[3] = (value>>24) & 0xFF;
    zp->stream_crc32 = zip_crc32(zp->stream_crc32, (char *)&v, 4);
    zp->stream_left += 4;
  }

  return ZIP_SUCCESS;
}

enum zip_error zwrite(char *src, size_t srcLen, struct zip_archive *zp)
{
  struct zip_file_header *fh;
  char *buffer;
  size_t writeLen;
  Uint16 method = ZIP_M_NO_COMPRESSION;

  void *fp;

  enum zip_error result;

  result = zip_write_raw_mode_check(zp);
  if(result)
    goto err_out;

  fp = zp->fp;

  fh = zp->streaming_file;
  if(fh)
  {
    method = fh->method;
  }

  // No compression
  if(method == ZIP_M_NO_COMPRESSION)
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

    if(!zp->write(src, srcLen, 1, fp))
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
    size_t consumed = srcLen;

    result = zip_compress(&buffer, &writeLen, src, &consumed);
    if(result != Z_STREAM_END)
    {
      warn("Compress result %d. Source length %u, used %u (output %u)\n",
       result, srcLen, consumed, writeLen);
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
    if(!zp->write(buffer, writeLen, 1, fp))
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

enum zip_error zip_write_open_file_stream(struct zip_archive *zp, char *name, int method)
{
  struct zip_file_header *fh;
  char *file_name;

  void *fp;

  enum zip_error result;

  result = zip_write_file_mode_check(zp);
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
  // No compression
  if(method == ZIP_M_NO_COMPRESSION)
  {
    debug("File: %s type: no compression\n", name);
  }

  else
  // DEFLATE
  if(method == ZIP_M_DEFLATE)
  {
    debug("File: %s type: DEFLATE\n", name);
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
  fh->flags = 0;
  fh->method = method;
  fh->crc32 = 0;
  fh->compressed_size = 0;
  fh->uncompressed_size = 0;
  fh->offset = zp->tell(fp);
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
  void (*putd)(int, void *);
  int (*seek)(void *, int, int);
  int seek_value;

  enum zip_error result;

  result = zip_write_stream_mode_check(zp);
  if(result)
    goto err_out;

  // Add missing fields to the header.
  fh = zp->streaming_file;
  fh->crc32 = zp->stream_crc32;
  fh->compressed_size = zp->stream_left;

  // Go back and write sizes and CRC32
  // Could use a data descriptor instead...

  fp = zp->fp;
  putd = zp->putd;
  seek = zp->seek;

  // Compressed data + file name + extra field length + file name length
  seek_value = fh->compressed_size + fh->file_name_length + 2 + 2;

  if(seek(fp, -12-seek_value, SEEK_CUR))
  {
    result = ZIP_SEEK_ERROR;
    goto err_out;
  }

  putd(fh->crc32, fp);
  putd(fh->compressed_size, fp);
  putd(fh->uncompressed_size, fp);

  if(seek(fp, seek_value, SEEK_CUR))
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

  return ZIP_SUCCESS;

err_out:
  zip_error("zip_write_close_stream", result);
  return result;
}


/* Write a file to a zip archive. The first time this is used, the archive will
 * change to file writing mode, after which zip_write() will fail.
 * Valid methods: ZIP_M_NO_COMPRESSION, ZIP_M_DEFLATE
 */

enum zip_error zip_write_file(struct zip_archive *zp, char *name, char *src,
 size_t srcLen, int method)
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
,
};

static int _zip_header_cmp(const void *a, const void *b)
{
  return strcmp(
   (*(struct zip_file_header **)a)->file_name,
   (*(struct zip_file_header **)b)->file_name
  );
}

enum zip_error zip_read_directory(struct zip_archive *zp)
{
  int n, i, j;
  int result;

  void *fp;

  int (*seek)(void *, int, int);
  int (*getc)(void *);
  int (*getw)(void *);
  int (*getd)(void *);

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
  seek = zp->seek;
  getc = zp->getc;
  getw = zp->getw;
  getd = zp->getd;

  // Go to the end of the file.
  seek(fp, 0, SEEK_END);

  // Go back to the latest position the EOCD might be.
  if(seek(fp, -22, SEEK_CUR))
  {
    result = ZIP_NO_EOCD;
    goto err_out;
  }

  // Find the end of central directory signature.
  i = 0;
  while(1)
  {
    n = getc(fp);
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

      if(seek(fp, i, SEEK_CUR))
      {
        result = ZIP_NO_EOCD;
        goto err_out;
      }

      i = 0;
    }
  }

  // Number of this disk
  n = getw(fp);
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
  if(seek(fp, 4, SEEK_CUR))
  {
    result = ZIP_SEEK_ERROR;
    goto err_out;
  }

  // Total number of central directory records
  n = getw(fp);
  if(n < 0)
  {
    result = ZIP_READ_ERROR;
    goto err_out;
  }
  zp->num_files = n;

  // Size of central directory (bytes)
  zp->size_central_directory = getd(fp);

  // Offset of start of central directory, relative to start of file
  zp->offset_central_directory = getd(fp);

  // Load central directory records
  if(n)
  {
    struct zip_file_header **f = cmalloc(sizeof(struct zip_file_header) * n);
    zp->files = f;

    // Go to the start of the central directory.
    j = zp->tell(fp);
    if(seek(fp, zp->offset_central_directory - j, SEEK_CUR))
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

    // We want a sorted directory.
    qsort(zp->files, n, sizeof(struct zip_file_header *),
     _zip_header_cmp);
  }

  // We're in file read mode now.
  zp->mode = ZIP_S_READ_FILES;

  // At this point, we're probably at the EOCD. Reading files will seek
  // to the start of their respective entries, so just leave it alone.
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

  int (*putc)(int, void *) = zp->putc;
  void (*putw)(int, void *) = zp->putw;
  void (*putd)(int, void *) = zp->putd;

  // Memfiles: make sure there's enough space
  if(zp->hasspace && !zp->hasspace(22, fp))
    return ZIP_ALLOC_MORE_SPACE;

  // Signature
  for(i = 0; i<4; i++)
  {
    if(putc(eocd_sig[i], fp) == EOF)
      return ZIP_WRITE_ERROR;
  }

  // Number of this disk
  putw(0, fp);

  // Disk where central directory starts
  putw(0, fp);

  // Number of central directory records on this disk
  putw(zp->num_files, fp);

  // Total number of central directory records
  putw(zp->num_files, fp);

  // Size of central directory
  putd(zp->size_central_directory, fp);

  // Offset of central directory
  putd(zp->offset_central_directory, fp);

  // Comment length
  putw(0, fp);

  // Comment (length is zero)

  // Check for errors.
  if(zp->error && zp->error(fp))
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
  int (*tell)(void *);

  if(!zp)
  {
    return ZIP_NULL;
  }

  mode = zp->mode;

  fp = zp->fp;
  tell = zp->tell;

  // Before initiating the close, make sure there wasn't an open write stream!
  if(zp->streaming_file && mode == ZIP_S_WRITE_STREAM)
  {
    warn("zip_close called while writing file stream!\n");
    zip_write_close_stream(zp);
  }

  if(!zp->closing)
  {
    debug("Finalizing and closing zip file.\n");
    zp->closing = 1;
    zp->pos = 0;
  
    if(mode == ZIP_S_WRITE_FILES)
      zp->offset_central_directory = tell(fp);
  }

  if(mode == ZIP_S_WRITE_FILES)
  {
    int expected_size = 22 + 46*zp->num_files + zp->running_file_name_length;

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

  // Write the end of central directory record
  if(mode == ZIP_S_WRITE_FILES || mode == ZIP_S_WRITE_RAW)
  {
    zp->size_central_directory = tell(fp) - zp->offset_central_directory;

    result = zip_write_eocd_record(zp);

    if(final_length)
      *final_length = tell(fp);
  }

  else
  {
    if(final_length)
      *final_length = zp->end_in_file;
  }

  zp->close(zp->fp);

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
  zp->getc = (int(*)(void *)) fgetc;
  zp->getw = (int(*)(void *)) fgetw;
  zp->getd = (int(*)(void *)) fgetd;
  zp->putc = (int(*)(int, void *)) fputc;
  zp->putw = (void(*)(int, void *)) fputw;
  zp->putd = (void(*)(int, void *)) fputd;
  zp->read = (int(*)(void *, size_t, size_t, void *)) fread;
  zp->write = (int(*)(void *, size_t, size_t, void *)) fwrite;
  zp->seek = (int(*)(void *, int, int)) fseek;
  zp->tell = (int(*)(void *)) ftell;
  zp->error = (int(*)(void *)) ferror;
  zp->close = (int(*)(void *)) fclose;
  return zp;
}


/* Open a zip archive located in a file for reading. The archive will be in
 * raw read mode, for use with zip_read(), until zip_read_directory()
 * is called. Afterward, the archive will be in file read mode.
 */

struct zip_archive *zip_open_file_read(char *file_name)
{
  FILE *fp = fopen_unsafe(file_name, "rb");

  if(fp)
  {
    struct zip_archive *zp = zip_get_archive_file(fp);
    zp->end_in_file = ftell_and_rewind(fp);

    debug("Opened file '%s' for reading.\n", file_name);
    return zp;
  }

  warn("Failed to open file '%s'.\n", file_name);
  return NULL;
}


/* Open a zip archive located in a file for writing. The archive will be in
 * raw write mode, for use with zip_write(), until zip_write_file() is called.
 * Afterward, the archive will be in file write mode.
 */

struct zip_archive *zip_open_file_write(char *file_name)
{
  FILE *fp = fopen_unsafe(file_name, "wb");

  if(fp)
  {
    struct zip_archive *zp = zip_get_archive_file(fp);

    zip_init_for_write(zp, ZIP_DEFAULT_NUM_FILES);

    debug("Opened file '%s' for writing.\n", file_name);
    return zp;
  }

  warn("Failed to open file '%s'.\n", file_name);
  return NULL;
}



/* Configure the zip archive for memory reading. TODO: abstract the file
 * functions further so the struct isn't as large?
 */

static struct zip_archive *zip_get_archive_mem(struct memfile *mf)
{
  struct zip_archive *zp = zip_new_archive();
  zp->fp = mf;
  zp->hasspace = (int(*)(size_t, void *)) mfhasspace;
  zp->getc = (int(*)(void *)) mfgetc;
  zp->getw = (int(*)(void *)) mfgetw;
  zp->getd = (int(*)(void *)) mfgetd;
  zp->putc = (int(*)(int, void *)) mfputc;
  zp->putw = (void(*)(int, void *)) mfputw;
  zp->putd = (void(*)(int, void *)) mfputd;
  zp->read = (int(*)(void *, size_t, size_t, void *)) mfread;
  zp->write = (int(*)(void *, size_t, size_t, void *)) mfwrite;
  zp->seek = (int(*)(void *, int, int)) mfseek;
  zp->tell = (int(*)(void *)) mftell;
  zp->error = NULL;
  zp->close = (int(*)(void *)) mfclose;
  return zp;
}


/* Open a zip archive located in a block of memory for reading. The archive
 * will be in raw read mode, for use with zip_read(), until zip_read_directory()
 * is called. Afterward, the archive will be in file read mode.
 */

struct zip_archive *zip_open_mem_read(char *src, size_t len)
{
  struct zip_archive *zp;
  struct memfile *mf;

  if(src && len > 0)
  {
    mf = mfopen(src, len);
    zp = zip_get_archive_mem(mf);

    zp->end_in_file = len;
    
    debug("Opened memfile '%p', length %d for reading.\n", src, len);
    return zp;
  }

  warn("Failed to open memfile '%p'.\n", src);
  return NULL;
}


/* Open a zip archive located in a block of memory for writing. The archive
 * will be in raw write mode, for use with zip_write(), until zip_write_file()
 * is called. Afterward, the archive will be in file write mode.
 */

struct zip_archive *zip_open_mem_write(char *src, size_t len)
{
  struct zip_archive *zp;
  struct memfile *mf;

  if(src && len > 0)
  {
    mf = mfopen(src, len);
    zp = zip_get_archive_mem(mf);

    zip_init_for_write(zp, ZIP_DEFAULT_NUM_FILES);

    debug("Opened memfile '%p', length %d for writing.\n", src, len);
    return zp;
  }

  warn("Failed to open memfile '%p'.\n", src);
  return NULL;
}


/* Expand the allocated size of a zip archive's memfile. Use when a zip
 * write function fails with ZIP_ALLOC_MORE_SPACE.
 */

enum zip_error zip_expand(struct zip_archive *zp, char **src, size_t new_size)
{
  struct memfile *mf;
  size_t current_offset;
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

  if((size_t)(mf->end - start) < new_size)
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
  size_t zip_len = 0;
  size_t len = 32;
  
  enum zip_error result = 0;

  // Read an arbitrary zip from file
  zp = zip_open_file_read((char *)"_mzmstrings.zip");

  if(zread(name, len, zp) == ZIP_SUCCESS)
  {
    name[31] = 0;
    debug("Some raw data: %s\n", name);
  }
  if(!zip_read_directory(zp))
  {
    while(1)
    {
      result = zip_read_file(zp, name, 31, &data, &len);
      if(result)
      {
        if(result == ZIP_EOF)
          debug("No more files.\n");
        break;
      }
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

  zip_write_file(zp, name, name, len, ZIP_M_NO_COMPRESSION);

  result = zip_close(zp, &zip_len);
  if(result == ZIP_ALLOC_MORE_SPACE)
  {
    debug("Attempting to expand to %d bytes\n", zip_len);
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
    zip_next_file_name(zp, name, 31);

    result = zip_read_open_file_stream(zp, &len);
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
  zip_write_file(zp, name, data, len, ZIP_M_DEFLATE);
  zip_close(zp, NULL);
  printf("\n");

  free(data);
}
#endif //TEST
