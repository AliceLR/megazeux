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
#include <dirent.h>

static inline FILE *_real_fopen(const char *path, const char *mode)
 { return fopen(path, mode); }

static inline char *_real_getcwd(char *buf, size_t size)
 { return getcwd(buf, size); }

static inline int _real_chdir(const char *path)
 { return chdir(path); }

static inline int _real_unlink(const char *path)
 { return unlink(path); }

static inline int _real_rmdir(const char *path)
 { return rmdir(path); }

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

int mzx_rmdir(const char *path)
{
#ifdef __WIN32__
  wchar_t wpath[MAX_PATH];

  if(utf8_to_utf16(path, wpath, MAX_PATH))
    return _wrmdir(wpath);
#endif

  return _real_rmdir(path);
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

static inline boolean platform_opendir(struct mzx_dir *dir, const char *path)
{
#ifdef __WIN32__
  wchar_t wpath[MAX_PATH];
  dir->is_wdirent = false;

  if(utf8_to_utf16(path, wpath, MAX_PATH))
  {
    dir->opaque = _wopendir(wpath);
    if(dir->opaque)
    {
      dir->is_wdirent = true;
      return true;
    }
  }
#endif

#if defined(CONFIG_PSP) || defined(CONFIG_3DS)
  if(dir->path != path)
  {
    snprintf(dir->path, PATH_BUF_LEN, "%s%c*", path,
      path[strlen(path) - 1] != DIR_SEPARATOR_CHAR ? DIR_SEPARATOR_CHAR : 0
    );
  }
#endif

  dir->opaque = opendir(path);
  return !!dir->opaque;
}

static inline void platform_closedir(struct mzx_dir *dir)
{
#ifdef __WIN32__
  if(dir->is_wdirent)
  {
    _wclosedir(dir->opaque);
    return;
  }
#endif

  closedir(dir->opaque);
}

#ifdef DT_UNKNOWN
/**
 * On platforms that support it, the d_type field can be used to avoid
 * stat calls. This is critical for the file manager on embedded platforms.
 */
static inline int resolve_dirent_d_type(int d_type)
{
  switch(d_type)
  {
    case DT_REG:
      return DIR_TYPE_FILE;

    case DT_DIR:
      return DIR_TYPE_DIR;

    default:
      return DIR_TYPE_UNKNOWN;
  }
}
#endif

static inline boolean platform_readdir(struct mzx_dir *dir,
 char *dest, size_t dest_len, int *type)
{
  struct dirent *inode;

#ifdef __WIN32__
  if(dir->is_wdirent)
  {
    struct _wdirent *w_inode = _wreaddir(dir->opaque);
    if(!w_inode)
    {
      if(dest)
        dest[0] = 0;
      return false;
    }

    if(type)
    {
#ifdef DT_UNKNOWN
      *type = resolve_dirent_d_type(w_inode->d_type);
#else
      *type = DIR_TYPE_UNKNOWN;
#endif
    }

    if(dest)
      return !!utf16_to_utf8(w_inode->d_name, dest, dest_len);

    return true;
  }
#endif

  inode = readdir(dir->opaque);
  if(!inode)
  {
    if(dest)
      dest[0] = 0;
    return false;
  }

  if(type)
  {
#ifdef DT_UNKNOWN
    *type = resolve_dirent_d_type(inode->d_type);
#else
    *type = DIR_TYPE_UNKNOWN;
#endif
  }

  if(dest)
    snprintf(dest, dest_len, "%s", inode->d_name);

  return true;
}

static inline boolean platform_rewinddir(struct mzx_dir *dir)
{
  // pspdev/devkitPSP historically does not have a rewinddir implementation.
  // libctru (3DS) has rewinddir but it doesn't work.
#if defined(CONFIG_PSP) || defined(CONFIG_3DS)
  dir->path[PATH_BUF_LEN - 1] = 0;
  platform_closedir(dir);
  if(!platform_opendir(dir, dir->path))
    return false;
#elif defined(__WIN32__)
  if(dir->is_wdirent)
    _wrewinddir(dir->opaque);
  else
    rewinddir(dir->opaque);
#else
  rewinddir(dir->opaque);
#endif
  return true;
}

long dir_tell(struct mzx_dir *dir)
{
  return dir->pos;
}

boolean dir_open(struct mzx_dir *dir, const char *path)
{
  if(!platform_opendir(dir, path))
    return false;

  dir->entries = 0;
  while(platform_readdir(dir, NULL, 0, NULL))
    dir->entries++;

  if(!platform_rewinddir(dir))
    return false;

  dir->pos = 0;
  return true;
}

void dir_close(struct mzx_dir *dir)
{
  if(dir->opaque)
  {
    platform_closedir(dir);
    dir->opaque = NULL;
    dir->entries = 0;
    dir->pos = 0;
  }
}

void dir_seek(struct mzx_dir *dir, long offset)
{
  long i;

  if(!dir->opaque)
    return;

  dir->pos = CLAMP(offset, 0L, dir->entries);

  if(!platform_rewinddir(dir))
    return;

  for(i = 0; i < dir->pos; i++)
    platform_readdir(dir, NULL, 0, NULL);
}

boolean dir_get_next_entry(struct mzx_dir *dir, char *entry, int *type)
{
  if(!dir->opaque)
    return false;

  dir->pos = MIN(dir->pos + 1, dir->entries);

  return platform_readdir(dir, entry, PATH_BUF_LEN, type);
}
