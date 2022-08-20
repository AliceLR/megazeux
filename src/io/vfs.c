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
#include "vio.h"

// Force invalidation of a cached file.
#define VFS_INVALIDATE_FORCE UINT32_MAX
// Maximum reference count for a given VFS inode.
#define VFS_MAX_REFCOUNT 255
// Maximum name length.
#define VFS_MAX_NAME UINT16_MAX

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
  int64_t create_time;
  int64_t modify_time;
  uint8_t flags; // 0=deleted
  uint8_t refcount; // If 255, refuse creation of new refs.
  uint16_t name_length;
  char name[20];
};

/* If `flags` & VFS_INODE_NAME_ALLOC, `name` is a pointer instead of a buffer.
 * `name` is still null terminated in this case. Alloc size is strlen(name)+1. */
struct vfs_inode_name_alloc
{
  union vfs_inode_contents contents;
  size_t length;
  size_t length_alloc;
  uint32_t timestamp;
  int64_t create_time;
  int64_t modify_time;
  uint8_t flags; // 0=deleted
  uint8_t refcount; // If 255, refuse creation of new refs.
  uint16_t name_length;
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
  platform_thread_id origin;
  platform_mutex lock;
  platform_cond cond;
  int num_writers;
  int num_readers;
  int num_promotions;
#endif
  boolean is_writer;
  int error;
  char current_path[MAX_PATH];
};

static uint32_t vfs_get_timestamp(void)
{
  uint32_t timestamp = (uint32_t)(get_ticks() / 1000);
  return timestamp ? timestamp : 1;
}

static int64_t vfs_get_date(void)
{
  return time(NULL);
}

static int vfs_name_cmp(const char *a, const char *b)
{
#ifdef VIRTUAL_FILESYSTEM_CASE_INSENSITIVE
  return strcasecmp(a, b);
#else
  return strcmp(a, b);
#endif
}


/**
 * vfs_inode functions.
 */

static boolean vfs_inode_init_name(struct vfs_inode *n, const char *name)
{
  size_t name_len = strlen(name);
  if(name_len > VFS_MAX_NAME)
    return false;

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

  n->name_length = name_len;
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
  n->refcount = 0;
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
  n->refcount = 0;
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

static boolean vfs_inode_insert_directory(struct vfs_inode *n, uint32_t inode, uint32_t pos)
{
  uint32_t *inodes;

  assert(VFS_INODE_TYPE(n) == VFS_INODE_DIR);
  assert(pos >= 2 && pos <= n->length);

  if(n->length == UINT32_MAX)
    return false;

  inodes = n->contents.inodes;

  if(pos < n->length)
    memmove(inodes + pos + 1, inodes + pos, sizeof(uint32_t) * (n->length - pos));

  inodes[pos] = inode;
  n->length++;
  return true;
}

static boolean vfs_inode_delete_directory(struct vfs_inode *n, uint32_t pos)
{
  uint32_t *inodes;

  assert(VFS_INODE_TYPE(n) == VFS_INODE_DIR);
  assert(pos >= 2 && pos < n->length);

  inodes = n->contents.inodes;

  if(pos < n->length - 1)
    memmove(inodes + pos, inodes + pos + 1, sizeof(uint32_t) * (n->length - pos - 1));
  n->length--;
  return true;
}

static boolean vfs_inode_move_directory(struct vfs_inode *n, uint32_t old_pos,
 uint32_t new_pos)
{
  uint32_t *inodes;
  uint32_t inode;

  assert(VFS_INODE_TYPE(n) == VFS_INODE_DIR);
  assert(old_pos >= 2 && old_pos < n->length);
  assert(new_pos >= 2 && new_pos < n->length);

  inodes = n->contents.inodes;
  inode = inodes[old_pos];

  if(old_pos < new_pos)
  {
    memmove(inodes + old_pos, inodes + old_pos + 1, sizeof(uint32_t) * (new_pos - old_pos));
    inodes[new_pos] = inode;
  }
  else

  if(old_pos > new_pos)
  {
    memmove(inodes + new_pos + 1, inodes + new_pos, sizeof(uint32_t) * (old_pos - new_pos));
    inodes[new_pos] = inode;
  }
  return true;
}


/**
 * vfilesystem functions.
 */

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
  vfs->origin = platform_get_thread_id();
  vfs->num_readers = 0;
  vfs->num_writers = 0;
  vfs->num_promotions = 0;
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

  assert(vfs->num_readers > 0);
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

  while(vfs->num_readers || vfs->is_writer)
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

  assert(vfs->is_writer);
  vfs->is_writer = false;
  platform_cond_broadcast(&(vfs->cond));

  platform_mutex_unlock(&(vfs->lock));
#endif
  return true;
}

/**
 * Promote the current thread's reader lock to a writer lock. On failure,
 * returns `false`, and the current thread will still have a reader lock.
 */
static boolean vfs_elevate_lock(vfilesystem *vfs)
{
#ifdef VIRTUAL_FILESYSTEM_PARALLEL
  if(!platform_mutex_lock(&(vfs->lock)))
    return false;

  vfs->num_writers++;
  vfs->num_promotions++;

  while(vfs->num_readers > vfs->num_promotions || vfs->is_writer)
    platform_cond_wait(&(vfs->cond), &(vfs->lock));

  vfs->num_readers--;
  vfs->num_writers--;
  vfs->num_promotions--;
  vfs->is_writer = true;

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
  if(!n->contents.inodes)
    return false;

  n->contents.inodes[VFS_IDX_SELF]   = VFS_NO_INODE;
  n->contents.inodes[VFS_IDX_PARENT] = VFS_NO_INODE;
  n->contents.inodes[2] = VFS_ROOT_INODE;
  n->length = 3;
  n->length_alloc = 3;
  n->timestamp = 0;
  n->flags = VFS_INODE_DIR;
  n->refcount = 255;
  vfs_inode_init_name(n, "");

  /* 1: / or C:\ */
  n = vfs->table[VFS_ROOT_INODE];
  n->contents.inodes = malloc(4 * sizeof(uint32_t));
  if(!n->contents.inodes)
    return false;

  n->contents.inodes[VFS_IDX_SELF]   = VFS_ROOT_INODE;
  n->contents.inodes[VFS_IDX_PARENT] = VFS_ROOT_INODE;
  n->length = 2;
  n->length_alloc = 4;
  n->timestamp = 0;
  n->create_time = vfs_get_date();
  n->modify_time = n->create_time;
  n->flags = VFS_INODE_DIR | VFS_INODE_IS_REAL;
  n->refcount = 0;
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
  vfs_inode_init_name(n, "C:\\");
#else
  vfs_inode_init_name(n, "/");
#endif

  strcpy(vfs->current_path, n->name);
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
 * Get a pointer to an inode data structure from its index.
 * This will return a pointer even for cleared inodes.
 */
static struct vfs_inode *vfs_get_inode_ptr(vfilesystem *vfs, uint32_t inode)
{
  assert(inode < vfs->table_length);
  return vfs->table[inode];
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
  while((current = path_tokenize(&next)))
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
 * Get the path for a given directory inode.
 */
static boolean vfs_get_inode_path(vfilesystem *vfs, uint32_t inode,
 char *buffer, size_t buffer_length)
{
  struct vfs_inode *n;
  uint32_t current;
  uint32_t next;
  size_t needed = 0;

  // 1. Get the required size for this string.
  current = inode;
  while(true)
  {
    n = vfs_get_inode_ptr(vfs, current);
    if(!n || !n->contents.inodes || VFS_INODE_TYPE(n) != VFS_INODE_DIR)
      return false;

    next = n->contents.inodes[VFS_IDX_PARENT];

    // Dir separator, except for the root since it already has one.
    if(needed && next != current)
      needed++;

    needed += n->name_length;

    if(next == current)
      break;

    current = next;
  }

  if(needed + 1 >= buffer_length)
    return false;

  // 2. Copy name into the buffer.
  buffer[needed] = '\0';
  current = inode;
  while(true)
  {
    n = vfs_get_inode_ptr(vfs, current);

    next = n->contents.inodes[VFS_IDX_PARENT];
    if(next == current)
      break;

    needed -= n->name_length;
    memcpy(buffer + needed, vfs_inode_name(n), n->name_length);
    buffer[--needed] = DIR_SEPARATOR_CHAR;
    current = next;
  }
  // Add root name into buffer separately.
  memcpy(buffer, vfs_inode_name(n), n->name_length);
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

  vfs_inode_insert_directory(p, pos, pos_in_parent);
  return pos;
}

/**
 * Delete an inode in the VFS in a given directory. The inode will be removed
 * from the parent directory and marked for reuse.
 */
static boolean vfs_delete_inode(vfilesystem *vfs, uint32_t parent,
 uint32_t inode, const char *name)
{
  struct vfs_inode *p;
  struct vfs_inode *n;
  uint32_t pos_in_parent;
  uint32_t pos;

  if(inode == VFS_NO_INODE)
    return vfs_seterror(vfs, ENOENT);

  p = vfs_get_inode_ptr(vfs, parent);
  n = vfs_get_inode_ptr(vfs, inode);
  assert(VFS_INODE_TYPE(p) == VFS_INODE_DIR);

  // Find in parent.
  pos = vfs_get_inode_in_parent_by_name(vfs, p, name, &pos_in_parent);
  if(pos != inode)
    return vfs_seterror(vfs, ENOENT);

  // Remove from parent.
  vfs_inode_delete_directory(p, pos_in_parent);

  // Clear.
  vfs_inode_clear(n);
  if(inode < vfs->table_next)
    vfs->table_next = inode;

  return true;
}

/**
 * Move an inode in the VFS from one directory to another. The old and new
 * parent directories may be the same directory.
 */
static boolean vfs_move_inode(vfilesystem *vfs, uint32_t old_parent,
 uint32_t new_parent, uint32_t inode, const char *old_name, const char *new_name)
{
  struct vfs_inode *old_p;
  struct vfs_inode *new_p;
  struct vfs_inode *n;
  uint32_t old_pos_in_parent;
  uint32_t new_pos_in_parent;
  uint32_t pos;

  if(inode == VFS_NO_INODE)
    return vfs_seterror(vfs, ENOENT);

  n = vfs_get_inode_ptr(vfs, inode);
  old_p = vfs_get_inode_ptr(vfs, old_parent);
  new_p = vfs_get_inode_ptr(vfs, new_parent);
  assert(VFS_INODE_TYPE(old_p) == VFS_INODE_DIR);
  assert(VFS_INODE_TYPE(new_p) == VFS_INODE_DIR);

  if(old_p != new_p && !vfs_inode_expand_directory(new_p, 1))
    return vfs_seterror(vfs, ENOSPC);

  // Find in parents.
  pos = vfs_get_inode_in_parent_by_name(vfs, old_p, old_name, &old_pos_in_parent);
  if(pos != inode)
    return vfs_seterror(vfs, ENOENT);

  pos = vfs_get_inode_in_parent_by_name(vfs, new_p, new_name, &new_pos_in_parent);
  if(pos != VFS_NO_INODE)
    return vfs_seterror(vfs, EEXIST);

  // Rename.
  if(!vfs_inode_rename(n, new_name))
    return vfs_seterror(vfs, ENOSPC);

  // Move from old position to new position.
  if(old_p != new_p)
  {
    vfs_inode_delete_directory(old_p, old_pos_in_parent);
    vfs_inode_insert_directory(new_p, inode, new_pos_in_parent);
  }
  else
  {
    // The position determined above assumes this will keep its old position,
    // so adjust it to account for its own removal.
    if(new_pos_in_parent > old_pos_in_parent)
      new_pos_in_parent--;

    vfs_inode_move_directory(old_p, old_pos_in_parent, new_pos_in_parent);
  }

  return true;
}

/**
 * Determine if inode `A` is the same as or is an ancestor of inode `B`.
 * Both inodes `A` and `B` MUST be directories currently.
 */
static boolean vfs_is_ancestor_inode(vfilesystem *vfs, uint32_t A, uint32_t B)
{
  struct vfs_inode *a = vfs_get_inode_ptr(vfs, A);
  struct vfs_inode *b = vfs_get_inode_ptr(vfs, B);
  if(!a || !b)
    return false;

  assert(VFS_INODE_TYPE(a) == VFS_INODE_DIR);
  assert(VFS_INODE_TYPE(b) == VFS_INODE_DIR);

  // Walk B back to its root.
  while(true)
  {
    if(a == b)
      return true;

    if(!b->contents.inodes || b->contents.inodes[VFS_IDX_PARENT] == B)
      return false;

    B = b->contents.inodes[VFS_IDX_PARENT];
    b = vfs_get_inode_ptr(vfs, B);
    assert(b && VFS_INODE_TYPE(b) == VFS_INODE_DIR);
  }
}

#if 0
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


/**
 * External functions.
 */

/**
 * Create a file in the VFS at a given path if it doesn't already exist.
 * If the file does exist, this function will set the error to `EEXIST`, same
 * as an `open` call with O_CREAT|O_EXCL set.
 *
 * @param vfs   VFS handle.
 * @param path  path of file within `vfs` to create.
 * @return      0 on success, otherwise a negative number corresponding to the
 *              relevant `errno` code (-255 for unknown error). This function
 *              does not set `errno`.
 */
int vfs_create_file_at_path(vfilesystem *vfs, const char *path)
{
  struct vfs_inode *p;
  char buffer[MAX_PATH];
  uint32_t parent;
  uint32_t inode;
  int code = 0;

  if(!vfs_read_lock(vfs))
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
  p = vfs_get_inode_ptr(vfs, parent);
  if(!p)
    goto err;

  if(!vfs_elevate_lock(vfs))
    goto err;

  // Create a virtual (i.e. not real) file.
  inode = vfs_make_inode(vfs, parent, buffer, 0, VFS_INODE_FILE);
  if(inode)
  {
    struct vfs_inode *n = vfs_get_inode_ptr(vfs, inode);
    n->create_time = vfs_get_date();
    n->modify_time = n->create_time;
    p->modify_time = n->create_time;
  }
  else
    code = vfs_geterror(vfs);

  vfs_write_unlock(vfs);
  return -code;

err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return -code;
}

/**
 * "Open" a file in a VFS, obtaining a file descriptor for the file and
 * incrementing its reference count by one. This is meaningless for directories,
 * and calls where the path is a directory will return `-EISDIR`
 * (use `vfs_readdir` instead).
 *
 * @param vfs       VFS handle.
 * @param path      path of file within `vfs` to "open".
 * @param is_write  should be `true` if write operations are to be performed.
 * @param inode     a pointer for the file descriptor to be written to.
 * @return          0 on success, otherwise a negative number corresponding to the
 *                  relevant `errno` code (-255 for unknown error). This function
 *                  does not set `errno`.
 */
int vfs_open_if_exists(vfilesystem *vfs, const char *path, boolean is_write,
 uint32_t *_inode)
{
  struct vfs_inode *n;
  uint32_t inode;
  int code;

  if(!vfs_read_lock(vfs))
    return -1;

  inode = vfs_get_inode_by_path(vfs, path);
  if(!inode)
    goto err;

  n = vfs_get_inode_ptr(vfs, inode);
  if(!n || n->refcount >= VFS_MAX_REFCOUNT)
    goto err;

  // Opening directories is nonsensical.
  if(VFS_INODE_TYPE(n) == VFS_INODE_DIR)
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

  vfs_read_unlock(vfs);
  *_inode = inode;
  return 0;

err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return -code;
}

/**
 * "Close" a file in a VFS, decrementing its reference count by one.
 *
 * @param vfs   VFS handle.
 * @param inode file descriptor to "close".
 * @return      0 on success, otherwise a negative number corresponding to the
 *              relevant `errno` code (-255 for unknown error). This function
 *              does not set `errno`.
 */
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

  n->modify_time = vfs_get_date();

  // TODO cache writeback
  vfs_read_unlock(vfs);
  return 0;
}

/**
 * Truncate an "open" VFS file to length 0.
 *
 * @param vfs   VFS handle.
 * @param inode file descriptor to the file to truncate.
 * @return      0 on success, otherwise a negative number corresponding to the
 *              relevant `errno` code (-255 for unknown error). This function
 *              does not set `errno`.
 */
int vfs_truncate(vfilesystem *vfs, uint32_t inode)
{
  struct vfs_inode *n;
  if(inode >= vfs->table_length)
    return -EBADF;

  if(!vfs_write_lock(vfs))
    return -1;

  n = vfs_get_inode_ptr(vfs, inode);
  if(!n || !n->refcount)
  {
    vfs_write_unlock(vfs);
    return -EBADF;
  }

  if(n->length_alloc > VFS_DEFAULT_FILE_SIZE)
  {
    unsigned char *tmp = (unsigned char *)realloc(n->contents.data, VFS_DEFAULT_FILE_SIZE);
    if(tmp)
      n->contents.data = tmp;
  }
  n->length = 0;

  vfs_write_unlock(vfs);
  return 0;
}

/**
 * Lock an "open" VFS file for reading and retrieve its underlying data buffer.
 * Multiple reading locks can be acquired on a VFS handle simultaneously, but
 * a reading lock will block if a writing lock is held. To avoid deadlocks,
 * only one lock should ever be acquired per thread simultaneously.
 *
 * @param vfs         VFS handle.
 * @param inode       file descriptor to the file to read.
 * @param data        pointer to store a pointer to the underlying data to.
 * @param data_length pointer to store the length of the underlying data to.
 * @return            0 on success, otherwise a negative number corresponding to the
 *                    relevant `errno` code (-255 for unknown error). This function
 *                    does not set `errno`.
 */
int vfs_lock_file_read(vfilesystem *vfs, uint32_t inode,
 const unsigned char **data, size_t *data_length)
{
  struct vfs_inode *n;
  if(inode < vfs->table_length)
  {
    if(!vfs_read_lock(vfs))
      return -1;

    n = vfs_get_inode_ptr(vfs, inode);
    if(!n || !n->refcount)
    {
      vfs_read_unlock(vfs);
      return -EBADF;
    }

    *data = n->contents.data;
    *data_length = n->length;
    return 0;
  }
  return -EBADF;
}

/**
 * Release a held reading lock on a VFS handle.
 *
 * @param vfs   VFS handle.
 * @param inode file descriptor to the file to read.
 * @return      0 on success, otherwise a negative number corresponding to the
 *              relevant `errno` code (-255 for unknown error). This function
 *              does not set `errno`.
 */
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

/**
 * Lock an "open" VFS file for writing and retrieve its underlying data buffer.
 * Only one writing lock can be acquired on a VFS handle at any given time.
 * To avoid deadlocks, only one lock should ever be acquired per thread
 * simultaneously.
 *
 * @param vfs         VFS handle.
 * @param inode       file descriptor to the file to write.
 * @param data        pointer to store a pointer to the pointer to the underlying data to.
 * @param data_length pointer to store a pointer to the the length of the underlying data to.
 * @param data_alloc  pointer to store a pointer to the the allocation size of the underlying data to.
 * @return            0 on success, otherwise a negative number corresponding to the
 *                    relevant `errno` code (-255 for unknown error). This function
 *                    does not set `errno`.
 */
int vfs_lock_file_write(vfilesystem *vfs, uint32_t inode,
 unsigned char ***data, size_t **data_length, size_t **data_alloc)
{
  struct vfs_inode *n;
  if(inode < vfs->table_length)
  {
    if(!vfs_write_lock(vfs))
      return -1;

    n = vfs_get_inode_ptr(vfs, inode);
    if(!n || !n->refcount)
    {
      vfs_write_unlock(vfs);
      return -EBADF;
    }

    *data = &(n->contents.data);
    *data_length = &(n->length);
    *data_alloc = &(n->length_alloc);
    return 0;
  }
  return -EBADF;
}

/**
 * Release a held writing lock on a VFS handle.
 *
 * @param vfs   VFS handle.
 * @param inode file descriptor to the file to write.
 * @return      0 on success, otherwise a negative number corresponding to the
 *              relevant `errno` code (-255 for unknown error). This function
 *              does not set `errno`.
 */
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
 * Set the current working directory of a VFS. This operation currently can
 * only be performed by the thread that created this VFS.
 *
 * @param vfs   VFS handle.
 * @param path  path of directory to set as the current working directory.
 * @return      0 on success, otherwise a negative number corresponding to the
 *              relevant `errno` code (-255 for unknown error). This function
 *              does not set `errno`.
 */
int vfs_chdir(vfilesystem *vfs, const char *path)
{
  struct vfs_inode *n;
  uint32_t inode;
  uint32_t root;
  int code;
  char buffer[MAX_PATH * 2];
  size_t len;

  if(!vfs_read_lock(vfs))
    return -1;

#ifdef VIRTUAL_FILESYSTEM_PARALLEL
  // TODO: for now, only allow chdir from the thread that created this VFS.
  if(!platform_is_same_thread(vfs->origin, platform_get_thread_id()))
  {
    vfs_seterror(vfs, EACCES);
    goto err;
  }
#endif

  // Navigate the data structure first to get better errors than path_navigate.
  inode = vfs_get_inode_by_path(vfs, path);
  if(!inode)
    goto err;

  // If this is the same as the current inode, exit early.
  if(inode == vfs->current)
  {
    vfs_read_unlock(vfs);
    return 0;
  }

  n = vfs_get_inode_ptr(vfs, inode);
  if(!n)
    goto err;

  if(VFS_INODE_TYPE(n) != VFS_INODE_DIR)
  {
    vfs_seterror(vfs, ENOTDIR);
    goto err;
  }

  // Get the new working directory path string.
  if(!vfs_get_inode_path(vfs, inode, buffer, sizeof(buffer)))
  {
    vfs_seterror(vfs, ENAMETOOLONG);
    goto err;
  }
  len = strlen(buffer);
  if(len >= sizeof(vfs->current_path))
  {
    vfs_seterror(vfs, ENAMETOOLONG);
    goto err;
  }

  // Find the root this directory exists in.
  root = inode;
  while(true)
  {
    uint32_t next;
    if(!root || !n || !n->contents.inodes)
      goto err;

    next = n->contents.inodes[VFS_IDX_PARENT];
    if(next == root)
      break;

    root = next;
    n = vfs_get_inode_ptr(vfs, root);
  }

  if(!vfs_elevate_lock(vfs))
    goto err;

  memcpy(vfs->current_path, buffer, len + 1);
  vfs->current = inode;
  vfs->current_root = root;
  vfs_write_unlock(vfs);
  return 0;

err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return -code;
}

/**
 * Get the current working directory of a VFS as a path string.
 *
 * @param vfs       VFS handle.
 * @param dest      buffer to store the current working directory path to.
 * @param dest_len  size of `dest` in bytes.
 * @return          0 on success, otherwise a negative number corresponding to the
 *                  relevant `errno` code (-255 for unknown error). This function
 *                  does not set `errno`.
 */
int vfs_getcwd(vfilesystem *vfs, char *dest, size_t dest_len)
{
  size_t total_length;

  if(!dest || !dest_len)
    return -EINVAL;

  if(!vfs_read_lock(vfs))
    return -1;

  total_length = strlen(vfs->current_path);
  if(total_length >= dest_len)
  {
    vfs_read_unlock(vfs);
    return -ERANGE;
  }
  // This should already have been cleaned by vfs_chdir, so copy directly.
  memcpy(dest, vfs->current_path, total_length + 1);

  vfs_read_unlock(vfs);
  return 0;
}

/**
 * Create a virtual directory (i.e. not a cached copy of a real directory).
 * If this virtual directory is being created on a real path, that path must
 * be cached via `vfs_cache_at_path` prior to calling this function.
 *
 * @param vfs   VFS handle.
 * @param path  path to create a directory at within `vfs`.
 * @param mode  permission bits for the created directory (ignored).
 * @return      0 on success, otherwise a negative number corresponding to the
 *              relevant `errno` code (-255 for unknown error). This function
 *              does not set `errno`.
 */
int vfs_mkdir(vfilesystem *vfs, const char *path, int mode)
{
  struct vfs_inode *p;
  char buffer[MAX_PATH];
  uint32_t parent;
  uint32_t inode;
  int code = 0;

  if(!vfs_read_lock(vfs))
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

  p = vfs_get_inode_ptr(vfs, parent);
  if(!p)
    goto err;

  if(!vfs_elevate_lock(vfs))
    goto err;

  // Create a virtual (i.e. not real) directory.
  inode = vfs_make_inode(vfs, parent, buffer, 0, VFS_INODE_DIR);
  if(inode)
  {
    struct vfs_inode *n = vfs_get_inode_ptr(vfs, inode);
    n->create_time = vfs_get_date();
    n->modify_time = n->create_time;
    p->modify_time = n->create_time;
  }
  else
    code = vfs_geterror(vfs);

  vfs_write_unlock(vfs);
  return -code;

err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return -code;
}

/**
 * Rename a virtual file or directory (i.e. not a cached file or directory).
 * If this is a cached real file or directory, it should be renamed separately
 * and invalidated with `vfs_invalidate_at_path` (and possibly recached with
 * `vfs_cache_at_path`).
 *
 * @param vfs     VFS handle.
 * @param oldpath path of a file or directory within `vfs` to rename.
 * @param newpath new path to rename the file or directory to.
 * @return        0 on success, otherwise a negative number corresponding to the
 *                relevant `errno` code (-255 for unknown error). This function
 *                does not set `errno`.
 */
int vfs_rename(vfilesystem *vfs, const char *oldpath, const char *newpath)
{
  char buffer[MAX_PATH];
  char buffer2[MAX_PATH];
  struct vfs_inode *old_p;
  struct vfs_inode *old_n;
  struct vfs_inode *new_p;
  struct vfs_inode *new_n;
  uint32_t old_parent;
  uint32_t new_parent;
  uint32_t old_inode;
  uint32_t new_inode;
  int code = 0;

  if(!vfs_read_lock(vfs))
    return -1;

  if(!vfs_get_inode_and_parent_by_path(vfs, oldpath, &old_parent, &old_inode, buffer, sizeof(buffer)))
    goto err;

  // old must have both a parent and an inode and they should be different.
  if(!old_parent || !old_inode)
  {
    vfs_seterror(vfs, ENOENT);
    goto err;
  }
  if(old_parent == old_inode)
  {
    vfs_seterror(vfs, EBUSY);
    goto err;
  }
  old_p = vfs_get_inode_ptr(vfs, old_parent);
  old_n = vfs_get_inode_ptr(vfs, old_inode);
  // This function is not applicable to cached files.
  if(VFS_INODE_TYPE(old_p) != VFS_INODE_DIR || VFS_IS_CACHED(old_n))
    goto err;

  if(!vfs_get_inode_and_parent_by_path(vfs, newpath, &new_parent, &new_inode, buffer2, sizeof(buffer2)))
    goto err;

  // new must have a parent, and its inode should be different.
  if(!new_parent)
  {
    vfs_seterror(vfs, ENOENT);
    goto err;
  }
  if(new_parent == new_inode)
  {
    vfs_seterror(vfs, EBUSY);
    goto err;
  }
  new_p = vfs_get_inode_ptr(vfs, new_parent);
  new_n = new_inode ? vfs_get_inode_ptr(vfs, new_inode) : NULL;
  // This function is not applicable to cached files.
  if(VFS_INODE_TYPE(new_p) != VFS_INODE_DIR || (new_n && VFS_IS_CACHED(new_n)))
    goto err;

  // Special case: if old and new are the same, return early with success.
  if(old_n == new_n)
  {
    vfs_read_unlock(vfs);
    return 0;
  }

  if(VFS_INODE_TYPE(old_n) == VFS_INODE_DIR)
  {
    // If old is a dir, the new parent should not be prefixed by old.
    if(vfs_is_ancestor_inode(vfs, old_inode, new_parent))
    {
      vfs_seterror(vfs, EINVAL);
      goto err;
    }

    // If old is a dir, new must be an empty dir if it exists.
    if(new_n)
    {
      if(VFS_INODE_TYPE(new_n) != VFS_INODE_DIR)
      {
        vfs_seterror(vfs, ENOTDIR);
        goto err;
      }
      if(new_n->length > 2)
      {
        vfs_seterror(vfs, EEXIST);
        goto err;
      }
    }
  }
  else
  {
    if(new_n)
    {
      // If old is a file, new must be a file if it exists.
      if(VFS_INODE_TYPE(new_n) == VFS_INODE_DIR)
      {
        vfs_seterror(vfs, EISDIR);
        goto err;
      }
      // New must not have any references if it exists.
      if(new_n->refcount)
      {
        // Might be incorrect, POSIX only mentions dirs.
        vfs_seterror(vfs, EBUSY);
        goto err;
      }
    }
  }

  if(!vfs_elevate_lock(vfs))
    goto err;

  if(new_inode && !vfs_delete_inode(vfs, new_parent, new_inode, buffer2))
    goto err_write;

  if(!vfs_move_inode(vfs, old_parent, new_parent, old_inode, buffer, buffer2))
    goto err_write;

  old_p->modify_time = vfs_get_date();
  new_p->modify_time = old_p->modify_time;
  vfs_write_unlock(vfs);
  return -code;

err_write:
  code = vfs_geterror(vfs);
  vfs_write_unlock(vfs);
  return -code;

err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return -code;
}

/**
 * Remove a virtual file (i.e. not a cached copy of a real file).
 * If this is a cached real file, it should be unlinked separately and
 * invalidated with `vfs_invalidate_at_path`. This function does not work on
 * directories (use `vfs_rmdir` instead).
 *
 * @param vfs   VFS handle.
 * @param path  path of file within `vfs` to remove.
 * @return      0 on success, otherwise a negative number corresponding to the
 *              relevant `errno` code (-255 for unknown error). This function
 *              does not set `errno`.
 */
int vfs_unlink(vfilesystem *vfs, const char *path)
{
  char buffer[MAX_PATH];
  struct vfs_inode *n;
  uint32_t parent;
  uint32_t inode;
  int code = 0;

  if(!vfs_read_lock(vfs))
    return -1;

  if(!vfs_get_inode_and_parent_by_path(vfs, path, &parent, &inode, buffer, sizeof(buffer)))
    goto err;

  // Both must exist and be different.
  if(!parent || !inode)
  {
    vfs_seterror(vfs, ENOENT);
    goto err;
  }
  if(parent == inode)
  {
    vfs_seterror(vfs, EBUSY);
    goto err;
  }

  // This function is not applicable to cached files.
  n = vfs_get_inode_ptr(vfs, inode);
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

  if(!vfs_elevate_lock(vfs))
    goto err;

  if(vfs_delete_inode(vfs, parent, inode, buffer))
  {
    struct vfs_inode *p = vfs_get_inode_ptr(vfs, parent);
    p->modify_time = vfs_get_date();
  }
  else
    code = vfs_geterror(vfs);

  vfs_write_unlock(vfs);
  return -code;

err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return -code;
}

/**
 * Remove an empty virtual directory (i.e. not a cached directory).
 * If this is a cached directory, it should be removed separately and
 * invalidated with `vfs_invalidate_at_path`.
 *
 * @param vfs   VFS handle.
 * @param path  path of directory within `vfs` to remove.
 * @return      0 on success, otherwise a negative number corresponding to the
 *              relevant `errno` code (-255 for unknown error). This function
 *              does not set `errno`.
 */
int vfs_rmdir(vfilesystem *vfs, const char *path)
{
  char buffer[MAX_PATH];
  struct vfs_inode *n;
  uint32_t parent;
  uint32_t inode;
  int code = 0;

  if(!vfs_read_lock(vfs))
    return -1;

  if(!vfs_get_inode_and_parent_by_path(vfs, path, &parent, &inode, buffer, sizeof(buffer)))
    goto err;

  // Both must exist and be different.
  if(!parent || !inode)
  {
    vfs_seterror(vfs, ENOENT);
    goto err;
  }
  if(parent == inode)
  {
    vfs_seterror(vfs, EBUSY);
    goto err;
  }

  // This function is not applicable to cached files.
  n = vfs_get_inode_ptr(vfs, inode);
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

  if(!vfs_elevate_lock(vfs))
    goto err;

  if(vfs_delete_inode(vfs, parent, inode, buffer))
  {
    struct vfs_inode *p = vfs_get_inode_ptr(vfs, parent);
    p->modify_time = vfs_get_date();
  }
  else
    code = vfs_geterror(vfs);

  vfs_write_unlock(vfs);
  return -code;

err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return -code;
}

/**
 * Get access permissions for a virtual file or directory (i.e. not cached).
 * If this is a cached file or directory, it should be checked with `access`.
 * Currently, the VFS system doesn't bother with permissions, so this works
 * as long as the file exists and isn't cached.
 *
 * @param vfs   VFS handle.
 * @param path  path within `vfs` to query.
 * @param mode  permissions to query (ignored; always treated as `F_OK`).
 * @return      0 on success, otherwise a negative number corresponding to the
 *              relevant `errno` code (-255 for unknown error). This function
 *              does not set `errno`.
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
 * @param vfs   VFS handle.
 * @param path  path within `vfs` to query.
 * @param st    destination to store queried `stat` data to.
 * @return      0 on success, otherwise a negative number corresponding to the
 *              relevant `errno` code (-255 for unknown error). This function
 *              does not set `errno`.
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

  n = vfs_get_inode_ptr(vfs, inode);
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
  st->st_dev = (dev_t)(('M'<<24) | ('Z'<<16) | ('X'<<8) | ('V'));
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

/**
 * Get a list of virtual files present in a directory.
 * The list of virtual files will be allocated to `struct vfs_dir *d`.
 *
 * @param vfs   VFS handle.
 * @param path  path within `vfs` to query.
 * @param d     destination to store queried directory data to.
 * @return      0 on success, otherwise a negative number corresponding to the
 *              relevant `errno` code (-255 for unknown error). This function
 *              does not set `errno`.
 */
int vfs_readdir(vfilesystem *vfs, const char *path, struct vfs_dir *d)
{
  struct vfs_dir_file **files;
  struct vfs_inode *p;
  uint32_t *inodes;
  uint32_t inode;
  size_t num_alloc;
  size_t num_files;
  size_t i;
  int code;

  if(!vfs_read_lock(vfs))
    return -1;

  inode = vfs_get_inode_by_path(vfs, path);
  if(!inode)
    goto err;

  p = vfs_get_inode_ptr(vfs, inode);
  if(!p)
    goto err;

  // This function is only applicable to files.
  if(VFS_INODE_TYPE(p) != VFS_INODE_DIR)
  {
    vfs_seterror(vfs, ENOTDIR);
    goto err;
  }

  files = NULL;
  num_files = 0;
  num_alloc = 0;
  inodes = p->contents.inodes;

  for(i = 2; i < p->length; i++)
  {
    struct vfs_dir_file *f;
    struct vfs_inode *n;
    const char *name;
    size_t len;
    int type;
    inode = inodes[i];

    // Ignore cached files; the caller should be using dirent for them.
    n = vfs_get_inode_ptr(vfs, inode);
    if(!n || VFS_IS_CACHED(n))
      continue;

    if(num_files >= num_alloc)
    {
      struct vfs_dir_file **t;
      num_alloc = num_alloc ? num_alloc << 1 : 4;

      t = (struct vfs_dir_file **)realloc(files, num_alloc * sizeof(struct vfs_dir_file *));
      if(!t)
      {
        vfs_seterror(vfs, ENOMEM);
        goto err_free;
      }
      files = t;
    }

    name = vfs_inode_name(n);
    len = n->name_length;
    switch(VFS_INODE_TYPE(n))
    {
      case VFS_INODE_DIR:
        type = DIR_TYPE_DIR;
        break;
      case VFS_INODE_FILE:
        type = DIR_TYPE_FILE;
        break;
      default:
        type = DIR_TYPE_UNKNOWN;
        break;
    }

    f = (struct vfs_dir_file *)malloc(sizeof(struct vfs_dir_file) + len);
    if(!f)
    {
      vfs_seterror(vfs, ENOMEM);
      goto err_free;
    }

    f->type = type;
    memcpy(f->name, name, len + 1);

    files[num_files++] = f;
  }

  if(num_files < num_alloc)
  {
    struct vfs_dir_file **t = (struct vfs_dir_file **)realloc(files,
     num_files * sizeof(struct vfs_dir_file *));
    if(t)
      files = t;
  }

  d->files = files;
  d->num_files = num_files;

  vfs_read_unlock(vfs);
  return 0;

err_free:
  if(files)
  {
    for(i = 0; i < num_files; i++)
      free(files[i]);
    free(files);
  }
err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return -code;
}

/**
 * Free memory allocated to a `struct vfs_dir` by `vfs_readdir`.
 *
 * @param d   `vfs_dir` struct to free data from.
 * @return    0 on success, otherwise a negative value.
 */
int vfs_readdir_free(struct vfs_dir *d)
{
  size_t i;
  if(d->files)
  {
    for(i = 0; i < d->num_files; i++)
      free(d->files[i]);

    free(d->files);
    d->files = NULL;
  }
  return 0;
}

#endif /* VIRTUAL_FILESYSTEM */

/**
 * Allocate and initialize a VFS.
 * (This function is enabled even when VIRTUAL_FILESYSTEM is not defined so
 * -pedantic doesn't complain about ISO C not allowing empty compilation units.)
 *
 * @return a `vfilesystem` handle on success, otherwise `NULL`. If VFS support
 *         is disabled, this function will always return `NULL`.
 */
vfilesystem *vfs_init(void)
{
#ifdef VIRTUAL_FILESYSTEM
  vfilesystem *vfs = (vfilesystem *)malloc(sizeof(vfilesystem));
  if(vfs)
  {
    if(vfs_setup(vfs))
      return vfs;

    vfs_clear(vfs);
    free(vfs);
  }
#endif
  return NULL;
}

/**
 * Free a VFS handle created by `vfs_init`.
 *
 * @param vfs   VFS handle to free.
 */
void vfs_free(vfilesystem *vfs)
{
#ifdef VIRTUAL_FILESYSTEM
  vfs_clear(vfs);
  free(vfs);
#endif
}
