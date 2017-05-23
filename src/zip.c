/* MegaZeux0
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
#define ZIP_S_WRITE_RAW 2
#define ZIP_S_WRITE_FILES 3


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

struct memfile
{
  char *start;
  char *current;
  char *end;
};

static struct memfile *mfopen(char *src, int len)
{
  struct memfile *mf = cmalloc(sizeof(struct memfile));

  mf->start = src;
  mf->current = src;
  mf->end = src + len;

  return mf;
}

static int mfclose(struct memfile *mf)
{
  if(mf)
  {
    free(mf);
    return 0;
  }
  return -1;
}

static int mfhasspace(size_t len, struct memfile *mf)
{
  return (len + mf->current) <= mf->end;
}

static int mfgetc(struct memfile *mf)
{
  int v;
  v =  mf->current[0];
  mf->current += 1;
  return v;
}

static int mfgetw(struct memfile *mf)
{
  int v;
  v =  mf->current[0];
  v |= mf->current[1] << 8;
  mf->current += 2;
  return v;
}

static int mfgetd(struct memfile *mf)
{
  int v;
  v =  mf->current[0];
  v |= mf->current[1] << 8;
  v |= mf->current[2] << 16;
  v |= mf->current[3] << 24;
  mf->current += 4;
  return v;
}

static int mfputc(int ch, struct memfile *mf)
{
  mf->current[0] = ch & 0xFF;
  mf->current += 1;
  return ch & 0xFF;
}

static void mfputw(int ch, struct memfile *mf)
{
  mf->current[0] = ch & 0xFF;
  mf->current[1] = (ch >> 8) & 0xFF;
  mf->current += 2;
}

static void mfputd(int ch, struct memfile *mf)
{
  mf->current[0] = ch & 0xFF;
  mf->current[1] = (ch >> 8) & 0xFF;
  mf->current[2] = (ch >> 16) & 0xFF;
  mf->current[3] = (ch >> 24) & 0xFF;
  mf->current += 4;
}

static int mfread(char *dest, size_t len, size_t count, struct memfile *mf)
{
  unsigned int i;
  for(i = 0; i < count; i++)
  {
    if(mf->current + len >= mf->end)
      break;

    memcpy(dest, mf->current, len);
    mf->current += len;
    dest += len;
  }

  return i;
}

static int mfwrite(char *src, size_t len, size_t count, struct memfile *mf)
{
  unsigned int i;
  for(i = 0; i < count; i++)
  {
    if(mf->current + len >= mf->end)
      break;

    memcpy(mf->current, src, len);
    mf->current += len;
    src += len;
  }

  return i;
}

static int mfseek(struct memfile *mf, int offs, int code)
{
  char *ptr;
  switch(code)
  {
    case SEEK_SET:
      ptr = mf->start + offs;
      break;

    case SEEK_CUR:
      ptr = mf->current + offs;
      break;

    case SEEK_END:
      ptr = mf->end + offs;
      break;

    default:
      ptr = NULL;
      break;
  }

  if(ptr && ptr >= mf->start && ptr <= mf->end)
  {
    mf->current = ptr;
    return 0;
  }

  return -1;
}

static int mftell(struct memfile *mf)
{
  return mf->current - mf->start;
}


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
    case ZIP_NULL:
      return "Function received null archive pointer";
    case ZIP_EOF:
      return "Reached end of file";
    case ZIP_SEEK_ERROR:
      return "Could not seek to requested position";
    case ZIP_READ_ERROR:
      return "Could not read from requested position";
    case ZIP_WRITE_ERROR:
      return "Could not write to requested position";
    case ZIP_OPEN_ERROR:
      return "Could not open archive";
    case ZIP_ALLOC_MORE_SPACE:
      return "Unexpectedly reached the end of the input memory block";
    case ZIP_INVALID_READ_IN_WRITE_MODE:
      return "Archive is in write mode and can not be read from";
    case ZIP_INVALID_WRITE_IN_READ_MODE:
      return "Archive is in read mode and can not be written to";
    case ZIP_INVALID_RAW_READ_IN_FILE_MODE:
      return "Archive is in file mode and can not be directly read from";
    case ZIP_INVALID_RAW_WRITE_IN_FILE_MODE:
      return "Archive is in file mode and can not be directly written to";
    case ZIP_INVALID_FILE_READ_IN_RAW_MODE:
      return "Archive is in raw mode; use zip_read_directory before writing files";
    case ZIP_INVALID_DIRECTORY_READ_IN_FILE_MODE:
      return "Directory has already been read";
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
    case ZIP_UNSUPPORTED_DATA_DESCRIPTOR:
      return "Data descriptors are unsupported";
    case ZIP_UNSUPPORTED_FLAGS:
      return "File uses unsupported flags; use 0";
    case ZIP_UNSUPPORTED_COMPRESSION:
      return "File uses unsupported compression method; use DEFLATE or no compression";
    case ZIP_UNSUPPORTED_ZIP64:
      return "File contains unsupported ZIP64 data";
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

static int zip_read_file_header(struct zip_archive *zp,
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
      return ZIP_ALLOC_MORE_SPACE;

    n = getc(fp);
    if(n < 0)
      return ZIP_READ_ERROR;

    //debug("Current position: %d, n=%x, i=%d\n", zp->tell(fp), n, i);

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
    warn("Zips using data descriptors currently unsupported.\n");
    return ZIP_UNSUPPORTED_DATA_DESCRIPTOR;
  }
  else
  if(flags != 0)
  {
    warn("Zip using unsupported options (%d) -- use 0.\n", flags);
    return ZIP_UNSUPPORTED_FLAGS;
  }

  // compression method
  method = getw(fp);
  if(method < 0)
  {
    return ZIP_READ_ERROR;
  }
  else
  if(method != ZIP_M_NO_COMPRESSION && method != ZIP_M_DEFLATE)
  {
    warn("Unsupported compression method %d.\n", method);
    return ZIP_UNSUPPORTED_COMPRESSION;
  }
  fh->method = method;
  
  // file last modification time (2)
  // file last modification date (2)
  if(seek(fp, 4, SEEK_CUR))
    return ZIP_SEEK_ERROR;

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
  return ZIP_SUCCESS;
}

static int zip_write_file_header(struct zip_archive *zp,
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
  putw(0, fp);

  // Compression method
  putw(fh->method, fp);

  // File last modification time
  // File last modification date
  putd(zp->dos_date_time, fp);

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


/* Read a block of data from a zip archive. Only works before
 * zip_read_directory() is called.
 */

int zip_read(struct zip_archive *zp, char *dest, int *destLen)
{
  int bufferLen = *destLen;
  int result;

  if(!zp)
  {
    result = ZIP_NULL;
    goto err_out;
  }

  if(zp->mode != ZIP_S_READ_RAW)
  {
    if(zp->mode == ZIP_S_READ_FILES)
    {
      result = ZIP_INVALID_RAW_READ_IN_FILE_MODE;
      goto err_out;
    }
    result = ZIP_INVALID_READ_IN_WRITE_MODE;
  }

  *destLen *= zp->read(dest, bufferLen, 1, zp->fp);

  if(!*destLen)
  {
    result = ZIP_READ_ERROR;
    goto err_out;
  }

  return ZIP_SUCCESS;

err_out:
  zip_error("zip_read", result);

  dest[0] = 0;
  *destLen = 0;
  return result;
}


/* Read a file from the a zip archive. Only works after zip_read_directory()
 * is called. On return, **dest will point to a newly allocated block of data
 * and *destLen will contain the length of this block.
 */

int zip_read_file(struct zip_archive *zp, char *name, int name_buffer_size,
 char **dest, int *destLen)
{
  struct zip_file_header *central_fh;
  struct zip_file_header local_fh;
  void *fp;

  Uint32 f_crc32;
  Uint32 u_crc32;
  Uint32 c_size;
  Uint32 u_size;
  char *src;

  Uint16 method;
  Uint32 read_pos;

  int result;

  if(!zp)
  {
    result = ZIP_NULL;
    goto err_out;
  }

  // This only works in file reading mode
  if(zp->mode != ZIP_S_READ_FILES)
  {
    if(zp->mode == ZIP_S_READ_RAW)
    {
      result = ZIP_INVALID_FILE_READ_IN_RAW_MODE;
      goto err_out;
    }
    result = ZIP_INVALID_READ_IN_WRITE_MODE;
    goto err_out;
  }

  if(zp->pos >= zp->num_files)
  {
    result = ZIP_EOF;
    goto err_out;
  }

  central_fh = zp->files[zp->pos];
  zp->pos++;

  f_crc32 = central_fh->crc32;
  c_size = central_fh->compressed_size;
  u_size = central_fh->uncompressed_size;
  method = central_fh->method;

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

  // Read the local header
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

  // No compression
  if(method == ZIP_M_NO_COMPRESSION)
  {
    debug("File: %s type: no compression\n", central_fh->file_name);
    *dest = cmalloc(u_size);
    if(zp->read(*dest, u_size, 1, fp))
    {
      result = ZIP_READ_ERROR;
      goto err_free_dest;
    }
  }
  else

  // DEFLATE compression
  if(method == ZIP_M_DEFLATE)
  {
    Uint32 old_size = u_size;

    debug("File: %s type: DEFLATE\n", central_fh->file_name);

    *dest = cmalloc(u_size);
    src = cmalloc(c_size);

    if(!zp->read(src, c_size, 1, fp))
    {
      result = ZIP_READ_ERROR;
      goto err_free_src;
    }

    result = zip_uncompress(*dest, &u_size, src, &c_size);

    if(result != Z_STREAM_END || old_size != u_size)
    {
      warn("Uncompress result %d. Expected %u, got %u (consumed %u)\n",
       result, old_size, u_size, c_size);
      result = ZIP_INFLATE_FAILED;
      goto err_free_src;
    }
    free(src);
  }

  else
  {
    warn("Unsupported compression method.\n");
    result = ZIP_UNSUPPORTED_COMPRESSION;
    goto err_out;
  }

  *destLen = u_size;

  u_crc32 = crc32(0, (Bytef *) *dest, u_size);
  if(f_crc32 != u_crc32)
  {
    warn("crc check failed: expected %x, got %x\n", f_crc32, u_crc32);
    result = ZIP_CRC32_MISMATCH;
    goto err_free_dest;
  }

  name_buffer_size = MIN(name_buffer_size, central_fh->file_name_length);
  memcpy(name, central_fh->file_name, name_buffer_size);
  name[name_buffer_size] = 0;

  return ZIP_SUCCESS;

err_free_src:
  free(src);

err_free_dest:
  free(*dest);

err_out:
  if(result != ZIP_EOF)
    zip_error("zip_read_file", result);

  *dest = NULL;
  *destLen = 0;
  return result;
}


/* Write a block of data to a zip archive. Only works before zip_write_file()
 * is called.
 */

int zip_write(struct zip_archive *zp, char *src, int srcLen)
{
  int bufferLen = srcLen;
  int result;

  if(!zp)
  {
    result = ZIP_NULL;
    goto err_out;
  }

  if(zp->mode != ZIP_S_WRITE_RAW)
  {
    if(zp->mode == ZIP_S_WRITE_FILES)
    {
      result = ZIP_INVALID_RAW_WRITE_IN_FILE_MODE;
      goto err_out;
    }
    result = ZIP_INVALID_WRITE_IN_READ_MODE;
    goto err_out;
  }

  result = zp->write(src, bufferLen, 1, zp->fp);

  if(!result)
  {
    result = ZIP_WRITE_ERROR;
    goto err_out;
  }

  return ZIP_SUCCESS;

err_out:
  zip_error("zip_write", result);
  return result;
}


/* Write a file to a zip archive. The first time this is used, the archive will
 * change to file writing mode, after which zip_write() will fail.
 * Valid methods: ZIP_M_NO_COMPRESSION, ZIP_M_DEFLATE
 */

int zip_write_file(struct zip_archive *zp, char *name, char *src, int srcLen,
 int method)
{
  struct zip_file_header *fh;
  char *file_name;
  int expected_size;
  char *dest;

  void *fp;

  Uint32 u_size = srcLen;
  Uint32 c_size;

  int result;

  if(!zp)
  {
    result = ZIP_NULL;
    goto err_out;
  }

  // This only works in write mode
  if(zp->mode != ZIP_S_WRITE_FILES)
  {
    // If we're in raw mode, switch to file mode.
    if(zp->mode == ZIP_S_WRITE_RAW)
    {
      zp->mode = ZIP_S_WRITE_FILES;
    }
    else
    {
      result = ZIP_INVALID_READ_IN_WRITE_MODE;
      goto err_out;
    }
  }

  fh = cmalloc(sizeof(struct zip_file_header));
  file_name = cmalloc(strlen(name) + 1);
  fp = zp->fp;

  // Set up the header
  fh->method = method;
  fh->crc32 = zip_crc32(0, src, u_size);
  fh->offset = zp->tell(fp);
  fh->file_name_length = strlen(name);
  fh->file_name = file_name;
  strcpy(file_name, name);

  expected_size = 30 + fh->file_name_length;

  // No compression
  if(method == ZIP_M_NO_COMPRESSION)
  {
    debug("File: %s type: no compression\n", name);

    fh->compressed_size = u_size;
    fh->uncompressed_size = u_size;

    // Test if we have the space to write
    if(zp->hasspace)
    {
      expected_size += u_size;

      if(!zp->hasspace(expected_size, fp))
      {
        result = ZIP_ALLOC_MORE_SPACE;
        goto err_free;
      }
    }

    // Write
    result = zip_write_file_header(zp, fh, 0);
    if(result)
      goto err_free;

    if(!zp->write(src, u_size, 1, fp))
    {
      result = ZIP_WRITE_ERROR;
      goto err_free;
    }
  }
  else

  // DEFLATE
  if(method == ZIP_M_DEFLATE)
  {
    debug("File: %s type: DEFLATE\n", name);

    result = zip_compress(&dest, &c_size, src, (Uint32 *)&srcLen);
    if(result != Z_STREAM_END)
    {
      warn("Uncompress result %d. Expected %u, used %u (wrote %u)\n",
       result, u_size, srcLen, c_size);
      result = ZIP_DEFLATE_FAILED;
      goto err_free_dest;
    }

    fh->compressed_size = c_size;
    fh->uncompressed_size = u_size;

    // Test if we have the space to write
    if(zp->hasspace)
    {
      expected_size += c_size;

      if(!zp->hasspace(expected_size, fp))
      {
        result = ZIP_ALLOC_MORE_SPACE;
        goto err_free_dest;
      }
    }

    // Write
    result = zip_write_file_header(zp, fh, 0);
    if(result)
      goto err_free_dest;

    if(!zp->write(dest, c_size, 1, fp))
    {
      result = ZIP_WRITE_ERROR;
      goto err_free_dest;
    }

    free(dest);
  }

  else
  {
    result = ZIP_UNSUPPORTED_COMPRESSION;
    goto err_free;
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

  return ZIP_SUCCESS;

err_free_dest:
  free(dest);

err_free:
  free(file_name);
  free(fh);

err_out:
  zip_error("zip_write_file", result);
  return result;
}


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

/* Reads the central directory of a zip archive. This places the archive into
 * file read mode; read files using zip_read_file(). If this fails, the input
 * is probably not actually a zip archive, or uses features we don't support.
 */

int zip_read_directory(struct zip_archive *zp)
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

    //debug("Current position: %d, n=%x, i=%d\n", zp->tell(fp), n, i);
  
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


static int zip_write_eocd_record(struct zip_archive *zp)
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

int zip_close(struct zip_archive *zp, int *final_length)
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

    // Calcualte projected file size in case more space is needed
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
      *final_length = zp->end;
  }

  zp->close(zp->fp);

  free(zp->files);
  free(zp);

  if(result != ZIP_SUCCESS)
    zip_error("zip_close", result);

  return result;
}


static void zip_init_for_write(struct zip_archive *zp, int num_files)
{
  struct zip_file_header **f;

  f = cmalloc(sizeof(struct zip_file_header) * num_files);

  zp->files_alloc = num_files;
  zp->files = f;

  zp->dos_date_time = zip_get_dos_date_time();
  zp->running_file_name_length = 0;

  zp->mode = ZIP_S_WRITE_RAW;
}


static struct zip_archive *zip_new_archive(void)
{
  struct zip_archive *zp = cmalloc(sizeof(struct zip_archive));

  zp->files_alloc = 0;
  zp->closing = 0;
  zp->start = 0;
  zp->pos = 0;

  zp->num_files = 0;
  zp->size_central_directory = 0;
  zp->offset_central_directory = 0;

  zp->mode = ZIP_S_READ_RAW;

  return zp;
}


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
    zp->end = ftell_and_rewind(fp);

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

struct zip_archive *zip_open_mem_read(char *src, int len)
{
  struct zip_archive *zp;
  struct memfile *mf;

  if(src && len > 0)
  {
    mf = mfopen(src, len);
    zp = zip_get_archive_mem(mf);

    zp->end = len;
    
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

struct zip_archive *zip_open_mem_write(char *src, int len)
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

int zip_expand(struct zip_archive *zp, char **src, size_t new_size)
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
  int zip_len = 0;
  int len = 32;
  int result = 0;

  // Read an arbitrary zip from file
  zp = zip_open_file_read((char *)"_mzmstrings.zip");
  if(!zip_read(zp, name, &len) && len)
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
  zip_len = zip_bound_total_header_usage(1, 14);
  zip_len += zip_bound_data_usage(name, strlen(name) + 1);
  zip_file = cmalloc(zip_len);

  zp = zip_open_mem_write(zip_file, zip_len);
  
  debug("(note: writing raw data to throw off the calculated bound)\n");
  sprintf(name, "Some data.txt");
  len = strlen(name) + 1;
  zip_write(zp, name, len);

  len = strlen(name) + 1;
  result = zip_write_file(zp, name, name, len, ZIP_M_DEFLATE);
  if(result)
  {
    debug("Error code: %d\n", result);
  }
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
  zp = zip_open_mem_read(zip_file, zip_len);
  if(!zip_read_directory(zp))
  {
    result = zip_read_file(zp, name, 31, &data, &len);
    if(result)
      debug("Error code: %d\n", result);
  }
  zip_close(zp, NULL);
  printf("\n");

  free(zip_file);

  // Write a zip to file
  zp = zip_open_file_write((char *)"output.zip");
  result = zip_write_file(zp, name, data, len, ZIP_M_DEFLATE);
  if(result)
  {
    debug("Error code: %d\n", result);
  }
  zip_close(zp, NULL);
  printf("\n");

  free(data);
}
#endif //TEST
