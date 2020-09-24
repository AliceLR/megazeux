/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
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

/**
 * MZX dirent wrapper with several platform-specific hacks.
 *
 * These are a bit more involved than the stdio wrappers and require fields in
 * the mzx_dir struct, so the platform hacks are here instead of in the vfile
 * platform function headers.
 */

#include <dirent.h>

#include "dir.h"
#include "../util.h"

#ifdef __WIN32__
// utf8_to_utf16, utf16_to_utf8
#include "vfile_win32.h"
#endif

static inline boolean platform_opendir(struct mzx_dir *dir, const char *path)
{
#ifdef WIDE_PATHS
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

#ifdef NO_REWINDDIR
  if(dir->path != path)
  {
    snprintf(dir->path, PATH_BUF_LEN, "%s", path);
    dir->path[PATH_BUF_LEN - 1] = 0;
  }
#endif

  dir->opaque = opendir(path);
  return !!dir->opaque;
}

static inline void platform_closedir(struct mzx_dir *dir)
{
#ifdef WIDE_PATHS
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

#ifdef WIDE_PATHS
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
#ifdef NO_REWINDDIR
  platform_closedir(dir);
  if(!platform_opendir(dir, dir->path))
    return false;
#elif defined(WIDE_PATHS)
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
