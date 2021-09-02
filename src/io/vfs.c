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

#include <assert.h>
#include <errno.h>
#include <stdint.h>

#include "vfs.h"

#ifdef VIRTUAL_FILESYSTEM

#include "../platform.h" /* get_ticks */
#include "path.h"

#define VFS_DEFAULT_FILE_SIZE 32
#define VFS_DEFAULT_DIR_SIZE 4

#define VFS_NO_INODE 0
#define VFS_ROOT_INODE 1

#define VFS_IDX_SELF   0
#define VFS_IDX_PARENT 1

#define VFS_ERROR(e) (errno = e, VFS_NO_INODE)

#define VFS_INODE_TYPE(n) ((n)->flags & VFS_INODE_TYPEMASK)

enum vfs_inode_flags
{
  VFS_INODE_FILE       = (1<<0),
  VFS_INODE_DIR        = (1<<1),
  VFS_INODE_TYPEMASK   = VFS_INODE_FILE | VFS_INODE_DIR,
  VFS_INODE_IS_REAL    = (1<<2), // Cache of a real location in the filesystem.
  VFS_INODE_NAME_ALLOC = (1<<7),
};

union vfs_inode_contents
{
  void *has_contents;
  uint32_t *inodes;
  uint8_t *data;
};

/* `name` buffer is long enough to confortably fit null terminated 8.3 names. */
struct vfs_inode
{
  union vfs_inode_contents contents;
  uint32_t length;
  uint32_t length_alloc;
  uint32_t timestamp;
  uint8_t flags; // 0=deleted
  uint8_t refcount; // If 255, refuse creation of new refs.
  char name[14];
};

/* If `flags` & VFS_INODE_NAME_ALLOC, `name` is a pointer instead of a buffer.
 * `name` is still null terminated in this case. Alloc size is strlen(name)+1. */
struct vfs_inode_name_alloc
{
  union vfs_inode_contents contents;
  uint32_t length;
  uint32_t length_alloc;
  uint32_t timestamp;
  uint8_t flags; // 0=deleted
  uint8_t refcount; // If 255, refuse creation of new refs.
  char *name;
};

struct vfilesystem
{
  struct vfs_inode *table;
  uint32_t table_length;
  uint32_t table_alloc;
  uint32_t table_next;
  uint32_t current;
  uint32_t current_root;
};

static uint64_t vfs_get_timestamp(void)
{
  return get_ticks() / 1000;
}

/**
 * strsep but for paths.
 * TODO: move to path.c.
 */
static char *vfs_tokenize(char **next)
{
  char *src = *next;
  if(src)
  {
    char *current = strpbrk(src, "\\/");
    if(current)
    {
      *current = '\0';
      *next = current + 1;
    }
    else
      *next = NULL;
  }
  return src;
}

static int vfs_name_cmp(const char *a, const char *b)
{
#ifdef VIRTUAL_FILESYSTEM_CASE_INSENSITIVE
  return strcasecmp(a, b);
#else
  return strcmp(a, b);
#endif
}

static boolean vfs_inode_init_name(struct vfs_inode *n, const char *name)
{
  size_t name_len = strlen(name);
  if(name_len >= sizeof(n->name))
  {
    struct vfs_inode_name_alloc *_n = (struct vfs_inode_name_alloc *)n;
    char *inode_name = malloc(name_len + 1);
    if(!inode_name)
      return false;

    _n->flags |= VFS_INODE_NAME_ALLOC;
    _n->name = inode_name;
    memcpy(_n->name, name, name_len + 1);
  }
  else
    memcpy(n->name, name, name_len + 1);

  return true;
}

static boolean vfs_inode_rename(struct vfs_inode *n, const char *name)
{
  if(n->flags & VFS_INODE_NAME_ALLOC)
  {
    struct vfs_inode_name_alloc *_n = (struct vfs_inode_name_alloc *)n;
    char *prev_name = _n->name;

    if(!vfs_inode_init_name(n, name))
    {
      _n->name = prev_name;
      return false;
    }
    free(_n->name);
    return true;
  }
  return vfs_inode_init_name(n, name);
}

static inline const char *vfs_inode_name(struct vfs_inode *n)
{
  if(n->flags & VFS_INODE_NAME_ALLOC)
  {
    struct vfs_inode_name_alloc *_n = (struct vfs_inode_name_alloc *)n;
    return _n->name;
  }
  return n->name;
}

static boolean vfs_inode_init_file(struct vfs_inode *n, const char *name,
 size_t init_alloc, boolean is_real)
{
  if(init_alloc < VFS_DEFAULT_FILE_SIZE)
    init_alloc = VFS_DEFAULT_FILE_SIZE;

  n->contents.data = malloc(init_alloc);
  if(!n->contents.data)
    return false;

  n->length = 0;
  n->length_alloc = init_alloc;
  n->timestamp = vfs_get_timestamp();
  n->flags = VFS_INODE_FILE;
  if(is_real)
    n->flags |= VFS_INODE_IS_REAL;

  if(!vfs_inode_init_name(n, name))
  {
    free(n->contents.data);
    n->flags = 0;
    return false;
  }
  return true;
}

static boolean vfs_inode_init_directory(struct vfs_inode *n, const char *name,
 size_t init_alloc, boolean is_real)
{
  if(init_alloc < VFS_DEFAULT_DIR_SIZE)
    init_alloc = VFS_DEFAULT_DIR_SIZE;

  n->contents.inodes = malloc(init_alloc * sizeof(uint32_t));
  if(!n->contents.inodes)
    return false;

  n->length = 2;
  n->length_alloc = init_alloc;
  n->timestamp = vfs_get_timestamp();
  n->flags = VFS_INODE_DIR;
  if(is_real)
    n->flags |= VFS_INODE_IS_REAL;

  if(!vfs_inode_init_name(n, name))
  {
    free(n->contents.inodes);
    n->flags = 0;
    return false;
  }
  return true;
}

static void vfs_inode_clear(struct vfs_inode *n)
{
  if(n->flags)
  {
    struct vfs_inode_name_alloc *_n = (struct vfs_inode_name_alloc *)n;

    if(n->contents.has_contents)
    {
      free(n->contents.has_contents);
      n->contents.has_contents = NULL;
    }

    if(n->flags & VFS_INODE_NAME_ALLOC)
    {
      free(_n->name);
      _n->name = NULL;
    }

    n->flags = 0;
  }
}

static boolean vfs_inode_expand_directory(struct vfs_inode *n, size_t count)
{
  uint32_t *inodes;
  uint64_t new_length = n->length + count;
  uint64_t new_alloc = n->length_alloc;

  if(new_length > UINT32_MAX)
    return false;

  assert(VFS_INODE_TYPE(n) == VFS_INODE_DIR);

  if(n->length_alloc >= new_length)
    return true;

  while(new_length > new_alloc)
    new_alloc <<= 1;

  if(new_alloc > UINT32_MAX)
    return false;

  inodes = realloc(n->contents.inodes, new_alloc * 2 * sizeof(uint32_t));
  if(!inodes)
    return false;

  n->contents.inodes = inodes;
  n->length_alloc = new_alloc;
  return true;
}

/**
 * Initialize the given VFS. The initialized VFS will contain one root inode
 * corresponding to C: (Windows) or / (everything else).
 */
static boolean vfs_setup(vfilesystem *vfs)
{
  struct vfs_inode *n;

  memset(vfs, 0, sizeof(vfilesystem));

  vfs->table = calloc(4, sizeof(struct vfs_inode));
  if(!vfs->table)
    return false;

  vfs->table_length = 2;
  vfs->table_alloc = 4;
  vfs->table_next = 2;
  vfs->current = VFS_ROOT_INODE;
  vfs->current_root = VFS_ROOT_INODE;

  /* 0: list of roots. */
  n = &(vfs->table[VFS_NO_INODE]);
  n->contents.inodes = malloc(3 * sizeof(uint32_t));
  if(!n->contents.has_contents)
    return false;

  n->contents.inodes[VFS_IDX_SELF]   = VFS_NO_INODE;
  n->contents.inodes[VFS_IDX_PARENT] = VFS_NO_INODE;
  n->contents.inodes[2] = VFS_ROOT_INODE;
  n->length = 3;
  n->length_alloc = 3;
  n->timestamp = 0;
  n->flags = VFS_INODE_DIR;
  n->refcount = 255;
  n->name[0] = '\0';

  /* 1: / or C:\ */
  n = &(vfs->table[VFS_ROOT_INODE]);
  n->contents.inodes = malloc(4 * sizeof(uint32_t));
  if(!n->contents.has_contents)
    return false;

  n->contents.inodes[VFS_IDX_SELF]   = VFS_ROOT_INODE;
  n->contents.inodes[VFS_IDX_PARENT] = VFS_ROOT_INODE;
  n->length = 2;
  n->length_alloc = 4;
  n->timestamp = 0;
  n->flags = VFS_INODE_DIR | VFS_INODE_IS_REAL;
  n->refcount = 0;
#if defined(__WIN32__)
  strcpy(n->name, "C:\\");
#else
  strcpy(n->name, "/");
#endif

  return true;
}

/**
 * Release all allocated memory for a given VFS.
 */
static void vfs_clear(vfilesystem *vfs)
{
  size_t i;
  for(i = 0; i < vfs->table_length; i++)
    vfs_inode_clear(&(vfs->table[i]));

  free(vfs->table);
  vfs->table = NULL;
}

/**
 * Clear a given inode and update table_next.
 */
static void vfs_release_inode(vfilesystem *vfs, uint32_t inode)
{
  vfs_inode_clear(&(vfs->table[inode]));
  if(inode < vfs->table_next)
    vfs->table_next = inode;
}

/**
 * Get the next unused inode in the VFS. table_next will be advanced to the
 * position of the returned inode. This will allocate more inode space in the
 * VFS if needed.
 */
static uint32_t vfs_get_next_free_inode(vfilesystem *vfs)
{
  while(vfs->table_next < vfs->table_length)
  {
    if(!vfs->table[vfs->table_next].flags)
      return vfs->table_next;

    vfs->table_next++;
  }

  if(vfs->table_length >= vfs->table_alloc)
  {
    struct vfs_inode *ptr;
    if(!vfs->table_alloc)
      vfs->table_alloc = 4;

    vfs->table_alloc <<= 1;
    ptr = realloc(vfs->table, vfs->table_alloc * sizeof(struct vfs_inode));
    if(!ptr)
      return VFS_ERROR(ENOSPC);

    vfs->table = ptr;
  }

  vfs->table[vfs->table_length].flags = 0;
  return (vfs->table_length++);
}

/**
 * Returns the inode of a given name within parent. This name should not include
 * path separators. The value of `index` will be set to the index of the inode
 * if it exists, otherwise the index where that inode should be placed.
 */
static uint32_t vfs_get_inode_in_parent_by_name(vfilesystem *vfs,
 struct vfs_inode *parent, const char *name, uint32_t *index)
{
  uint32_t a = 2;
  uint32_t b = parent->length - 1;
  uint32_t current;
  uint32_t i;
  int cmp;

  assert(parent->length >= 2);

  while(a <= b)
  {
    current = (b - a) / 2 + a;
    i = parent->contents.inodes[current];

    cmp = vfs_name_cmp(name, vfs_inode_name(&(vfs->table[i])));
    if(cmp > 0)
    {
      a = current + 1;
    }
    else

    if(cmp < 0)
    {
      b = current - 1;
    }
    else
    {
      if(index)
        *index = current;
      return i;
    }
  }

  if(index)
    *index = a;
  return VFS_NO_INODE;
}

/**
 * Get the first inode for a given path string.
 * This is either the "working directory" inode or a root inode.
 */
static uint32_t vfs_get_path_base_inode(vfilesystem *vfs, const char **path)
{
  ssize_t len = path_is_absolute(*path);
  uint32_t inode;
  char buffer[32];

  if(len >= (ssize_t)sizeof(buffer))
    return VFS_ERROR(ENOENT);

  if(len > 0)
  {
    memcpy(buffer, *path, len);
    buffer[len] = '\0';
    *path += len;

    inode = vfs_get_inode_in_parent_by_name(vfs, &(vfs->table[0]), buffer, NULL);
    if(inode == VFS_NO_INODE)
    {
#if defined(__WIN32__)
      // Windows and DJGPP: / behaves as the root of the current active drive.
      if(!strcmp(buffer, "/") || !strcmp(buffer, "\\"))
        return vfs->current_root;
#endif

      return VFS_ERROR(ENOENT);
    }

    return inode;
  }
  return vfs->current;
}

/**
 * Find an inode in the VFS located at the given path.
 */
static uint32_t vfs_get_inode_by_path(vfilesystem *vfs, const char *path)
{
  struct vfs_inode *parent;
  char buffer[MAX_PATH];
  char *current;
  char *next;
  uint32_t inode;

  inode = vfs_get_path_base_inode(vfs, &path);
  if(!inode)
    return VFS_NO_INODE;

  snprintf(buffer, sizeof(buffer), "%s", path);

  current = buffer;
  while((current = vfs_tokenize(&next)))
  {
    parent = &(vfs->table[inode]);
    if(VFS_INODE_TYPE(parent) != VFS_INODE_DIR)
      return VFS_ERROR(ENOTDIR);

    if(current[0] == '.')
    {
      if(current[1] == '.' && current[2] == '\0')
      {
        inode = parent->contents.inodes[VFS_IDX_PARENT];
        continue;
      }
      else

      if(current[1] == '\0')
        continue;
    }

    inode = vfs_get_inode_in_parent_by_name(vfs, parent, current, NULL);
    if(!inode)
      return VFS_ERROR(ENOENT);
  }

  return inode;
}

/**
 * Generate an inode in the VFS with a given name. This name should not include
 * path separators.
 */
static uint32_t vfs_make_inode(vfilesystem *vfs, struct vfs_inode *parent,
 const char *name, size_t init_alloc, int flags)
{
  struct vfs_inode *n;
  uint32_t *inodes;
  uint32_t pos;
  uint32_t pos_in_parent;

  assert(VFS_INODE_TYPE(parent) == VFS_INODE_DIR);
  assert(flags & VFS_INODE_TYPEMASK);

  pos = vfs_get_inode_in_parent_by_name(vfs, parent, name, &pos_in_parent);
  if(pos != VFS_NO_INODE)
    return VFS_ERROR(EEXIST);

  pos = vfs_get_next_free_inode(vfs);
  if(!pos)
    return VFS_NO_INODE;

  if(!vfs_inode_expand_directory(parent, 1))
    return VFS_ERROR(ENOSPC);

  n = &(vfs->table[pos]);

  if((flags & VFS_INODE_TYPEMASK) == VFS_INODE_DIR)
  {
    if(!vfs_inode_init_directory(n, name, init_alloc, !!(flags & VFS_INODE_IS_REAL)))
      return VFS_ERROR(ENOSPC);
  }
  else
  {
    if(!vfs_inode_init_file(n, name, init_alloc, !!(flags & VFS_INODE_IS_REAL)))
      return VFS_ERROR(ENOSPC);
  }

  inodes = parent->contents.inodes;
  if(pos_in_parent < parent->length)
    memmove(inodes + pos_in_parent + 1, inodes + pos_in_parent, parent->length - pos_in_parent);
  inodes[pos_in_parent] = pos;
  parent->length++;

  return pos;
}

/**
 * Generate an inode in the VFS at the given path, generating parent directory
 * inodes as-required.
 */
static uint32_t vfs_make_inode_recursively(vfilesystem *vfs, const char *path,
 boolean is_real)
{
  /*
  int flags = is_real ? VFS_INODE_IS_REAL : 0;
  char buffer[MAX_PATH];
  char *current;
  char *next;
  */
  uint32_t inode;

  inode = vfs_get_path_base_inode(vfs, &path);
  if(!inode)
    return VFS_NO_INODE;

// FIXME
/*
  if((parent->flags & VFS_INODE_TYPE) != VFS_INODE_DIR)
    return VFS_ERROR(ENOTDIR);
*/

  // FIXME
  return VFS_NO_INODE;
}

#endif /* VIRTUAL_FILESYSTEM */

vfilesystem *vfs_init(void)
{
#ifdef VIRTUAL_FILESYSTEM
  vfilesystem *vfs = malloc(sizeof(vfilesystem));
  vfs_setup(vfs);
  return vfs;
#endif
}

void vfs_free(vfilesystem *vfs)
{
#ifdef VIRTUAL_FILESYSTEM
  vfs_clear(vfs);
  free(vfs);
#endif
}
