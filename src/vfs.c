/* MegaZeux
 *
 * Copyright (C) 2019 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>

#include "memfile.h"
#include "vfs.h"
//#include "zip.h"

enum vfileflags
{
  VF_FILE               = (1<<0),
  VF_MEMORY             = (1<<1),
  VF_MEMORY_EXPANDABLE  = (1<<2) | VF_MEMORY,
//VF_IN_ARCHIVE         = (1<<3),
  VF_READ               = (1<<4),
  VF_WRITE              = (1<<5),
  VF_APPEND             = (1<<6),
  VF_BINARY             = (1<<7),
};

struct vfile
{
  FILE *fp;

  struct memfile mf;
  void **external_buffer;
  size_t *external_buffer_size;

  enum vfileflags flags;
};

/**
 * Parse vfile mode flags from a standard fopen mode string.
 */
static enum vfileflags get_vfile_mode_flags(const char *mode)
{
  enum vfileflags flags = 0;

  assert(mode);

  switch(*(mode++))
  {
    case 'r':
      flags |= VF_READ;
      break;

    case 'w':
      flags |= VF_WRITE;
      break;

    case 'a':
      flags |= VF_WRITE | VF_APPEND;
      break;

    default:
      return 0;
  }

  while(*mode)
  {
    switch(*(mode++))
    {
      case 'b':
        flags |= VF_BINARY;
        break;

      case '+':
        flags |= (VF_READ | VF_WRITE);
        break;

      default:
        return 0;
    }
  }
  return flags;
}

/**
 * Open a file for input or output.
 */
vfile *vfopen_unsafe(const char *filename, const char *mode)
{
  enum vfileflags flags = get_vfile_mode_flags(mode);
  vfile *vf = NULL;

  assert(filename && flags);

  // TODO archive detection/memory file caching here
  // TODO vf = vfscache_vfopen(filename, flags);

  // Open the vfile as a normal file.
  if(!vf)
  {
    // TODO this is where Win32 UTF-8 support goes:
    // TODO fp = platform_fopen(filename, mode);
    FILE *fp = fopen_unsafe(filename, mode);

    if(fp)
    {
      // TODO buffering for real files
      //if((flags & VF_BINARY) && (!(flags & VF_READ) || !(flags & VF_WRITE)))
      //  setvbuf lol

      vf = ccalloc(1, sizeof(vfile));
      vf->fp = fp;
      vf->flags = flags | VF_FILE;
      return vf;
    }
  }
  return NULL;
}

/**
 * Create a vfile from an existing fp.
 */
vfile *vfile_init_fp(FILE *fp, const char *mode)
{
  enum vfileflags flags = get_vfile_mode_flags(mode);
  vfile *vf = NULL;

  assert(fp && flags);

  vf = ccalloc(1, sizeof(vfile));
  vf->fp = fp;
  vf->flags = flags | VF_FILE;
  return vf;
}

/**
 * Create a vfile from an existing memory buffer.
 */
vfile *vfile_init_mem(void *buffer, size_t size, const char *mode)
{
  enum vfileflags flags = get_vfile_mode_flags(mode);
  vfile *vf = NULL;

  // TODO append not really supported by memfiles, so that needs manual
  // handling at some point..
  assert(buffer && size && flags && !(flags & VF_APPEND));

  vf = ccalloc(1, sizeof(vfile));
  mfopen(buffer, size, &(vf->mf));
  vf->flags = flags | VF_MEMORY;
  return vf;
}

/**
 * Create a vfile from an existing memory buffer.
 * This vfile will be resizable and, when resized, the source pointer and size
 * will be updated to match.
 */
vfile *vfile_init_mem_ext(void **external_buffer, size_t *external_buffer_size,
 const char *mode)
{
  vfile *vf = vfile_init_mem(*external_buffer, *external_buffer_size, mode);

  assert(vf->flags & VF_WRITE);

  vf->flags |= VF_MEMORY_EXPANDABLE;
  vf->external_buffer = external_buffer;
  vf->external_buffer_size = external_buffer_size;
  return vf;
}

/**
 * Close a file. The file pointer should not be used after using this function.
 */
int vfclose(vfile *vf)
{
  int retval = 0;

  assert(vf);

  if(vf->flags & VF_FILE)
    retval = fclose(vf->fp);

  free(vf);
  return retval;
}

/**
 * Get the underlying memfile of a memory vfile. This should pretty much
 * not be used, but some things in the zip code already relied on it.
 */
struct memfile *vfile_get_memfile(vfile *vf)
{
  assert(vf);
  assert(vf->flags & VF_MEMORY);
  return &(vf->mf);
}

/**
 * Change the current working directory to path.
 */
int vchdir(const char *path)
{
  // TODO archive detection, platform overrides, etc
  // TODO don't use real chdir
  return chdir(path);
}

/**
 * Print the name of the current working directory to buffer 'buf' of
 * length 'size'.
 */
char *vgetcwd(char *buf, size_t size)
{
  // TODO archive detection, platform overrides, etc
  // TODO don't use real getcwd
  return getcwd(buf, size);
}

/**
 * Unlink a name (and delete a file if it was the last link to the file) if
 * the file is a real file.
 */
int vunlink(const char *path)
{
  // TODO archive detection, platform overrides, etc
  return unlink(path);
}

/**
 * Get file information for a file at a given path.
 */
int vstat(const char *path, struct stat *buf)
{
  // TODO archive detection, platform overrides, etc
  return stat(path, buf);
}

/**
 * Ensure an amount of space exists in a memory vfile or expand the vfile
 * (if possible). This should be used for writing only.
 */
static inline boolean vfile_ensure_space(int amount_to_write, vfile *vf)
{
  struct memfile *mf = &(vf->mf);
  ssize_t new_size;

  assert(vf->flags & VF_MEMORY);

  if(!mfhasspace(amount_to_write, mf))
  {
    if(!(vf->flags & VF_MEMORY_EXPANDABLE))
      return false;

    // TODO it would be cool if this could have a separate alloc size.
    new_size = (mf->current - mf->start) + amount_to_write;
    mfresize(new_size, mf);

    *(vf->external_buffer) = mf->start;
    *(vf->external_buffer_size) = new_size;

    return (mf->end - mf->start) >= new_size;
  }
  return true;
}

/**
 * Read a single byte from a file.
 */
int vfgetc(vfile *vf)
{
  assert(vf && (vf->flags & VF_READ));

  if(vf->flags & VF_MEMORY)
    return mfhasspace(1, &(vf->mf)) ? mfgetc(&(vf->mf)) : EOF;

  if(vf->flags & VF_FILE)
    return fgetc(vf->fp);

  assert(0);
  return EOF;
}

/**
 * Read two bytes from a file as an unsigned integer (little endian).
 */
int vfgetw(vfile *vf)
{
  assert(vf && (vf->flags & VF_READ));

  if(vf->flags & VF_MEMORY)
    return mfhasspace(2, &(vf->mf)) ? mfgetw(&(vf->mf)) : EOF;

  if(vf->flags & VF_FILE)
  {
    FILE *fp = vf->fp;
    int a = fgetc(fp);
    int b = fgetc(fp);

    return (a != EOF) && (b != EOF) ? ((b << 8) | a) : EOF;
  }

  assert(0);
  return EOF;
}

/**
 * Read four bytes from a file as a signed integer (little endian).
 */
int vfgetd(vfile *vf)
{
  assert(vf && (vf->flags & VF_READ));

  if(vf->flags & VF_MEMORY)
    return mfhasspace(4, &(vf->mf)) ? mfgetd(&(vf->mf)) : EOF;

  if(vf->flags & VF_FILE)
  {
    FILE *fp = vf->fp;
    int a = fgetc(fp);
    int b = fgetc(fp);
    int c = fgetc(fp);
    int d = fgetc(fp);

    if((a == EOF) || (b == EOF) || (c == EOF) || (d == EOF))
      return EOF;

    return (d << 24) | (c << 16) | (b << 8) | a;
  }

  assert(0);
  return EOF;
}

/**
 * Write a single byte to a file.
 */
int vfputc(int character, vfile *vf)
{
  assert(vf && (vf->flags & VF_WRITE));

  if(vf->flags & VF_MEMORY)
  {
    if(!vfile_ensure_space(1, vf))
      return EOF;

    return mfputc(character, &(vf->mf));
  }

  if(vf->flags & VF_FILE)
    return fputc(character, vf->fp);

  assert(0);
  return EOF;
}

/**
 * Write an unsigned short integer to a file (little endian).
 */
int vfputw(int character, vfile *vf)
{
  assert(vf && (vf->flags & VF_WRITE));

  if(vf->flags & VF_MEMORY)
  {
    if(!vfile_ensure_space(2, vf))
      return EOF;

    mfputw(character, &(vf->mf));
    return character;
  }

  if(vf->flags & VF_FILE)
  {
    FILE *fp = vf->fp;
    int a = fputc(character & 0xFF, fp);
    int b = fputc((character >> 8) & 0xFF, fp);
    return (a != EOF) && (b != EOF) ? character : EOF;
  }

  assert(0);
  return EOF;
}

/**
 * Write a signed long integer to a file (little endian).
 */
int vfputd(int character, vfile *vf)
{
  assert(vf && (vf->flags & VF_WRITE));

  if(vf->flags & VF_MEMORY)
  {
    if(!vfile_ensure_space(4, vf))
      return EOF;

    mfputd(character, &(vf->mf));
    return character;
  }

  if(vf->flags & VF_FILE)
  {
    FILE *fp = vf->fp;
    int a = fputc(character & 0xFF, fp);
    int b = fputc((character >> 8) & 0xFF, fp);
    int c = fputc((character >> 16) & 0xFF, fp);
    int d = fputc((character >> 24) & 0xFF, fp);

    if((a == EOF) || (b == EOF) || (c == EOF) || (d == EOF))
      return EOF;

    return character;
  }

  assert(0);
  return EOF;
}

/**
 * Read an array from a file.
 */
int vfread(void *dest, size_t size, size_t count, vfile *vf)
{
  assert(vf && dest && (vf->flags & VF_READ));

  if(vf->flags & VF_MEMORY)
    return mfread(dest, size, count, &(vf->mf));

  if(vf->flags & VF_FILE)
    return fread(dest, size, count, vf->fp);

  assert(0);
  return 0;
}

/**
 * Write an array to a file.
 */
int vfwrite(const void *src, size_t size, size_t count, vfile *vf)
{
  assert(vf && src && (vf->flags & VF_WRITE));

  if(vf->flags & VF_MEMORY)
  {
    if(!vfile_ensure_space(size * count, vf))
      return 0;

    return mfwrite(src, size, count, &(vf->mf));
  }

  if(vf->flags & VF_FILE)
    return fwrite(src, size, count, vf->fp);

  assert(0);
  return 0;
}

/**
 * Seek to a position in a file.
 */
int vfseek(vfile *vf, long int offset, int whence)
{
  assert(vf);

  if(vf->flags & VF_MEMORY)
    return mfseek(&(vf->mf), offset, whence);

  if(vf->flags & VF_FILE)
    return fseek(vf->fp, offset, whence);

  assert(0);
  return -1;
}

/**
 * Return the current position in a file.
 */
long int vftell(vfile *vf)
{
  assert(vf);

  if(vf->flags & VF_MEMORY)
    return mftell(&(vf->mf));

  if(vf->flags & VF_FILE)
    return ftell(vf->fp);

  assert(0);
  return -1;
}

/**
 * Rewind the file to the start (and clear EOF/errors if applicable).
 */
void vrewind(vfile *vf)
{
  assert(vf);

  if(vf->flags & VF_MEMORY)
  {
    mfseek(&(vf->mf), 0, SEEK_SET);
    return;
  }

  if(vf->flags & VF_FILE)
  {
    rewind(vf->fp);
    return;
  }

  assert(0);
}

/**
 * Return the length of the file and rewind to the start of it.
 * TODO do something else instead of this.
 */
long vftell_and_rewind(vfile *vf)
{
  long size;

  vfseek(vf, 0, SEEK_END);
  size = vftell(vf);
  vrewind(vf);

  return size;
}
