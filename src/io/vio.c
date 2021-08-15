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
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>

#include "../util.h"
#include "memfile.h"
#include "vio.h"
//#include "zip.h"

#ifdef __WIN32__
#include "vio_win32.h"
#else
#include "vio_posix.h"
#endif

#ifndef VFILE_SMALL_BUFFER_SIZE
#define VFILE_SMALL_BUFFER_SIZE 256
#endif

#ifndef VFILE_LARGE_BUFFER_SIZE
#define VFILE_LARGE_BUFFER_SIZE 32768
#endif

enum vfileflags_private
{
  VF_FILE               = (1<<0),
  VF_MEMORY             = (1<<1),
  VF_MEMORY_EXPANDABLE  = (1<<2),
  VF_MEMORY_FREE        = (1<<3), // Free memory buffer on vfclose.
//VF_IN_ARCHIVE         = (1<<3),
  VF_READ               = (1<<4),
  VF_WRITE              = (1<<5),
  VF_APPEND             = (1<<6),
  VF_BINARY             = (1<<7),
  VF_TRUNCATE           = (1<<8),

  VF_STORAGE_MASK       = (VF_FILE | VF_MEMORY),
  VF_PUBLIC_MASK        = (V_SMALL_BUFFER | V_LARGE_BUFFER)
};

struct vfile
{
  FILE *fp;

  struct memfile mf;
  // Local copy of pointer/size of a memory vfile buffer (expandable or not).
  void *local_buffer;
  size_t local_buffer_size;
  // External copy of pointer/size of expandable vfile buffer.
  void **external_buffer;
  size_t *external_buffer_size;

  // vungetc buffer for memory files.
  int tmp_chr;
  int flags;
};

/**
 * Parse vfile mode flags from a standard fopen mode string.
 */
static int get_vfile_mode_flags(const char *mode)
{
  int flags = 0;

  assert(mode);

  switch(*(mode++))
  {
    case 'r':
      flags |= VF_READ;
      break;

    case 'w':
      flags |= VF_WRITE | VF_TRUNCATE;
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

      // Explicitly "text" mode. Does nothing, but some libcs support it.
      case 't':
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
 * Open a file for input or output with user-defined flags.
 */
vfile *vfopen_unsafe_ext(const char *filename, const char *mode,
 int user_flags)
{
  int flags = get_vfile_mode_flags(mode);
  vfile *vf = NULL;

  assert(filename && flags);
  flags |= (user_flags & VF_PUBLIC_MASK);

  // TODO archive detection/memory file caching here
  // TODO vf = vfscache_vfopen(filename, flags);

  // Open the vfile as a normal file.
  if(!vf)
  {
    FILE *fp = platform_fopen_unsafe(filename, mode);

    if(fp)
    {
      if((flags & VF_BINARY) && (!(flags & VF_READ) || !(flags & VF_WRITE)))
      {
        if(flags & V_LARGE_BUFFER)
        {
          setvbuf(fp, NULL, _IOFBF, VFILE_LARGE_BUFFER_SIZE);
          flags &= ~V_SMALL_BUFFER;
        }
        else

        if(flags & V_SMALL_BUFFER)
          setvbuf(fp, NULL, _IOFBF, VFILE_SMALL_BUFFER_SIZE);
      }
      else
        flags &= ~(V_SMALL_BUFFER | V_LARGE_BUFFER);

      vf = (vfile *)ccalloc(1, sizeof(vfile));
      vf->fp = fp;
      vf->tmp_chr = EOF;
      vf->flags = flags | VF_FILE;
      return vf;
    }
  }
  return NULL;
}

/**
 * Open a file for input or output.
 */
vfile *vfopen_unsafe(const char *filename, const char *mode)
{
  return vfopen_unsafe_ext(filename, mode, 0);
}

/**
 * Create a vfile from an existing fp.
 */
vfile *vfile_init_fp(FILE *fp, const char *mode)
{
  int flags = get_vfile_mode_flags(mode);
  vfile *vf = NULL;

  assert(fp && flags);

  vf = (vfile *)ccalloc(1, sizeof(vfile));
  vf->fp = fp;
  vf->tmp_chr = EOF;
  vf->flags = flags | VF_FILE;
  return vf;
}

/**
 * Create a vfile from an existing memory buffer.
 */
vfile *vfile_init_mem(void *buffer, size_t size, const char *mode)
{
  int flags = get_vfile_mode_flags(mode);
  vfile *vf = NULL;
  size_t filesize = size;

  assert((buffer && size) || (!buffer && !size));
  assert(flags);

  // "w"-based modes should start at size 0 and expand as-needed, either in
  // their fixed buffer or with an extendable buffer (see vfile_init_mem_ext).
  if(flags & VF_TRUNCATE)
    filesize = 0;

  vf = (vfile *)ccalloc(1, sizeof(vfile));
  mfopen(buffer, filesize, &(vf->mf));
  vf->mf.seek_past_end = true;
  vf->tmp_chr = EOF;
  vf->flags = flags | VF_MEMORY;
  vf->local_buffer = buffer;
  vf->local_buffer_size = size;
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
 * Create a temporary vfile.
 * If `initial_size` is non-zero, this function will attempt to allocate an
 * expandable temporary file in RAM with this size. If allocation fails or if
 * `initial_size` is zero, tmpfile() will be called instead.
 */
vfile *vtempfile(size_t initial_size)
{
  FILE *fp;
  vfile *vf;

  if(initial_size)
  {
    void *buffer = malloc(initial_size);
    if(buffer)
    {
      vf = vfile_init_mem(buffer, initial_size, "wb+");
      vf->flags |= VF_MEMORY_EXPANDABLE | VF_MEMORY_FREE;
      vf->local_buffer = buffer;
      vf->local_buffer_size = initial_size;
      return vf;
    }
  }

  fp = platform_tmpfile();
  if(fp)
  {
    setvbuf(fp, NULL, _IOFBF, VFILE_SMALL_BUFFER_SIZE);
    return vfile_init_fp(fp, "wb+");
  }

  return NULL;
}

/**
 * Close a file. The file pointer should not be used after using this function.
 */
int vfclose(vfile *vf)
{
  int retval = 0;

  assert(vf);

  if((vf->flags & VF_MEMORY) && (vf->flags & VF_MEMORY_FREE))
  {
    free(vf->local_buffer);
    if(vf->external_buffer)
    {
      // Make sure these didn't desync somehow...
      assert(vf->local_buffer == *(vf->external_buffer));
      *(vf->external_buffer) = NULL;
    }
  }

  if(vf->flags & VF_FILE)
    retval = fclose(vf->fp);

  free(vf);
  return retval;
}


/************************************************************************
 * Filesystem function wrappers.
 */

/**
 * Change the current working directory to path.
 */
int vchdir(const char *path)
{
  // TODO archive detection, etc
  return platform_chdir(path);
}

/**
 * Print the name of the current working directory to buffer 'buf' of
 * length 'size'.
 */
char *vgetcwd(char *buf, size_t size)
{
  // TODO archive detection, etc
  return platform_getcwd(buf, size);
}

/**
 * Make a directory with a given name.
 */
int vmkdir(const char *path, int mode)
{
  // TODO archive detection, etc
  return platform_mkdir(path, mode);
}

/**
 * Rename a file or directory.
 */
int vrename(const char *oldpath, const char *newpath)
{
  // TODO archive detection, etc
  return platform_rename(oldpath, newpath);
}

/**
 * Unlink a name (and delete a file if it was the last link to the file) if
 * the file is a real file.
 */
int vunlink(const char *path)
{
  // TODO archive detection, cache, etc
  return platform_unlink(path);
}

/**
 * Remove an empty directory at a given path.
 */
int vrmdir(const char *path)
{
  // TODO archive detection, etc
  return platform_rmdir(path);
}

/**
 * Check an access permission for a path for the current process.
 *
 * F_OK   (checks existence only)
 * R_OK   Read permission.
 * W_OK   Write permission.
 * X_OK   Execute permission.
 */
int vaccess(const char *path, int mode)
{
  // TODO archive detection, etc
  return platform_access(path, mode);
}

/**
 * Get file information for a file at a given path.
 */
int vstat(const char *path, struct stat *buf)
{
  // TODO archive detection, cache, etc
  return platform_stat(path, buf);
}


/************************************************************************
 * stdio function wrappers.
 */

/**
 * Ensure an amount of space exists in a memory vfile or expand the vfile
 * (if possible). This should be used for writing only.
 */
static inline boolean vfile_ensure_space(size_t amount_to_write, vfile *vf)
{
  struct memfile *mf = &(vf->mf);
  size_t new_size;

  assert(vf->flags & VF_MEMORY);

  /* In append mode, all writes should occur at the end. */
  if(vf->flags & VF_APPEND)
    mf->current = mf->end;

  if(!mfhasspace(amount_to_write, mf))
  {
    new_size = (mf->start ? (mf->current - mf->start) : 0) + amount_to_write;
    if(new_size > vf->local_buffer_size)
    {
      size_t new_size_alloc = 32;
      void *t;
      int i;

      if(!(vf->flags & VF_MEMORY_EXPANDABLE))
        return false;

      for(i = 0; i < 64 && new_size_alloc < new_size; i++)
        new_size_alloc <<= 1;

      if(i >= 64)
        return false;

      t = realloc(vf->local_buffer, new_size_alloc);
      if(!t)
        return false;

      vf->local_buffer = t;
      vf->local_buffer_size = new_size_alloc;

      if(vf->external_buffer)
        *(vf->external_buffer) = t;
      if(vf->external_buffer_size)
        *(vf->external_buffer_size) = new_size_alloc;
    }
    mfmove(vf->local_buffer, new_size, mf);

    return (mf->end - mf->start) >= (ptrdiff_t)new_size;
  }
  return true;
}

/**
 * Get a direct memory access memfile of a given length starting at the current
 * position of a memory vfile. This should pretty much not be used, but some
 * things in the zip code already relied on it. If dest is NULL, a size check
 * (and potential expansion) will still be performed.
 */
boolean vfile_get_memfile_block(vfile *vf, size_t length, struct memfile *dest)
{
  assert(vf);
  assert(vf->flags & VF_MEMORY);
  if(vf->flags & VF_WRITE)
  {
    if(!vfile_ensure_space(length, vf))
      return false;
  }
  if(!mfhasspace(length, &(vf->mf)))
    return false;

  if(dest)
    mfopen(vf->mf.current, length, dest);
  return true;
}

/**
 * Read a single byte from a file.
 */
int vfgetc(vfile *vf)
{
  assert(vf);
  assert(vf->flags & VF_STORAGE_MASK);
  assert(vf->flags & VF_READ);

  if(vf->flags & VF_MEMORY)
  {
    int tmp = vf->tmp_chr;
    vf->tmp_chr = EOF;
    if(tmp != EOF)
      return tmp;

    return mfhasspace(1, &(vf->mf)) ? mfgetc(&(vf->mf)) : EOF;
  }

  if(vf->flags & VF_FILE)
    return fgetc(vf->fp);

  return EOF;
}

/**
 * Read two bytes from a file as an unsigned integer (little endian).
 */
int vfgetw(vfile *vf)
{
  assert(vf);
  assert(vf->flags & VF_STORAGE_MASK);
  assert(vf->flags & VF_READ);

  if(vf->flags & VF_MEMORY)
  {
    int tmp = vf->tmp_chr;
    if(tmp != EOF)
    {
      if(!mfhasspace(1, &(vf->mf)))
        return EOF;

      vf->tmp_chr = EOF;
      return tmp | (mfgetc(&(vf->mf)) << 8);
    }

    return mfhasspace(2, &(vf->mf)) ? mfgetw(&(vf->mf)) : EOF;
  }

  if(vf->flags & VF_FILE)
  {
    FILE *fp = vf->fp;
    int a = fgetc(fp);
    int b = fgetc(fp);

    return (a != EOF) && (b != EOF) ? ((b << 8) | a) : EOF;
  }

  return EOF;
}

/**
 * Read four bytes from a file as a signed integer (little endian).
 */
int vfgetd(vfile *vf)
{
  assert(vf);
  assert(vf->flags & VF_STORAGE_MASK);
  assert(vf->flags & VF_READ);

  if(vf->flags & VF_MEMORY)
  {
    int tmp = vf->tmp_chr;
    if(tmp != EOF)
    {
      if(!mfhasspace(3, &(vf->mf)))
        return EOF;

      vf->tmp_chr = EOF;
      return tmp | (mfgetc(&(vf->mf)) << 8) | (mfgetw(&(vf->mf)) << 16);
    }

    return mfhasspace(4, &(vf->mf)) ? mfgetd(&(vf->mf)) : EOF;
  }

  if(vf->flags & VF_FILE)
  {
    FILE *fp = vf->fp;
    int a = fgetc(fp);
    int b = fgetc(fp);
    int c = fgetc(fp);
    int d = fgetc(fp);

    if((a == EOF) || (b == EOF) || (c == EOF) || (d == EOF))
      return EOF;

    return ((uint32_t)d << 24) | (c << 16) | (b << 8) | a;
  }

  return EOF;
}

/**
 * Write a single byte to a file.
 */
int vfputc(int character, vfile *vf)
{
  assert(vf);
  assert(vf->flags & VF_STORAGE_MASK);
  assert(vf->flags & VF_WRITE);

  if(vf->flags & VF_MEMORY)
  {
    if(!vfile_ensure_space(1, vf))
      return EOF;

    return mfputc(character, &(vf->mf));
  }

  if(vf->flags & VF_FILE)
    return fputc(character, vf->fp);

  return EOF;
}

/**
 * Write an unsigned short integer to a file (little endian).
 */
int vfputw(int character, vfile *vf)
{
  assert(vf);
  assert(vf->flags & VF_STORAGE_MASK);
  assert(vf->flags & VF_WRITE);

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

  return EOF;
}

/**
 * Write a signed long integer to a file (little endian).
 */
int vfputd(int character, vfile *vf)
{
  assert(vf);
  assert(vf->flags & VF_STORAGE_MASK);
  assert(vf->flags & VF_WRITE);

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

  return EOF;
}

/**
 * Read an array from a file.
 */
size_t vfread(void *dest, size_t size, size_t count, vfile *vf)
{
  assert(vf);
  assert(dest);
  assert(vf->flags & VF_STORAGE_MASK);
  assert(vf->flags & VF_READ);

  if(vf->flags & VF_MEMORY)
  {
    if(size && count && vf->tmp_chr != EOF)
    {
      char *d = (char *)dest;
      *(d++) = vf->tmp_chr;
      if(size > 1)
      {
        if(!mfread(d, size - 1, 1, &(vf->mf)))
          return 0;

        d += size - 1;
      }
      vf->tmp_chr = EOF;
      return (count > 1) ? mfread(d, size, count - 1, &(vf->mf)) + 1 : 1;
    }

    return mfread(dest, size, count, &(vf->mf));
  }

  if(vf->flags & VF_FILE)
    return fread(dest, size, count, vf->fp);

  return 0;
}

/**
 * Write an array to a file.
 */
size_t vfwrite(const void *src, size_t size, size_t count, vfile *vf)
{
  assert(vf);
  assert(src);
  assert(vf->flags & VF_STORAGE_MASK);
  assert(vf->flags & VF_WRITE);

  if(vf->flags & VF_MEMORY)
  {
    if(!vfile_ensure_space(size * count, vf))
      return 0;

    return mfwrite(src, size, count, &(vf->mf));
  }

  if(vf->flags & VF_FILE)
    return fwrite(src, size, count, vf->fp);

  return 0;
}

/**
 * Read a line from a file, safely trimming platform-specific line end chars
 * as-needed.
 */
char *vfsafegets(char *dest, int size, vfile *vf)
{
  assert(vf);
  assert(dest);
  assert(size > 1);
  assert(vf->flags & VF_STORAGE_MASK);
  assert(vf->flags & VF_READ);

  if(vf->flags & VF_MEMORY)
  {
    if(size && vf->tmp_chr != EOF)
    {
      int tmp = vf->tmp_chr;
      vf->tmp_chr = EOF;
      if(tmp == '\r' || tmp == '\n')
      {
        // If this \r is part of a DOS line end, consume the corresponding \n.
        if(tmp == '\r' && (mfhasspace(1, &(vf->mf)) && vf->mf.current[0] == '\n'))
          vf->mf.current++;
        dest[0] = '\0';
        return dest;
      }
      dest[0] = tmp;
      dest[1] = '\0';
      mfsafegets(dest + 1, size - 1, &(vf->mf));
      return dest;
    }
    return mfsafegets(dest, size, &(vf->mf));
  }

  if(vf->flags & VF_FILE)
  {
    if(fgets(dest, size, vf->fp))
    {
      size_t len = strlen(dest);
      while(len >= 1 && (dest[len - 1] == '\r' || dest[len - 1] == '\n'))
      {
        len--;
        dest[len] = '\0';
      }
      return dest;
    }
    return NULL;
  }

  return NULL;
}

/**
 * Write a null-terminated string to a file. The null terminator will not be
 * written to the file.
 */
int vfputs(const char *src, vfile *vf)
{
  size_t len;
  int ret;

  assert(vf);
  assert(src);
  assert(vf->flags & VF_STORAGE_MASK);
  assert(vf->flags & VF_WRITE);

  len = strlen(src);
  if(!len)
    return 0;

  ret = vfwrite(src, len, 1, vf);
  return (ret == 1) ? 0 : EOF;
}

/**
 * Print a formatted string with a variable number of parameters to a file.
 * This function internally uses vfprintf and vsnprintf.
 */
int vf_printf(vfile *vf, const char *fmt, ...)
{
  va_list args;
  int ret;

  va_start(args, fmt);
  ret = vf_vprintf(vf, fmt, args);
  va_end(args);

  return ret;
}

/**
 * Print a formatted string with a variable number of parameters in a va_list
 * to a file. This function internally uses vfprintf and vsnprintf.
 */
int vf_vprintf(vfile *vf, const char *fmt, va_list args)
{
  assert(vf);
  assert(fmt);
  assert(vf->flags & VF_STORAGE_MASK);
  assert(vf->flags & VF_WRITE);

  if(vf->flags & VF_MEMORY)
  {
    // Get the expected output length from the format string and args.
    // This requires a duplicate arglist so vsnprintf can be called twice.
    // va_copy is supported by MSVC 2015+.
    va_list tmp_args;
    int length;
    char _buffer[512];
    void *buffer;

    va_copy(tmp_args, args);
    length = vsnprintf(NULL, 0, fmt, tmp_args);
    va_end(tmp_args);

    // Note: vsnprintf will fail if this is actually MSVC's broken _vsnprintf.
    // This shouldn't happen since only MinGW and MSVC 2015+ are supported.
    // Alternatively, some errors cause it to return -1.
    if(length < 0)
      return -1;

    // Write to a temporary buffer to avoid null terminators overwriting data
    // and potential side effects of allocating extra vfile space. This is a
    // little slower but less messy than the alternative.
    if((size_t)length >= sizeof(_buffer))
    {
      buffer = malloc(length + 1);
      if(!buffer)
        return -1;
    }
    else
      buffer = _buffer;

    length = vsnprintf((char *)buffer, length + 1, fmt, args);

    if(vfile_ensure_space(length, vf))
    {
      mfwrite(buffer, length, 1, &(vf->mf));
    }
    else
      length = -1;

    if(buffer != _buffer)
      free(buffer);

    return length;
  }

  if(vf->flags & VF_FILE)
    return vfprintf(vf->fp, fmt, args);

  return -1;
}

/**
 * Place a character back into the stream. This can be used once only for
 * memory streams and is only guaranteed to be usable once for files.
 * If chr is EOF or otherwise does not represent a valid char, this function
 * will fail and the stream will be unmodified.
 */
int vungetc(int chr, vfile *vf)
{
  assert(vf);
  assert(vf->flags & VF_STORAGE_MASK);

  // Note: MSVCRT &255s non-EOF values so this may not be 100% accurate to stdio.
  // If this somehow breaks something it can be reverted later.
  if(chr < 0 || chr >= 256)
    return EOF;

  if(vf->flags & VF_MEMORY)
  {
    if(vf->tmp_chr != EOF)
      return EOF;

    vf->tmp_chr = chr;
    return chr;
  }

  if(vf->flags & VF_FILE)
    return ungetc(chr, vf->fp);

  return EOF;
}

/**
 * Seek to a position in a file.
 */
int vfseek(vfile *vf, long int offset, int whence)
{
  assert(vf);
  assert(vf->flags & VF_STORAGE_MASK);
  vf->tmp_chr = EOF;

  if(vf->flags & VF_MEMORY)
    return mfseek(&(vf->mf), offset, whence);

  if(vf->flags & VF_FILE)
    return fseek(vf->fp, offset, whence);

  return -1;
}

/**
 * Return the current position in a file.
 */
long int vftell(vfile *vf)
{
  assert(vf);
  assert(vf->flags & VF_STORAGE_MASK);

  if(vf->flags & VF_MEMORY)
  {
    long res = mftell(&(vf->mf));
    /**
     * The number of buffered chars should be subtracted from the cursor for
     * binary mode files (in text mode the behavior is unspecified.). If the
     * cursor is at position 0 the return value of this operation is undefined.
     */
    if((vf->tmp_chr != EOF) && (vf->flags & VF_BINARY) && res > 0)
      return res - 1;

    return res;
  }

  if(vf->flags & VF_FILE)
    return ftell(vf->fp);

  return -1;
}

/**
 * Rewind the file to the start (and clear EOF/errors if applicable).
 */
void vrewind(vfile *vf)
{
  assert(vf);
  assert(vf->flags & VF_STORAGE_MASK);
  vf->tmp_chr = EOF;

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
}

/**
 * Return the length of the file and optionally rewind to the start of it.
 * If rewind is false, this function is guaranteed to either not modify the
 * current file position or to restore it to its position prior to calling this.
 * This function is not guaranteed to work correctly on streams that have been
 * written to or on memory write streams with fixed buffers, but is probably
 * safe to use for these cases.
 */
long vfilelength(vfile *vf, boolean rewind)
{
  long size = -1;

  assert(vf);
  assert(vf->flags & VF_STORAGE_MASK);

  if(vf->flags & VF_MEMORY)
  {
    size = vf->mf.end - vf->mf.start;
  }

  if(vf->flags & VF_FILE)
  {
    struct stat st;
    int fd = fileno(vf->fp);

    // fstat (and maybe _filelength) rely on the copy on-disk being up to date.
    // The SEEK_END hack works without this, since fseek also flushes the file.
    if(vf->flags & VF_WRITE)
      fflush(vf->fp);

#ifdef __WIN32__
    size = _filelength(fd);
    if(size < 0)
#endif
    {
      if(!fstat(fd, &st))
      {
        size = st.st_size;
      }
      else
      {
        long current_pos = vftell(vf);

        vfseek(vf, 0, SEEK_END);
        size = vftell(vf);

        if(!rewind)
          vfseek(vf, current_pos, SEEK_SET);
      }
    }
  }

  if(rewind)
    vrewind(vf);

  return size;
}


/************************************************************************
 * dirent function wrappers.
 */

struct vdir
{
  struct dir_handle dh;
  long position;
  long num_total;
  long num_real;
#ifdef PLATFORM_NO_REWINDDIR
  // Path for dirent implementations with no working rewinddir.
  char path[1];
#endif
};

/**
 * Open a directory for reading.
 */
vdir *vdir_open(const char *path)
{
#ifdef PLATFORM_NO_REWINDDIR

  size_t pathlen = strlen(path);
  vdir *dir = (vdir *)malloc(sizeof(vdir) + pathlen);
  if(dir)
    memcpy(dir->path, path, pathlen + 1);

#else
  vdir *dir = (vdir *)malloc(sizeof(vdir));
#endif

  if(dir)
  {
    dir->position = 0;
    dir->num_total = 0;
    dir->num_real = 0;

    // TODO archive detection, etc
    if(!platform_opendir(&(dir->dh), path))
      goto err;

    // Get total real files.
    // TODO get total virtual files with no real backing.
    while(platform_readdir(dir->dh, NULL, 0, NULL))
      dir->num_real++;

    dir->num_total += dir->num_real;
    vdir_rewind(dir);
    return dir;
  }

err:
  free(dir);
  return NULL;
}

/**
 * Close a directory.
 */
void vdir_close(vdir *dir)
{
  platform_closedir(dir->dh);
  free(dir);
}

/**
 * Read the next file from a directory.
 * Returns `true` if a file was read, otherwise `false`.
 */
boolean vdir_read(vdir *dir, char *buffer, size_t len, enum vdir_type *type)
{
  int dirent_type = -1;

  if(dir->position >= dir->num_total)
    return false;

/*
  // TODO virtual files with no real backing.
  if(dir->position >= dir->num_real)
*/

  if(!platform_readdir(dir->dh, buffer, len, &dirent_type))
    return false;

  if(type)
  {
    switch(dirent_type)
    {
  #ifdef DT_UNKNOWN
      case DT_REG:
        *type = DIR_TYPE_FILE;
        break;

      case DT_DIR:
        *type = DIR_TYPE_DIR;
        break;
  #endif

      default:
        *type = DIR_TYPE_UNKNOWN;
        break;
    }
  }

  dir->position++;
  return true;
}

/**
 * Seek to a position in a directory.
 */
boolean vdir_seek(vdir *dir, long position)
{
  if(position < 0)
    return false;

  // If position is before current position, rewind.
  if(position < dir->position)
  {
    if(!vdir_rewind(dir))
      return false;
  }

  // Skip files until the current position is the requested position.
  while(dir->position < position)
    if(!vdir_read(dir, NULL, 0, NULL))
      break;

  return true;
}

/**
 * Rewind the directory to the first file.
 * Some platforms don't have a rewinddir implementation and need special
 * handling, hence both vdir_open and vdir_seek call this instead of rewinddir.
 */
boolean vdir_rewind(vdir *dir)
{
  dir->position = 0;
  if(!dir->num_real)
    return true;

#ifdef PLATFORM_NO_REWINDDIR
  platform_closedir(dir->dh);
  // TODO archive detection, etc
  if(!platform_opendir(&(dir->dh), dir->path))
  {
    dir->num_total -= dir->num_real;
    dir->num_real = 0;
    return false;
  }
  return true;
#else
  return platform_rewinddir(dir->dh);
#endif
}

/**
 * Report the current position in a directory.
 */
long vdir_tell(vdir *dir)
{
  return dir->position;
}

/**
 * Report the length of a directory.
 */
long vdir_length(vdir *dir)
{
  return dir->num_total;
}
