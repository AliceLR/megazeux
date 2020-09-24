/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
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

#ifndef __IO_DIR_H
#define __IO_DIR_H

#include "../compat.h"

__M_BEGIN_DECLS

#include <stdio.h>

#define PATH_BUF_LEN MAX_PATH

// pspdev/devkitPSP historically does not have a rewinddir implementation.
// libctru (3DS) and libnx (Switch) have rewinddir but it doesn't work.
#if defined(CONFIG_3DS) || defined(CONFIG_PSP) || defined(CONFIG_SWITCH)
#define NO_REWINDDIR
#endif

enum mzx_dir_type
{
  DIR_TYPE_UNKNOWN,
  DIR_TYPE_FILE,
  DIR_TYPE_DIR
};

struct mzx_dir
{
#ifdef NO_REWINDDIR
  char path[PATH_BUF_LEN];
#endif
  void *opaque;
  long entries;
  long pos;
#ifdef __WIN32__
  boolean is_wdirent;
#endif
};

boolean dir_open(struct mzx_dir *dir, const char *path);
void dir_close(struct mzx_dir *dir);
void dir_seek(struct mzx_dir *dir, long offset);
long dir_tell(struct mzx_dir *dir);
boolean dir_get_next_entry(struct mzx_dir *dir, char *entry, int *type);

__M_END_DECLS

#endif /* __IO_DIR_H */
