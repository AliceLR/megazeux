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
#include <time.h>
#include <sys/stat.h>

#include "vfs.h"

#ifdef VIRTUAL_FILESYSTEM

#include "../platform.h" /* get_ticks */
#include "../util.h"
#include "path.h"

// Maximum number of seconds a cached file can be considered valid.
#define VFS_INVALIDATE_S (5*60)
// Force invalidation of a cached file.
#define VFS_INVALIDATE_FORCE UINT32_MAX
// Maximum reference count for a given VFS inode.
#define VFS_MAX_REFCOUNT 255

#define VFS_DEFAULT_FILE_SIZE 32
#define VFS_DEFAULT_DIR_SIZE 4

#define VFS_UNKNOWN_ERROR 255

#define VFS_NO_INODE 0
#define VFS_ROOT_INODE 1

#define VFS_IDX_SELF   0
#define VFS_IDX_PARENT 1

#define VFS_INODE_TYPE(n) ((n)->flags & VFS_INODE_TYPEMASK)
#define VFS_IS_CACHED(n) ((n)->timestamp != 0)

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
  unsigned char *data;
};

/* `name` buffer is long enough to confortably fit null terminated 8.3 names. */
struct vfs_inode
{
  union vfs_inode_contents contents;
  size_t length;
  size_t length_alloc;
  uint32_t timestamp;
  uint64_t create_time;
  uint64_t modify_time;
  uint8_t flags; // 0=deleted
  uint8_t refcount; // If 255, refuse creation of new refs.
  char name[14];
};

/* If `flags` & VFS_INODE_NAME_ALLOC, `name` is a pointer instead of a buffer.
 * `name` is still null terminated in this case. Alloc size is strlen(name)+1. */
struct vfs_inode_name_alloc
{
  union vfs_inode_contents contents;
  size_t length;
  size_t length_alloc;
  uint32_t timestamp;
  uint64_t create_time;
  uint64_t modify_time;
  uint8_t flags; // 0=deleted
  uint8_t refcount; // If 255, refuse creation of new refs.
  char *name;
};

struct vfilesystem
{
  struct vfs_inode **table;
  uint32_t table_length;
  uint32_t table_alloc;
  uint32_t table_next;
  uint32_t current;
  uint32_t current_root;
#ifdef VIRTUAL_FILESYSTEM_PARALLEL
  platform_mutex lock;
  platform_cond cond;
  int num_writers;
  int num_readers;
#endif
  boolean is_writer;
  int error;
};

static uint32_t vfs_get_timestamp(void)
{
  uint32_t timestamp = (uint32_t)(get_ticks() / 1000);
  return timestamp ? timestamp : 1;
}

/* FIXME
static uint64_t vfs_get_date(void)
{
  return time(NULL);
}
*/

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

  n->contents.data = (unsigned char *)malloc(init_alloc);
  if(!n->contents.data)
    return false;

  n->length = 0;
  n->length_alloc = init_alloc;
  n->timestamp = is_real ? vfs_get_timestamp() : 0;
  n->create_time = 0;
  n->modify_time = 0;
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
  n->timestamp = is_real ? vfs_get_timestamp() : 0;
  n->create_time = 0;
  n->modify_time = 0;
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

static int vfs_seterror(vfilesystem *vfs, int e)
{
  vfs->error = e;
  return VFS_NO_INODE;
}

static int vfs_geterror(vfilesystem *vfs)
{
  int error = vfs->error;
  vfs->error = VFS_UNKNOWN_ERROR;
  return error;
}

static boolean vfs_init_lock(vfilesystem *vfs)
{
#ifdef VIRTUAL_FILESYSTEM_PARALLEL
  platform_mutex_init(&(vfs->lock));
  platform_cond_init(&(vfs->cond));
  vfs->num_readers = 0;
  vfs->num_writers = 0;
  vfs->is_writer = false;
  return true;
#endif
  vfs->is_writer = true;
  return true;
}

static boolean vfs_clear_lock(vfilesystem *vfs)
{
#ifdef VIRTUAL_FILESYSTEM_PARALLEL
  platform_mutex_destroy(&(vfs->lock));
  platform_cond_destroy(&(vfs->cond));
#endif
  return true;
}

static boolean vfs_read_lock(vfilesystem *vfs)
{
#ifdef VIRTUAL_FILESYSTEM_PARALLEL
  if(!platform_mutex_lock(&(vfs->lock)))
    return false;

  while(vfs->num_writers || vfs->is_writer)
    platform_cond_wait(&(vfs->cond), &(vfs->lock));

  vfs->num_readers++;

  platform_mutex_unlock(&(vfs->lock));
#endif
  return true;
}

static boolean vfs_read_unlock(vfilesystem *vfs)
{
#ifdef VIRTUAL_FILESYSTEM_PARALLEL
  if(!platform_mutex_lock(&(vfs->lock)))
    return false;

  vfs->num_readers--;
  if(!vfs->num_readers)
    platform_cond_broadcast(&(vfs->cond));

  platform_mutex_unlock(&(vfs->lock));
#endif
  return true;
}

static boolean vfs_write_lock(vfilesystem *vfs)
{
#ifdef VIRTUAL_FILESYSTEM_PARALLEL
  if(!platform_mutex_lock(&(vfs->lock)))
    return false;

  vfs->num_writers++;

  while(vfs->num_readers)
    platform_cond_wait(&(vfs->cond), &(vfs->lock));

  vfs->num_writers--;
  vfs->is_writer = true;

  platform_mutex_unlock(&(vfs->lock));
#endif
  return true;
}

static boolean vfs_write_unlock(vfilesystem *vfs)
{
#ifdef VIRTUAL_FILESYSTEM_PARALLEL
  if(!platform_mutex_lock(&(vfs->lock)))
    return false;

  vfs->is_writer = false;
  platform_cond_broadcast(&(vfs->cond));

  platform_mutex_unlock(&(vfs->lock));
#endif
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

  vfs->table = calloc(4, sizeof(struct vfs_inode *));
  if(!vfs->table)
    return false;

  vfs->table[VFS_NO_INODE] = (struct vfs_inode *)calloc(1, sizeof(struct vfs_inode));
  vfs->table[VFS_ROOT_INODE] = (struct vfs_inode *)calloc(1, sizeof(struct vfs_inode));
  if(!vfs->table[VFS_NO_INODE] || !vfs->table[VFS_ROOT_INODE])
    return false;

  if(!vfs_init_lock(vfs))
    return false;

  vfs->table_length = 2;
  vfs->table_alloc = 4;
  vfs->table_next = 2;
  vfs->current = VFS_ROOT_INODE;
  vfs->current_root = VFS_ROOT_INODE;
  vfs_seterror(vfs, VFS_UNKNOWN_ERROR);

  /* 0: list of roots. */
  n = vfs->table[VFS_NO_INODE];
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
  n = vfs->table[VFS_ROOT_INODE];
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
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
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
  {
    if(vfs->table[i])
    {
      vfs_inode_clear(vfs->table[i]);
      free(vfs->table[i]);
    }
  }

  vfs_clear_lock(vfs);

  free(vfs->table);
  vfs->table = NULL;
}

/**
 * Allocate a given inode.
 */
static struct vfs_inode *vfs_allocate_inode(vfilesystem *vfs, uint32_t inode)
{
  vfs->table[inode] = (struct vfs_inode *)malloc(sizeof(struct vfs_inode));
  if(vfs->table[inode])
    vfs->table[inode]->flags = 0;

  return vfs->table[inode];
}

/**
 * Clear a given inode and update table_next.
 */
static void vfs_release_inode(vfilesystem *vfs, uint32_t inode)
{
  vfs_inode_clear(vfs->table[inode]);
  if(inode < vfs->table_next)
    vfs->table_next = inode;
}

/**
 * Get a pointer to an inode data structure from its index.
 * This will return a pointer even for cleared inodes.
 */
static struct vfs_inode *vfs_get_inode_ptr(vfilesystem *vfs, uint32_t inode)
{
  assert(inode < vfs->table_length);
  return vfs->table[inode];
}

/**
 * Get a pointer to an inode data structure from its index.
 * If the timestamp indicates it is stale, return NULL.
 *
 * If the VFS is under writer lock and no references are left to the stale
 * file, immediately attempt to delete it.
 */
static struct vfs_inode *vfs_get_inode_ptr_invalidate(vfilesystem *vfs, uint32_t inode)
{
  struct vfs_inode *n = vfs_get_inode_ptr(vfs, inode);
  uint32_t current_time = vfs_get_timestamp();

  if(VFS_IS_CACHED(n))
  {
    if(n->timestamp == VFS_INVALIDATE_FORCE ||
     (current_time - n->timestamp > VFS_INVALIDATE_S))
    {
      // If there are no references and this is writer locked, attempt to delete now.
      if(!n->refcount && vfs->is_writer)
      {
        // FIXME directories: don't release if this (or a child) is the current dir.
        // FIXME directories: recursively check children for references too.
        // FIXME anything: remove from parent.
        vfs_release_inode(vfs, inode);
        return NULL;
      }
      // Otherwise, just flag this for immediate deletion later.
      n->timestamp = VFS_INVALIDATE_FORCE;
    }
  }
  return n;
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
    if(!vfs->table[vfs->table_next])
    {
      if(!vfs_allocate_inode(vfs, vfs->table_next))
        return vfs_seterror(vfs, ENOSPC);

      return vfs->table_next;
    }

    if(!vfs->table[vfs->table_next]->flags)
      return vfs->table_next;

    vfs->table_next++;
  }

  if(vfs->table_length >= vfs->table_alloc)
  {
    struct vfs_inode **ptr;
    if(!vfs->table_alloc)
      vfs->table_alloc = 4;

    vfs->table_alloc <<= 1;
    ptr = realloc(vfs->table, vfs->table_alloc * sizeof(struct vfs_inode *));
    if(!ptr)
      return vfs_seterror(vfs, ENOSPC);

    vfs->table = ptr;
  }

  if(!vfs_allocate_inode(vfs, vfs->table_length))
    return vfs_seterror(vfs, ENOSPC);

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
  uint32_t inode;
  int cmp;

  assert(parent->length >= 2);

  if(!name[0])
    return VFS_NO_INODE;

  /* Special case--current and parent dir. */
  if(name[0] == '.')
  {
    if(name[1] == '.' && name[2] == '\0')
    {
      if(index)
        *index = VFS_IDX_PARENT;
      return parent->contents.inodes[VFS_IDX_PARENT];
    }
    else

    if(name[1] == '\0')
    {
      if(index)
        *index = VFS_IDX_SELF;
      return parent->contents.inodes[VFS_IDX_SELF];
    }
  }

  while(a <= b)
  {
    current = (b - a) / 2 + a;
    inode = parent->contents.inodes[current];

    cmp = vfs_name_cmp(name, vfs_inode_name(vfs_get_inode_ptr(vfs, inode)));
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
      return inode;
    }
  }

  if(index)
    *index = a;
  return VFS_NO_INODE;
}

/**
 * Get the first inode for a given path string.
 * This is either the "working directory" inode or a root inode.
 * The string pointed to by `path` will be advanced past the base token if it
 * exists.
 */
static uint32_t vfs_get_path_base_inode(vfilesystem *vfs, const char **path)
{
  ssize_t len = path_is_absolute(*path);
  uint32_t inode;
  char buffer[32];

  if(!*path[0])
    return vfs_seterror(vfs, ENOENT);

  if(len >= (ssize_t)sizeof(buffer))
    return vfs_seterror(vfs, ENOENT);

  if(len > 0)
  {
    struct vfs_inode *roots = vfs_get_inode_ptr(vfs, 0);
    memcpy(buffer, *path, len);
    buffer[len] = '\0';
    *path += len;

    path_clean_slashes(buffer, sizeof(buffer));

    inode = vfs_get_inode_in_parent_by_name(vfs, roots, buffer, NULL);
    if(inode == VFS_NO_INODE)
    {
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
      // Windows and DJGPP: / behaves as the root of the current active drive.
      if(!strcmp(buffer, DIR_SEPARATOR))
        return vfs->current_root;
#endif

      return vfs_seterror(vfs, ENOENT);
    }

    return inode;
  }
  return vfs->current;
}

/**
 * Find an inode in the VFS located at the given relative path within a known
 * base inode. `relative_path` should be cleaned for slashes prior to calling
 * this function and should be writeable.
 */
static uint32_t vfs_get_inode_by_relative_path(vfilesystem *vfs, uint32_t inode,
 char *relative_path)
{
  struct vfs_inode *parent;
  char *current;
  char *next;

  next = relative_path;
  while((current = vfs_tokenize(&next)))
  {
    if(!current[0])
      continue;

    parent = vfs_get_inode_ptr(vfs, inode);
    if(VFS_INODE_TYPE(parent) != VFS_INODE_DIR)
      return vfs_seterror(vfs, ENOTDIR);

    inode = vfs_get_inode_in_parent_by_name(vfs, parent, current, NULL);
    if(!inode)
      return vfs_seterror(vfs, ENOENT);
  }

  return inode;
}

/**
 * Find an inode in the VFS located at the given path.
 */
static uint32_t vfs_get_inode_by_path(vfilesystem *vfs, const char *path)
{
  char buffer[MAX_PATH];
  uint32_t inode;

  inode = vfs_get_path_base_inode(vfs, &path);
  if(!inode)
    return VFS_NO_INODE;

  path_clean_slashes_copy(buffer, sizeof(buffer), path);

  return vfs_get_inode_by_relative_path(vfs, inode, buffer);
}

/**
 * Find an inode and its parent inode in the VFS from a given path, if either
 * exists. The name of the child inode will be copied to the provided buffer.
 */
static boolean vfs_get_inode_and_parent_by_path(vfilesystem *vfs, const char *path,
 uint32_t *_parent, uint32_t *_inode, char *name, size_t name_length)
{
  struct vfs_inode *p;
  char buffer[MAX_PATH];
  char *child;
  uint32_t inode = VFS_NO_INODE;
  uint32_t parent = VFS_NO_INODE;
  uint32_t base;

  base = vfs_get_path_base_inode(vfs, &path);
  if(!base)
    return false;

  path_clean_slashes_copy(buffer, sizeof(buffer), path);

  // If the buffer is empty, the entire path is a root.
  // The target inode is the root and its parent is itself in this situation.
  if(!buffer[0])
  {
    child = buffer;
    parent = base;
    inode = base;
  }
  else
  {
    child = strrchr(buffer, DIR_SEPARATOR_CHAR);
    if(child)
    {
      *(child++) = '\0';
      parent = vfs_get_inode_by_relative_path(vfs, base, buffer);
    }
    else
    {
      child = buffer;
      parent = base;
    }

    if(parent)
    {
      p = vfs_get_inode_ptr(vfs, parent);
      if(VFS_INODE_TYPE(p) != VFS_INODE_DIR)
      {
        vfs_seterror(vfs, ENOTDIR);
        return false;
      }
      inode = vfs_get_inode_in_parent_by_name(vfs, p, child, NULL);
    }
  }

  if(_parent)
    *_parent = parent;
  if(_inode)
    *_inode = inode;
  if(name)
    snprintf(name, name_length, "%s", child);

  return true;
}

/**
 * Generate an inode in the VFS with a given name. This name should not include
 * path separators.
 */
static uint32_t vfs_make_inode(vfilesystem *vfs, uint32_t parent,
 const char *name, size_t init_alloc, int flags)
{
  struct vfs_inode *p;
  struct vfs_inode *n;
  uint32_t *inodes;
  uint32_t pos;
  uint32_t pos_in_parent;

  p = vfs_get_inode_ptr(vfs, parent);

  assert(VFS_INODE_TYPE(p) == VFS_INODE_DIR);
  assert(flags & VFS_INODE_TYPEMASK);

  if(!name[0])
    return vfs_seterror(vfs, ENOENT);

  pos = vfs_get_inode_in_parent_by_name(vfs, p, name, &pos_in_parent);
  if(pos != VFS_NO_INODE)
    return vfs_seterror(vfs, EEXIST);

  pos = vfs_get_next_free_inode(vfs);
  if(!pos)
    return VFS_NO_INODE;

  if(!vfs_inode_expand_directory(p, 1))
    return vfs_seterror(vfs, ENOSPC);

  n = vfs_get_inode_ptr(vfs, pos);

  if((flags & VFS_INODE_TYPEMASK) == VFS_INODE_DIR)
  {
    if(!vfs_inode_init_directory(n, name, init_alloc, !!(flags & VFS_INODE_IS_REAL)))
      return vfs_seterror(vfs, ENOSPC);

    // Init self and parent inodes.
    n->contents.inodes[VFS_IDX_SELF] = pos;
    n->contents.inodes[VFS_IDX_PARENT] = p->contents.inodes[VFS_IDX_SELF];
  }
  else
  {
    if(!vfs_inode_init_file(n, name, init_alloc, !!(flags & VFS_INODE_IS_REAL)))
      return vfs_seterror(vfs, ENOSPC);
  }

  inodes = p->contents.inodes;
  if(pos_in_parent < p->length)
  {
    memmove(inodes + pos_in_parent + 1, inodes + pos_in_parent,
     sizeof(uint32_t) * (p->length - pos_in_parent));
  }
  inodes[pos_in_parent] = pos;
  p->length++;

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
    return vfs_seterror(vfs, ENOTDIR);
*/

  // FIXME
  return VFS_NO_INODE;
}

#ifdef DEBUG
static void _vfs_print_str(vfilesystem *vfs, const char *str, int level)
{
  fprintf(mzxerr, "%*.*s%s\n", level*2, level*2, "", str);
  fflush(mzxerr);
}

static void _vfs_print_len(vfilesystem *vfs, size_t len, int level)
{
  fprintf(mzxerr, "%*.*slen: %zu\n", level*2, level*2, "", len);
  fflush(mzxerr);
}

static void _vfs_print_name(vfilesystem *vfs, struct vfs_inode *n, int level)
{
  _vfs_print_str(vfs, vfs_inode_name(n), level);
}

static void _vfs_print_dir(vfilesystem *vfs, struct vfs_inode *dir, int level)
{
  size_t i;
  assert(VFS_INODE_TYPE(dir) == VFS_INODE_DIR);

  level++;
  if(level > 0)
    _vfs_print_len(vfs, dir->length, level);

  for(i = 0; i < dir->length; i++)
  {
    uint32_t inode = dir->contents.inodes[i];
    struct vfs_inode *n = vfs_get_inode_ptr(vfs, inode);

    if(i == VFS_IDX_SELF)
    {
      assert(n == dir);
      if(level > 0)
        _vfs_print_str(vfs, ".", level);
    }
    else

    if(i == VFS_IDX_PARENT)
    {
      if(level > 0)
        _vfs_print_str(vfs, "..", level);
    }
    else

    if(VFS_INODE_TYPE(n) == VFS_INODE_DIR)
    {
      _vfs_print_name(vfs, n, level);
      _vfs_print_str(vfs, "{", level);
      _vfs_print_dir(vfs, n, level);
      _vfs_print_str(vfs, "}", level);
    }
    else
      _vfs_print_name(vfs, n, level);
  }
}

static inline void vfs_print(vfilesystem *vfs)
{
  struct vfs_inode *roots = vfs_get_inode_ptr(vfs, 0);
  _vfs_print_dir(vfs, roots, -1);
}
#endif /* DEBUG */

#endif /* VIRTUAL_FILESYSTEM */

/**
 * Allocate and initialize a VFS.
 * (This function is enabled even when VIRTUAL_FILESYSTEM is not defined so
 * -pedantic doesn't complain about ISO C not allowing empty compilation units.)
 */
vfilesystem *vfs_init(void)
{
#ifdef VIRTUAL_FILESYSTEM
  vfilesystem *vfs = (vfilesystem *)malloc(sizeof(vfilesystem));
  vfs_setup(vfs);
  return vfs;
#endif
  return NULL;
}

void vfs_free(vfilesystem *vfs)
{
#ifdef VIRTUAL_FILESYSTEM
  vfs_clear(vfs);
  free(vfs);
#endif
}

#ifdef VIRTUAL_FILESYSTEM

void vfs_reset(vfilesystem *vfs)
{
  vfs_clear(vfs);
}

int vfs_cache_at_path(vfilesystem *vfs, const char *path); // FIXME
int vfs_invalidate_at_path(vfilesystem *vfs, const char *path); // FIXME

/**
 * Create a file in the VFS at a given path if it doesn't already exist.
 * If the file does exist, this function will set the error to `EEXIST`, same
 * as an `open` call with O_CREAT|O_EXCL set.
 */
int vfs_create_file_at_path(vfilesystem *vfs, const char *path)
{
  struct vfs_inode *p;
  char buffer[MAX_PATH];
  uint32_t parent;
  uint32_t inode;
  int code;

  if(!vfs_write_lock(vfs))
    return -1;

  if(!vfs_get_inode_and_parent_by_path(vfs, path, &parent, &inode, buffer, sizeof(buffer)))
    goto err;

  // Parent must exist, but target must not exist.
  if(inode)
  {
    // open() sets a different error depending on the type of the existing file.
    p = vfs_get_inode_ptr(vfs, inode);
    if(p && VFS_INODE_TYPE(p) == VFS_INODE_FILE)
      vfs_seterror(vfs, EEXIST);
    else
      vfs_seterror(vfs, EISDIR);
    goto err;
  }
  if(!parent) // Error is set by vfs_get_inode_by_path.
    goto err;

  // If the parent is cached and times out, ignore this call.
  p = vfs_get_inode_ptr_invalidate(vfs, parent);
  if(!p)
    goto err;

  // Create a virtual (i.e. not real) file.
  inode = vfs_make_inode(vfs, parent, buffer, 0, VFS_INODE_FILE);
  if(!inode)
    goto err;

  vfs_write_unlock(vfs);
  return 0;

err:
  code = vfs_geterror(vfs);
  vfs_write_unlock(vfs);
  return -code;
}

int vfs_open_if_exists(vfilesystem *vfs, const char *path, boolean is_write,
 uint32_t *_inode)
{
  struct vfs_inode *n;
  uint32_t inode;
  int code;

  if(!vfs_write_lock(vfs))
    return -1;

  inode = vfs_get_inode_by_path(vfs, path);
  if(!inode)
    goto err;

  n = vfs_get_inode_ptr_invalidate(vfs, inode);
  if(!n || n->refcount >= VFS_MAX_REFCOUNT)
    goto err;

  // Opening directories is nonsensical.
  if(is_write && VFS_INODE_TYPE(n) == VFS_INODE_DIR)
  {
    vfs_seterror(vfs, EISDIR);
    goto err;
  }

  // TODO: writethrough and writeback caching would be nice but for now just
  // force file IO. Virtual files are fine to write to, though.
  if(is_write && VFS_IS_CACHED(n))
  {
    n->timestamp = VFS_INVALIDATE_FORCE;
    goto err;
  }

  n->refcount++;

  vfs_write_unlock(vfs);
  *_inode = inode;
  return 0;

err:
  code = vfs_geterror(vfs);
  vfs_write_unlock(vfs);
  return -code;
}

int vfs_open_if_exists_or_cacheable(vfilesystem *vfs, const char *path,
 boolean is_write, uint32_t *_inode)
{
  // FIXME
  return -1;
}

int vfs_close(vfilesystem *vfs, uint32_t inode)
{
  struct vfs_inode *n;
  if(inode >= vfs->table_length)
    return -EBADF;

  if(!vfs_read_lock(vfs))
    return -1;

  n = vfs_get_inode_ptr(vfs, inode);
  assert(n->refcount > 0);
  n->refcount--;

  // TODO cache writeback
  vfs_read_unlock(vfs);
  return 0;
}

/**
 * vfiles based on inodes in the VFS don't control their data buffer and may
 * lose access to it at any time. If a vfile writes to an inode's buffer and
 * another vfiles reads from it, the buffer pointer may also be stale from
 * reallocation. Each time one of these vfiles performs an operation, it needs
 * to acquire read or write access to the VFS.
 */

int vfs_lock_file_read(vfilesystem *vfs, uint32_t inode,
 const unsigned char **data, size_t *data_length)
{
  if(inode < vfs->table_length)
  {
    struct vfs_inode *n = vfs_get_inode_ptr(vfs, inode);
    if(!n || !n->refcount)
      return -EBADF;

    if(vfs_read_lock(vfs))
    {
      *data = n->contents.data;
      *data_length = n->length;
      return 0;
    }
  }
  return -EBADF;
}

int vfs_unlock_file_read(vfilesystem *vfs, uint32_t inode)
{
  if(inode < vfs->table_length)
  {
    struct vfs_inode *n = vfs_get_inode_ptr(vfs, inode);
    if(!n || !n->refcount)
      return -EBADF;

    if(vfs_read_unlock(vfs))
      return 0;
  }
  return -EBADF;
}

int vfs_lock_file_write(vfilesystem *vfs, uint32_t inode,
 unsigned char ***data, size_t **data_length, size_t **data_alloc)
{
  if(inode < vfs->table_length)
  {
    struct vfs_inode *n = vfs_get_inode_ptr(vfs, inode);
    if(!n || !n->refcount)
      return -EBADF;

    if(vfs_write_lock(vfs))
    {
      *data = &(n->contents.data);
      *data_length = &(n->length);
      *data_alloc = &(n->length_alloc);
      return 0;
    }
  }
  return -EBADF;
}

int vfs_unlock_file_write(vfilesystem *vfs, uint32_t inode)
{
  if(inode < vfs->table_length)
  {
    struct vfs_inode *n = vfs_get_inode_ptr(vfs, inode);
    if(!n || !n->refcount)
      return -EBADF;

    if(vfs_write_unlock(vfs))
      return 0;
  }
  return -EBADF;
}

/**
 * Create a virtual directory (i.e. not a cached copy of a real directory).
 * If this virtual directory is being created on a real path, that path must
 * be cached via `vfs_cache_at_path` prior to calling this function.
 *
 * This function does not set `errno`, but the equivalent `errno` code can be
 * retrieved with `vfs_error` after a failed call.
 */
int vfs_mkdir(vfilesystem *vfs, const char *path, int mode)
{
  struct vfs_inode *p;
  char buffer[MAX_PATH];
  uint32_t parent;
  uint32_t inode;
  int code;

  if(!vfs_write_lock(vfs))
    return -1;

  if(!vfs_get_inode_and_parent_by_path(vfs, path, &parent, &inode, buffer, sizeof(buffer)))
    goto err;

  // Parent must exist, but target must not exist.
  if(inode)
  {
    vfs_seterror(vfs, EEXIST);
    goto err;
  }
  if(!parent) // Error is set by vfs_get_inode_by_path.
    goto err;

  // If the parent is cached and times out, ignore this call.
  p = vfs_get_inode_ptr_invalidate(vfs, parent);
  if(!p)
    goto err;

  // Create a virtual (i.e. not real) directory.
  inode = vfs_make_inode(vfs, parent, buffer, 0, VFS_INODE_DIR);
  if(!inode)
    goto err;

  vfs_write_unlock(vfs);
  return 0;

err:
  code = vfs_geterror(vfs);
  vfs_write_unlock(vfs);
  return -code;
}

/**
 * Rename a virtual file or directory (i.e. not a cached file or directory).
 * If this is a cached real file or directory, it should be renamed separately
 * and invalidated with `vfs_invalidate_at_path` (and possibly recached with
 * `vfs_cache_at_path`).
 *
 * This function does not set `errno`, but the equivalent `errno` code can be
 * retrieved with `vfs_error` after a failed call.
 */
int vfs_rename(vfilesystem *vfs, const char *oldpath, const char *newpath)
{
  // TODO: renaming virtual files raises a few too many questions right now...
  // this could be particularly bad with virtual file trees from mounted ZIPs.
  return -1;
}

/**
 * Remove a virtual file (i.e. not a cached copy of a real file).
 * If this is a cached real file, it should be unlinked separately and
 * invalidated with `vfs_invalidate_at_path`.
 *
 * This function does not set `errno`, but the equivalent `errno` code can be
 * retrieved with `vfs_error` after a failed call.
 */
int vfs_unlink(vfilesystem *vfs, const char *path)
{
  struct vfs_inode *n;
  uint32_t inode;
  int code;

  if(!vfs_write_lock(vfs))
    return -1;

  inode = vfs_get_inode_by_path(vfs, path);
  if(!inode)
    goto err;

  // This function is not applicable to cached files.
  n = vfs_get_inode_ptr_invalidate(vfs, inode);
  if(!n || VFS_IS_CACHED(n))
    goto err;

  // If this is a directory, this call should be ignored.
  if(VFS_INODE_TYPE(n) == VFS_INODE_DIR)
  {
    vfs_seterror(vfs, EPERM);
    goto err;
  }

  // If the inode is currently in use, this call should be ignored.
  if(n->refcount)
  {
    vfs_seterror(vfs, EBUSY);
    goto err;
  }
  vfs_release_inode(vfs, inode);
  vfs_write_unlock(vfs);
  return 0;

err:
  code = vfs_geterror(vfs);
  vfs_write_unlock(vfs);
  return -code;
}

/**
 * Remove an empty virtual directory (i.e. not a cached directory).
 * If this is a cached directory, it should be removed separately and
 * invalidated with `vfs_invalidate_at_path`.
 *
 * This function does not set `errno`, but the equivalent `errno` code can be
 * retrieved with `vfs_error` after a failed call.
 */
int vfs_rmdir(vfilesystem *vfs, const char *path)
{
  struct vfs_inode *n;
  uint32_t inode;
  int code;

  if(!vfs_write_lock(vfs))
    return -1;

  inode = vfs_get_inode_by_path(vfs, path);
  if(!inode)
    goto err;

  // This function is not applicable to cached files.
  n = vfs_get_inode_ptr_invalidate(vfs, inode);
  if(!n || VFS_IS_CACHED(n))
    goto err;

  // If this is a file, this call should be ignored.
  if(VFS_INODE_TYPE(n) != VFS_INODE_DIR)
  {
    vfs_seterror(vfs, ENOTDIR);
    goto err;
  }

  // If the inode is not empty, this call should be ignored.
  if(n->length > 2)
  {
    vfs_seterror(vfs, ENOTEMPTY);
    goto err;
  }

  // If the inode is currently in use, this call should be ignored.
  if(n->refcount)
  {
    vfs_seterror(vfs, EBUSY);
    goto err;
  }
  vfs_release_inode(vfs, inode);
  vfs_write_unlock(vfs);
  return 0;

err:
  code = vfs_geterror(vfs);
  vfs_write_unlock(vfs);
  return -code;
}

/**
 * Get access permissions for a virtual file or directory (i.e. not cached).
 * If this is a cached file or directory, it should be checked with `access`.
 * Currently, the VFS system doesn't bother with permissions, so this works
 * as long as the file exists and isn't cached.
 *
 * This function does not set `errno`, but the equivalent `errno` code can be
 * retrieved with `vfs_error` after a failed call.
 */
int vfs_access(vfilesystem *vfs, const char *path, int mode)
{
  struct vfs_inode *n;
  uint32_t inode;
  int code;

  if(!vfs_read_lock(vfs))
    return -1;

  inode = vfs_get_inode_by_path(vfs, path);
  if(!inode)
    goto err;

  // This function is not applicable to cached files.
  n = vfs_get_inode_ptr(vfs, inode);
  if(!n || VFS_IS_CACHED(n))
    goto err;

  // All operations are allowed for virtual files.
  vfs_read_unlock(vfs);
  return 0;

err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return -code;
}

/**
 * Get extended information about any file or directory (i.e. virtual or cached).
 * This function allows usage on cached files since `stat` calls can be very
 * slow and do not modify the filesystem.
 *
 * This function does not set `errno`, but the equivalent `errno` code can be
 * retrieved with `vfs_error` after a failed call.
 */
int vfs_stat(vfilesystem *vfs, const char *path, struct stat *st)
{
  struct vfs_inode *n;
  uint32_t inode;
  int mode = S_IRWXU|S_IRWXG|S_IRWXO;
  int code;

  if(!vfs_read_lock(vfs))
    return -1;

  inode = vfs_get_inode_by_path(vfs, path);
  if(!inode)
    goto err;

  n = vfs_get_inode_ptr_invalidate(vfs, inode);
  if(!n)
    goto err;

  memset(st, 0, sizeof(struct stat));

  if(VFS_INODE_TYPE(n) == VFS_INODE_FILE)
  {
    st->st_mode = S_IFREG | mode;
    st->st_size = n->length;
  }
  else
    st->st_mode = S_IFDIR | mode;

  // Dummy device value to indicate this is an inode from an MZX VFS.
  st->st_dev = ('M'<<24) | ('Z'<<16) | ('X'<<8) | ('V');
  st->st_ino = inode;
  st->st_nlink = 1;
  // This is cheating a little--atime is the access time but noatime can
  // cause it to never be updated in real filesystems. ctime is for status
  // changes but this VFS treats modifications and status changes as the same.
  st->st_atime = n->create_time;
  st->st_mtime = n->modify_time;
  st->st_ctime = n->modify_time;
  vfs_read_unlock(vfs);
  return 0;

err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return -code;
}

// FIXME function to get list of ONLY virtual files for dirent
// this should be performed between vfs_open and vfs_close.

#endif /* VIRTUAL_FILESYSTEM */
