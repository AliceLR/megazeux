/* MegaZeux
 *
 * Copyright (C) 2018 Alice Rowan <petrifiedrowan@gmail.com>
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
#include <sys/stat.h>

static inline FILE *_real_fopen(const char *path, const char *mode)
 { return fopen(path, mode); }

static inline char *_real_getcwd(char *buf, size_t size)
 { return getcwd(buf, size); }

static inline int _real_chdir(const char *path)
 { return chdir(path); }

static inline int _real_unlink(const char *path)
 { return unlink(path); }

static inline int _real_stat(const char *path, struct stat *buf)
 { return stat(path, buf); }

// Including any of these will replace the names of the real functions above.
#include "path.h"
#include "util.h"

/**
 * The Windows versions of the functions we're replacing do not natively
 * support UTF-8 encoding, and the Win32 versions use UTF-16 instead.
 */
#ifdef __WIN32__
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>

// Return lengths for these functions include null terminator. Zero=error.
static int utf8_to_utf16(const char *src, wchar_t *dest, int dest_size)
{
  return MultiByteToWideChar(
    CP_UTF8,
    0,
    (LPCCH)src,
    -1, // Null terminated.
    (LPWSTR)dest,
    dest_size
  );
}

static int utf16_to_utf8(const wchar_t *src, char *dest, int dest_size)
{
  return WideCharToMultiByte(
    CP_UTF8,
    0,
    (LPCWCH)src,
    -1, // Null terminated.
    (LPSTR)dest,
    dest_size,
    NULL,
    NULL
  );
}
#endif

/**
 * path is UTF-8 on systems that support it (and Windows).
 */
FILE *fopen_unsafe(const char *path, const char *mode)
{
#ifdef __WIN32__
  wchar_t wpath[MAX_PATH];
  wchar_t wmode[64];

  if(utf8_to_utf16(path, wpath, MAX_PATH))
    if(utf8_to_utf16(mode, wmode, 64))
      return _wfopen(wpath, wmode);
#endif

  return _real_fopen(path, mode);
}

/**
 * buf should contain a UTF-8 path on systems that support it (and Windows).
 */
char *mzx_getcwd(char *buf, size_t size)
{
#ifdef __WIN32__
  wchar_t wpath[MAX_PATH];

  if(!_wgetcwd(wpath, MAX_PATH))
    return NULL;

  if(utf16_to_utf8(wpath, buf, size))
    return buf;
#endif

  return _real_getcwd(buf, size);
}

int mzx_chdir(const char *path)
{
#ifdef __WIN32__
  wchar_t wpath[MAX_PATH];

  if(utf8_to_utf16(path, wpath, MAX_PATH))
    return _wchdir(wpath);
#endif

  return _real_chdir(path);
}

int mzx_unlink(const char *path)
{
#ifdef __WIN32__
  wchar_t wpath[MAX_PATH];

  if(utf8_to_utf16(path, wpath, MAX_PATH))
    return _wunlink(wpath);
#endif

  return _real_unlink(path);
}

int mzx_stat(const char *path, struct stat *buf)
{
#ifdef __WIN32__
  wchar_t wpath[MAX_PATH];
  struct _stat stat_info;
  int ret;

  if(utf8_to_utf16(path, wpath, MAX_PATH))
  {
    ret =  _wstat(wpath, &stat_info);

    buf->st_gid = stat_info.st_gid;
    buf->st_atime = (time_t)stat_info.st_atime;
    buf->st_ctime = (time_t)stat_info.st_ctime;
    buf->st_dev = stat_info.st_dev;
    buf->st_ino = stat_info.st_ino;
    buf->st_mode = stat_info.st_mode;
    buf->st_mtime = (time_t)stat_info.st_mtime;
    buf->st_nlink = stat_info.st_nlink;
    buf->st_rdev = stat_info.st_rdev;
    buf->st_size = (_off_t)stat_info.st_size;
    buf->st_uid = stat_info.st_uid;
    return ret;
  }
#endif

  return _real_stat(path, buf);
}
