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
#include <sys/stat.h>

#include "vfs.h"

#ifdef VIRTUAL_FILESYSTEM

#include "../platform.h" /* get_ticks */
#include "path.h"

// Maximum number of seconds a cached file can be considered valid.
#define VFS_INVALIDATE_S (5*60)
// Force invalidation of a cached file.
#define VFS_INVALIDATE_FORCE UINT32_MAX
// Maximum reference count for a given VFS inode.
#define VFS_MAX_REFCOUNT 255

#define VFS_DEFAULT_FILE_SIZE 32
#define VFS_DEFAULT_DIR_SIZE 4

#define VFS_NO_INODE 0
#define VFS_ROOT_INODE 1

#define VFS_IDX_SELF   0
#define VFS_IDX_PARENT 1

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
  size_t length;
  size_t length_alloc;
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
  size_t length;
  size_t length_alloc;
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
  int error;
};

static uint32_t vfs_get_timestamp(void)
{
  uint32_t timestamp = (uint32_t)(get_ticks() / 1000);
  return timestamp ? timestamp : 1;
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

static int vfs_seterror(vfilesystem *vfs, int e)
{
  vfs->error = e;
  return VFS_NO_INODE;
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
 * Get a pointer to an inode data structure from its index.
 * This will return a pointer even for cleared inodes.
 */
static struct vfs_inode *vfs_get_inode_ptr(vfilesystem *vfs, uint32_t inode)
{
  assert(inode < vfs->table_length);
  return &(vfs->table[inode]);
}

/**
 * Get a pointer to an inode data structure from its index.
 * If the timestamp indicates it is stale, clear it instead and return NULL.
 */
static struct vfs_inode *vfs_get_inode_ptr_invalidate(vfilesystem *vfs, uint32_t inode)
{
  struct vfs_inode *n = vfs_get_inode_ptr(vfs, inode);
  uint32_t current_time = vfs_get_timestamp();

  if(n->timestamp)
  {
    if(n->timestamp == VFS_INVALIDATE_FORCE ||
     (current_time - n->timestamp > VFS_INVALIDATE_S))
    {
      if(!n->refcount)
      {
        vfs_release_inode(vfs, inode);
        return NULL;
      }
      else
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
      return vfs_seterror(vfs, ENOSPC);

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
  uint32_t inode;
  int cmp;

  assert(parent->length >= 2);

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

  if(len >= (ssize_t)sizeof(buffer))
    return vfs_seterror(vfs, ENOENT);

  if(len > 0)
  {
    memcpy(buffer, *path, len);
    buffer[len] = '\0';
    *path += len;

    inode = vfs_get_inode_in_parent_by_name(vfs, vfs_get_inode_ptr(vfs, 0), buffer, NULL);
    if(inode == VFS_NO_INODE)
    {
#if defined(__WIN32__)
      // Windows and DJGPP: / behaves as the root of the current active drive.
      if(!strcmp(buffer, "/") || !strcmp(buffer, "\\"))
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
  char *current;
  char *next;

  next = relative_path;
  while((current = vfs_tokenize(&next)))
  {
    struct vfs_inode *parent = vfs_get_inode_ptr(vfs, inode);
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
  ssize_t length;

  inode = vfs_get_path_base_inode(vfs, &path);
  if(!inode)
    return VFS_NO_INODE;

  length = path_clean_slashes_copy(buffer, sizeof(buffer), path);
  if(length < 0)
    return VFS_NO_INODE;

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
  ssize_t length;

  base = vfs_get_path_base_inode(vfs, &path);
  if(!base)
    return false;

  length = path_clean_slashes_copy(buffer, sizeof(buffer), path);
  if(length < 0)
    return false;

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
    inode = vfs_get_inode_in_parent_by_name(vfs, p, child, NULL);
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
    return vfs_seterror(vfs, EEXIST);

  pos = vfs_get_next_free_inode(vfs);
  if(!pos)
    return VFS_NO_INODE;

  if(!vfs_inode_expand_directory(parent, 1))
    return vfs_seterror(vfs, ENOSPC);

  n = vfs_get_inode_ptr(vfs, pos);

  if((flags & VFS_INODE_TYPEMASK) == VFS_INODE_DIR)
  {
    if(!vfs_inode_init_directory(n, name, init_alloc, !!(flags & VFS_INODE_IS_REAL)))
      return vfs_seterror(vfs, ENOSPC);
  }
  else
  {
    if(!vfs_inode_init_file(n, name, init_alloc, !!(flags & VFS_INODE_IS_REAL)))
      return vfs_seterror(vfs, ENOSPC);
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
    return vfs_seterror(vfs, ENOTDIR);
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
  return NULL;
}

void vfs_free(vfilesystem *vfs)
{
#ifdef VIRTUAL_FILESYSTEM
  vfs_clear(vfs);
  free(vfs);
#endif
}

void vfs_reset(vfilesystem *vfs)
{
  vfs_clear(vfs);
}

boolean vfs_cache_at_path(vfilesystem *vfs, const char *path); // FIXME
boolean vfs_invalidate_at_path(vfilesystem *vfs, const char *path); // FIXME
boolean vfs_create_file_at_path(vfilesystem *vfs, const char *path); // FIXME

/**
 * vfiles based on inodes in the VFS don't control their data buffer and may
 * lose access to it at any time. If a vfile writes to an inode's buffer and
 * another vfiles reads from it, the buffer pointer may also be stale from
 * reallocation. Each time one of these vfiles performs an operation, it needs
 * to sync with the VFS.
 *
 * TODO: memory vfile buffering to get around this?
 */
boolean vfs_sync_file(vfilesystem *vfs, uint32_t inode,
 void ***data, size_t **data_length, size_t **data_alloc)
{
  if(inode < vfs->table_length)
  {
    struct vfs_inode *n = vfs_get_inode_ptr(vfs, inode);
    if(n->refcount)
    {
      *data = (void **)&(n->contents.data);
      *data_length = &(n->length);
      *data_alloc = &(n->length_alloc);
      return true;
    }
  }
  return false;
}

uint32_t vfs_open_if_exists(vfilesystem *vfs, const char *path, boolean is_write)
{
  uint32_t inode = vfs_get_inode_by_path(vfs, path);
  if(inode)
  {
    struct vfs_inode *n = vfs_get_inode_ptr_invalidate(vfs, inode);
    if(!n || n->refcount >= VFS_MAX_REFCOUNT)
      return VFS_NO_INODE;

    // Write mode for directories is nonsensical.
    if(is_write && VFS_INODE_TYPE(n) == VFS_INODE_DIR)
      return vfs_seterror(vfs, EPERM);

    // TODO: writethrough and writeback caching would be nice but for now just
    // force file IO. Virtual files are fine to write to, though.
    if(is_write && n->timestamp)
    {
      n->timestamp = VFS_INVALIDATE_FORCE;
      return VFS_NO_INODE;
    }

    n->refcount++;
  }
  return inode;
}

uint32_t vfs_open_if_exists_or_cacheable(vfilesystem *vfs, const char *path,
 boolean is_write)
{
  // FIXME
  return 0;
}

void vfs_close(vfilesystem *vfs, uint32_t inode)
{
  struct vfs_inode *n;
  if(inode >= vfs->table_length)
    return;

  n = vfs_get_inode_ptr(vfs, inode);
  assert(n->refcount > 0);
  n->refcount--;

  // TODO cache writeback
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

  if(!vfs_get_inode_and_parent_by_path(vfs, path, &parent, &inode, buffer, sizeof(buffer)))
    return -1;

  // Parent must exist, but target must not exist.
  if(inode)
  {
    vfs_seterror(vfs, EEXIST);
    return -1;
  }
  if(!parent) // Error is set by vfs_get_inode_by_path.
    return -1;

  // If the parent is cached and times out, ignore this call.
  p = vfs_get_inode_ptr_invalidate(vfs, parent);
  if(!p)
    return -1;

  // Create a virtual (i.e. not real) directory.
  inode = vfs_make_inode(vfs, p, buffer, 0, VFS_INODE_DIR);
  if(!inode)
    return -1;

  vfs_seterror(vfs, 0);
  return 0;
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
  uint32_t inode = vfs_get_inode_by_path(vfs, path);
  if(inode)
  {
    // This function is not applicable to cached files.
    struct vfs_inode *n = vfs_get_inode_ptr_invalidate(vfs, inode);
    if(!n || n->timestamp)
      return -1;

    // If this is a directory, this call should be ignored.
    if(VFS_INODE_TYPE(n) == VFS_INODE_DIR)
    {
      vfs_seterror(vfs, EPERM);
      return -1;
    }

    // If the inode is currently in use, this call should be ignored.
    if(n->refcount)
    {
      vfs_seterror(vfs, EBUSY);
      return -1;
    }
    vfs_release_inode(vfs, inode);
    vfs_seterror(vfs, 0);
    return 0;
  }
  return -1;
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
  uint32_t inode = vfs_get_inode_by_path(vfs, path);
  if(inode)
  {
    // This function is not applicable to cached files.
    struct vfs_inode *n = vfs_get_inode_ptr_invalidate(vfs, inode);
    if(!n || n->timestamp)
      return -1;

    // If this is a file, this call should be ignored.
    if(VFS_INODE_TYPE(n) != VFS_INODE_DIR)
    {
      vfs_seterror(vfs, ENOTDIR);
      return -1;
    }

    // If the inode is currently in use, this call should be ignored.
    if(n->refcount)
    {
      vfs_seterror(vfs, EBUSY);
      return -1;
    }
    vfs_release_inode(vfs, inode);
    vfs_seterror(vfs, 0);
    return 0;
  }
  return -1;
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
  uint32_t inode = vfs_get_inode_by_path(vfs, path);
  if(inode)
  {
    // This function is not applicable to cached files.
    struct vfs_inode *n = vfs_get_inode_ptr_invalidate(vfs, inode);
    if(!n || n->timestamp)
      return -1;

    // All operations are allowed for virtual files.
    vfs_seterror(vfs, 0);
    return 0;
  }
  return -1;
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
  return -1;
  /*
  // FIXME make sure it exists and populate this info from the vfs inode.
  memset(st, 0, sizeof(struct stat));
  st->st_mode = S_IFDIR; // FIXME
  st->st_nlink = 1;
  st->st_size = 0; // FIXME
  st->st_atime = 0; // TODO
  st->st_mtime = 0; // TODO
  st->st_ctime = 0; // TODO
  return 0;
  */
}

// FIXME function to get list of ONLY virtual files for dirent
// this should be performed between vfs_open and vfs_close.


/**
 * Return the error value for a VFS and clear its internal error code.
 * This is roughly equivalent to checking and then clearing `errno` after a
 * standard POSIX filesystem call. The error code will be cleared on a
 * successful call to any of the functions that set this (as other functions
 * they call internally may set it).
 */
int vfs_error(vfilesystem *vfs)
{
  int error = vfs->error;
  vfs->error = 0;
  return error;
}
