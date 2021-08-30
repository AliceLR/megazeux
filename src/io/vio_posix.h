/* MegaZeux
 *
 * Copyright (C) 2020 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __IO_VIO_POSIX_H
#define __IO_VIO_POSIX_H

#include "../compat.h"

__M_BEGIN_DECLS

/**
 * Standard path function and dirent wrappers.
 * On most relevant platforms these already support UTF-8.
 */

#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

// pspdev/devkitPSP historically does not have a rewinddir implementation.
// libctru (3DS) and libnx (Switch) have rewinddir but it doesn't work.
#if defined(CONFIG_3DS) || defined(CONFIG_PSP) || defined(CONFIG_SWITCH)
#define PLATFORM_NO_REWINDDIR
#endif

#ifdef __USE_LARGEFILE64
/**
 * Regular readdir/dirent can cause issues for 32-bit executables running
 * on large filesystems. This is caused by values for d_ino or d_off
 * overflowing their respective fields (even though this code doesn't
 * care about them). Prefer dirent64 if it is available.
 *
 * NOTE: __USE_LARGEFILE64 is a glibc-specific internal define, but there
 * doesn't seem to be a better way to detect this. dirent64 is generally a
 * glibc extension. Bionic and musl both support dirent64, but their dirent64
 * structs are identical to their regular dirent structs. Newlib does not
 * support dirent64, but primarily targets platforms where this is irrelevant.
 */
#define PLATFORM_HAS_DIRENT64
#endif


static inline FILE *platform_fopen_unsafe(const char *path, const char *mode)
{
  return fopen_unsafe(path, mode);
}

static inline FILE *platform_tmpfile(void)
{
#ifdef CONFIG_NDS
  // tmpfile() pulls in the non-integer-only variant of sprintf from newlib,
  // while the only functions currently using tmpfile() have a fallback.
  return NULL;
#else
  return tmpfile();
#endif /* CONFIG_NDS */
}

static inline char *platform_getcwd(char *buf, size_t size)
{
  return getcwd(buf, size);
}

static inline int platform_chdir(const char *path)
{
  return chdir(path);
}

static inline int platform_mkdir(const char *path, int mode)
{
  return mkdir(path, mode);
}

static inline int platform_rename(const char *oldpath, const char *newpath)
{
  return rename(oldpath, newpath);
}

static inline int platform_unlink(const char *path)
{
  return unlink(path);
}

static inline int platform_rmdir(const char *path)
{
  return rmdir(path);
}

static inline int platform_access(const char *path, int mode)
{
  return access(path, mode);
}

static inline int platform_stat(const char *path, struct stat *buf)
{
  return stat(path, buf);
}

struct dir_handle
{
  DIR *dir;
};

static inline boolean platform_opendir(struct dir_handle *dh, const char *path)
{
  dh->dir = opendir(path);
  return dh->dir != NULL;
}

static inline void platform_closedir(struct dir_handle dh)
{
  closedir(dh.dir);
}

static inline boolean platform_readdir(struct dir_handle dh, char *buffer,
 size_t buffer_len, int *d_type)
{
#ifdef PLATFORM_HAS_DIRENT64

  struct dirent64 *d = readdir64(dh.dir);

#else

  struct dirent *d = readdir(dh.dir);

#endif

  if(!d)
    return false;

  if(buffer && buffer_len)
    snprintf(buffer, buffer_len, "%s", d->d_name);

#ifdef DT_UNKNOWN
  if(d_type)
    *d_type = d->d_type;
#endif

  return true;
}

static inline boolean platform_rewinddir(struct dir_handle dh)
{
#ifndef PLATFORM_NO_REWINDDIR
  rewinddir(dh.dir);
  return true;
#endif

  return false;
}

__M_END_DECLS

#endif /* __IO_VIO_POSIX_H */
