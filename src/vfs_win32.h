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

#ifndef __VFS_WIN32_H
#define __VFS_WIN32_H

#include "compat.h"

__M_BEGIN_DECLS

/**
 * Win32 UTF-8 stdio path function wrappers.
 * dirent equivalents are in dir.c.
 */

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>

/**
 * Convert a UTF-8 char string into a UTF-16 wide char string for use with
 * Win32 wide char functions. Returns the length of the output (including the
 * null terminator) or 0 on failure.
 */
static inline int utf8_to_utf16(const char *src, wchar_t *dest, int dest_size)
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

/**
 * Convert a UTF-16 wide char string into a UTF-8 char string for general
 * usage. Returns the length of the output (including the null terminator)
 * or 0 on failure.
 */
static inline int utf16_to_utf8(const wchar_t *src, char *dest, int dest_size)
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

static inline FILE *platform_fopen_unsafe(const char *path, const char *mode)
{
  wchar_t wpath[MAX_PATH];
  wchar_t wmode[64];

  if(utf8_to_utf16(path, wpath, MAX_PATH))
    if(utf8_to_utf16(mode, wmode, 64))
      return _wfopen(wpath, wmode);

  return fopen_unsafe(path, mode);
}

static inline char *platform_getcwd(char *buf, size_t size)
{
  wchar_t wpath[MAX_PATH];

  if(!_wgetcwd(wpath, MAX_PATH))
    return NULL;

  if(utf16_to_utf8(wpath, buf, size))
    return buf;

  return getcwd(buf, size);
}

static inline int platform_chdir(const char *path)
{
  wchar_t wpath[MAX_PATH];

  if(utf8_to_utf16(path, wpath, MAX_PATH))
    return _wchdir(wpath);

  return chdir(path);
}

static inline int platform_mkdir(const char *path, int mode)
{
  wchar_t wpath[MAX_PATH];

  if(utf8_to_utf16(path, wpath, MAX_PATH))
    return _wmkdir(wpath);

  return mkdir(path);
}

static inline int platform_unlink(const char *path)
{
  wchar_t wpath[MAX_PATH];

  if(utf8_to_utf16(path, wpath, MAX_PATH))
    return _wunlink(wpath);

  return unlink(path);
}

static inline int platform_rmdir(const char *path)
{
  wchar_t wpath[MAX_PATH];

  if(utf8_to_utf16(path, wpath, MAX_PATH))
    return _wrmdir(wpath);

  return rmdir(path);
}

static inline int platform_stat(const char *path, struct stat *buf)
{
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
  return stat(path, buf);
}

__M_END_DECLS

#endif /* __VFS_WIN32_H */
