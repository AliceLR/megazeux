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

/**
 * MZX virtual filesystem. This is intended mainly as a support layer
 * to be used internally by vio.c, but can also be used by itself.
 */

#ifndef __IO_VFS_H
#define __IO_VFS_H

#include "../compat.h"

__M_BEGIN_DECLS

#include <stdint.h>
#include "vfile.h"

#ifndef CONFIG_NDS
#define VIRTUAL_FILESYSTEM
#endif

#if defined(__WIN32__) || defined(__APPLE__)
#define VIRTUAL_FILESYSTEM_CASE_INSENSITIVE
#endif

#if defined(__WIN32__) || defined(CONFIG_DJGPP)
#define VIRTUAL_FILESYSTEM_DOS_DRIVE
#endif

UTILS_LIBSPEC vfilesystem *vfs_init(void);
UTILS_LIBSPEC void vfs_free(vfilesystem *vfs);

UTILS_LIBSPEC void vfs_reset(vfilesystem *vfs);
UTILS_LIBSPEC boolean vfs_cache_at_path(vfilesystem *vfs, const char *path);
UTILS_LIBSPEC boolean vfs_invalidate_at_path(vfilesystem *vfs, const char *path);
UTILS_LIBSPEC boolean vfs_create_file_at_path(vfilesystem *vfs, const char *path);
UTILS_LIBSPEC boolean vfs_sync_file(vfilesystem *vfs, uint32_t inode,
 void ***data, size_t **data_length, size_t **data_alloc);

UTILS_LIBSPEC uint32_t vfs_open_if_exists(vfilesystem *vfs,
 const char *path, boolean is_write);
UTILS_LIBSPEC uint32_t vfs_open_if_exists_or_cacheable(vfilesystem *vfs,
 const char *path, boolean is_write);
UTILS_LIBSPEC void vfs_close(vfilesystem *vfs, uint32_t inode);
UTILS_LIBSPEC int vfs_mkdir(vfilesystem *vfs, const char *path, int mode);
UTILS_LIBSPEC int vfs_rename(vfilesystem *vfs, const char *oldpath, const char *newpath);
UTILS_LIBSPEC int vfs_unlink(vfilesystem *vfs, const char *path);
UTILS_LIBSPEC int vfs_rmdir(vfilesystem *vfs, const char *path);
UTILS_LIBSPEC int vfs_access(vfilesystem *vfs, const char *path, int mode);
UTILS_LIBSPEC int vfs_stat(vfilesystem *vfs, const char *path, struct stat *st);
UTILS_LIBSPEC int vfs_error(vfilesystem *vfs);

__M_END_DECLS

#endif /* __IO_VFS_H */
