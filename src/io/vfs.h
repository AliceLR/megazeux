/* MegaZeux
 *
 * Copyright (C) 2021 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __IO_VFS_H
#define __IO_VFS_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "vfile.h"

#ifndef CONFIG_NDS
#define VIRTUAL_FILESYSTEM
#endif

#if defined(__WIN32__) || defined(__APPLE__)
#define VIRTUAL_FILESYSTEM_CASE_INSENSITIVE
#endif

UTILS_LIBSPEC vfilesystem *vfs_init(void);
UTILS_LIBSPEC void vfs_free(vfilesystem *vfs);

// TODO other vfs external API junk.

/**
 * Internal functions for vio.c.
 */

// FIXME

__M_END_DECLS

#endif /* __IO_VFS_H */
