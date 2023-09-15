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
#include <stdlib.h>
#include "vfile.h"

#if defined(CONFIG_VFS) && !defined(NO_VIRTUAL_FILESYSTEM)
#define VIRTUAL_FILESYSTEM
#endif

#if defined(_WIN32) || defined(CONFIG_DJGPP) || defined(__APPLE__)
#define VIRTUAL_FILESYSTEM_CASE_INSENSITIVE
#endif

#if defined(_WIN32) || defined(CONFIG_DJGPP)
#define VIRTUAL_FILESYSTEM_DOS_DRIVE
#endif

#if !defined(CONFIG_DJGPP)
#define VIRTUAL_FILESYSTEM_PARALLEL
#endif

/* Unknown internal error. */
#define VFS_ERR_UNKNOWN   65536
/* An operation failed because it does not work on cached files. */
#define VFS_ERR_IS_CACHED 65537

struct vfs_dir_file
{
  char type;
  char name[1];
};

struct vfs_dir
{
  struct vfs_dir_file **files;
  size_t num_files;
};

UTILS_LIBSPEC vfilesystem *vfs_init(void);
UTILS_LIBSPEC void vfs_free(vfilesystem *vfs);

#ifdef VIRTUAL_FILESYSTEM

UTILS_LIBSPEC int vfs_make_root(vfilesystem *vfs, const char *name);
UTILS_LIBSPEC int vfs_create_file_at_path(vfilesystem *vfs, const char *path);

UTILS_LIBSPEC int vfs_open_if_exists(vfilesystem *vfs,
 const char *path, boolean is_write, uint32_t *inode);
UTILS_LIBSPEC int vfs_close(vfilesystem *vfs, uint32_t inode);
UTILS_LIBSPEC int vfs_truncate(vfilesystem *vfs, uint32_t inode);
UTILS_LIBSPEC ssize_t vfs_filelength(vfilesystem *vfs, uint32_t inode);
UTILS_LIBSPEC int vfs_lock_file_read(vfilesystem *vfs, uint32_t inode,
 const unsigned char **data, size_t *data_length);
UTILS_LIBSPEC int vfs_unlock_file_read(vfilesystem *vfs, uint32_t inode);
UTILS_LIBSPEC int vfs_lock_file_write(vfilesystem *vfs, uint32_t inode,
 unsigned char ***data, size_t **data_length, size_t **data_alloc);
UTILS_LIBSPEC int vfs_unlock_file_write(vfilesystem *vfs, uint32_t inode);

UTILS_LIBSPEC int vfs_chdir(vfilesystem *vfs, const char *path);
UTILS_LIBSPEC int vfs_getcwd(vfilesystem *vfs, char *dest, size_t dest_len);
UTILS_LIBSPEC int vfs_mkdir(vfilesystem *vfs, const char *path, int mode);
UTILS_LIBSPEC int vfs_rename(vfilesystem *vfs, const char *oldpath, const char *newpath);
UTILS_LIBSPEC int vfs_unlink(vfilesystem *vfs, const char *path);
UTILS_LIBSPEC int vfs_rmdir(vfilesystem *vfs, const char *path);
UTILS_LIBSPEC int vfs_access(vfilesystem *vfs, const char *path, int mode);
UTILS_LIBSPEC int vfs_stat(vfilesystem *vfs, const char *path, struct stat *st);

UTILS_LIBSPEC int vfs_readdir(vfilesystem *vfs, const char *path, struct vfs_dir *d);
UTILS_LIBSPEC int vfs_readdir_free(struct vfs_dir *d);

UTILS_LIBSPEC int vfs_invalidate_at_path(vfilesystem *vfs, const char *path);
UTILS_LIBSPEC int vfs_invalidate_at_least(vfilesystem *vfs, size_t *amount_to_free);
UTILS_LIBSPEC int vfs_invalidate_all(vfilesystem *vfs);
UTILS_LIBSPEC int vfs_cache_directory(vfilesystem *vfs, const char *path,
 const struct stat *st);
UTILS_LIBSPEC int vfs_cache_file(vfilesystem *vfs, const char *path,
 const void *data, size_t data_length);
UTILS_LIBSPEC int vfs_cache_file_callback(vfilesystem *vfs, const char *path,
 size_t (*readfn)(void * RESTRICT, size_t, void * RESTRICT),
 void *priv, size_t data_length);
UTILS_LIBSPEC size_t vfs_get_cache_total_size(vfilesystem *vfs);
UTILS_LIBSPEC size_t vfs_get_total_memory_usage(vfilesystem *vfs);
UTILS_LIBSPEC void vfs_set_timestamps_enabled(vfilesystem *vfs, boolean enable);

#else /* !VIRTUAL_FILESYSTEM */

static inline int vfs_make_root(vfilesystem *v, const char *n) { return -1; }
static inline int vfs_create_file_at_path(vfilesystem *v, const char *p) { return -1; }

static inline int vfs_open_if_exists(vfilesystem *v,
 const char *p, boolean w, uint32_t *i) { return -1; }
static inline int vfs_close(vfilesystem *v, uint32_t i) { return -1; }
static inline int vfs_truncate(vfilesystem *v, uint32_t i) { return -1; }
static inline ssize_t vfs_filelength(vfilesystem *v, uint32_t i) { return -1; }
static inline int vfs_lock_file_read(vfilesystem *v, uint32_t i,
 const unsigned char **d, size_t *l) { return -1; }
static inline int vfs_unlock_file_read(vfilesystem *v, uint32_t i) { return -1; }
static inline int vfs_lock_file_write(vfilesystem * v, uint32_t i,
 unsigned char ***d, size_t **l, size_t **a) { return -1; }
static inline int vfs_unlock_file_write(vfilesystem *v, uint32_t i) { return -1; }

static inline int vfs_chdir(vfilesystem *v, const char *p) { return -1; }
static inline int vfs_getcwd(vfilesystem *v, char *d, size_t l) { return -1; }
static inline int vfs_mkdir(vfilesystem *v, const char *p, int m) { return -1; }
static inline int vfs_rename(vfilesystem *v, const char *o, const char *n) { return -1; }
static inline int vfs_unlink(vfilesystem *v, const char *p) { return -1; }
static inline int vfs_rmdir(vfilesystem *v, const char *p) { return -1; }
static inline int vfs_access(vfilesystem *v, const char *p, int m) { return -1; }
static inline int vfs_stat(vfilesystem *v, const char *p, struct stat *s) { return -1; }

static inline int vfs_readdir(vfilesystem *v, const char *p, struct vfs_dir *d) { return -1; }
static inline int vfs_readdir_free(struct vfs_dir *d) { return -1; }

static inline int vfs_invalidate_at_path(vfilesystem *v, const char *p) { return -1; }
static inline int vfs_invalidate_at_least(vfilesystem *v, size_t *a) { return -1; }
static inline int vfs_invalidate_all(vfilesystem *v) { return -1; }
static inline int vfs_cache_directory(vfilesystem *v, const char *p,
 const struct stat *st) { return -1; }
static inline int vfs_cache_file(vfilesystem *v, const char *p,
 const void *d, size_t l) { return -1; }
static inline int vfs_cache_file_callback(vfilesystem *v, const char *p,
 size_t (*r)(void * RESTRICT, size_t, void * RESTRICT),
 void *pr, size_t l) { return -1; }
static inline size_t vfs_get_cache_total_size(vfilesystem *v) { return 0; }
static inline size_t vfs_get_total_memory_usage(vfilesystem *vfs) { return 0; }
static inline void vfs_set_timestamps_enabled(vfilesystem *v, boolean e) { }

#endif /* !VIRTUAL_FILESYSTEM */

__M_END_DECLS

#endif /* __IO_VFS_H */
