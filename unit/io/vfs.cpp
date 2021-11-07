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

#include "../Unit.hpp"

#include "../../src/io/path.h"
#include "../../src/io/vfs.h"
#include "../../src/io/vio.h"

#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

static constexpr dev_t MZX_DEVICE = ('M'<<24) | ('Z'<<16) | ('X'<<8) | ('V');
static constexpr size_t GETCWD_BUF = 32;
static constexpr int ignore = -10000;

class ScopedVFS
{
  vfilesystem *vfs;

public:
  ScopedVFS(vfilesystem *p = nullptr) : vfs(p) {}
  ~ScopedVFS() { if(vfs) vfs_free(vfs); }

  operator vfilesystem *() { return vfs; }
};

class ScopedVFSDir : public vfs_dir
{
public:
  ScopedVFSDir()
  {
    files = nullptr;
    num_files = 0;
  }

  ~ScopedVFSDir()
  {
    vfs_readdir_free(this);
  }
};

struct vfs_result
{
  const char *path;
  int expected_ret;
};

enum op
{
  DO_STAT,
  DO_CHDIR,
  DO_GETCWD,
  DO_CREATE,
  DO_MKDIR,
  DO_RENAME,
  DO_UNLINK,
  DO_RMDIR,
  DO_ACCESS,
} op;

struct vfs_stat_data
{
  ino_t inode;
  mode_t filetype;
  off_t size;
};

struct vfs_stat_result
{
  const char *path;
  int expected_ret;
  vfs_stat_data st;
};

struct vfs_op_result
{
  const char *path;
  const char *path2;
  enum op op;
  int expected_ret;
  int stat_ret;
  vfs_stat_data st;
};

static void check_stat(const char *path, const vfs_stat_data &d, struct stat &st,
 time_t create_time = -1, time_t modify_time = -1)
{
  ASSERTEQ(st.st_dev, MZX_DEVICE, "%s", path);
  ASSERTEQ(st.st_ino, d.inode, "%s", path);
  ASSERTEQ(st.st_mode & S_IFMT, d.filetype, "%s", path);
  ASSERTEQ(st.st_size, d.size, "%s", path);
  ASSERT(st.st_atime, "%s", path);
  ASSERT(st.st_mtime, "%s", path);
  ASSERT(st.st_ctime, "%s", path);
  ASSERT(st.st_atime >= create_time, "%s", path);
  ASSERT(st.st_mtime >= modify_time, "%s", path);
  ASSERT(st.st_ctime >= modify_time, "%s", path);
}

static void do_op_and_stat(vfilesystem *vfs, const vfs_op_result &d,
 time_t create_time = -1, time_t modify_time = -1)
{
  const char *st_path = d.path;
  int ret;
  switch(d.op)
  {
    case DO_STAT:
      break;
    case DO_CHDIR:
      st_path = ".";
      ret = vfs_chdir(vfs, d.path);
      ASSERTEQ(ret, d.expected_ret, "chdir: %s", d.path);
      break;
    case DO_GETCWD:
      char buffer[GETCWD_BUF];
      ret = vfs_getcwd(vfs, buffer, sizeof(buffer));
      ASSERTEQ(ret, d.expected_ret, "getcwd: %s", d.path);
      if(!ret)
        ASSERTCMP(buffer, d.path, "getcwd");
      break;
    case DO_CREATE:
      ret = vfs_create_file_at_path(vfs, d.path);
      ASSERTEQ(ret, d.expected_ret, "create: %s", d.path);
      break;
    case DO_MKDIR:
      ret = vfs_mkdir(vfs, d.path, 0777);
      ASSERTEQ(ret, d.expected_ret, "mkdir: %s", d.path);
      break;
    case DO_RENAME:
      st_path = d.path2;
      ret = vfs_rename(vfs, d.path, d.path2);
      ASSERTEQ(ret, d.expected_ret, "rename: %s -> %s", d.path, d.path2);
      break;
    case DO_UNLINK:
      ret = vfs_unlink(vfs, d.path);
      ASSERTEQ(ret, d.expected_ret, "unlink: %s", d.path);
      break;
    case DO_RMDIR:
      ret = vfs_rmdir(vfs, d.path);
      ASSERTEQ(ret, d.expected_ret, "rmdir: %s", d.path);
      break;
    case DO_ACCESS:
      ret = vfs_access(vfs, d.path, R_OK);
      ASSERTEQ(ret, d.expected_ret, "access: %s", d.path);
      break;
  }

  struct stat st;
  ret = vfs_stat(vfs, st_path, &st);
  ASSERTEQ(ret, d.stat_ret, "%s", d.path);
  if(!ret)
    check_stat(d.path, d.st, st, create_time, modify_time);
}

UNITTEST(vfs_stat)
{
  // Simple test to make sure it works on known valid/invalid default files.
  // This function will be further tested in other tests that use it to fetch
  // info to verify other calls worked.

#ifndef CONFIG_VFS
  SKIP();
#endif

  static const vfs_stat_result valid_data[]
  {
    // These are all the default root since that's the only default file...
    { "..", 0,        { 1, S_IFDIR, 0 }},
    { ".", 0,         { 1, S_IFDIR, 0 }},
    { "./", 0,        { 1, S_IFDIR, 0 }},
    { "/", 0,         { 1, S_IFDIR, 0 }},
    { "/./../", 0,    { 1, S_IFDIR, 0 }},
    { "\\", 0,        { 1, S_IFDIR, 0 }},
    { "\\..\\.", 0,   { 1, S_IFDIR, 0 }},
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
    { "C:/", 0,       { 1, S_IFDIR, 0 }},
    { "C:\\", 0,      { 1, S_IFDIR, 0 }},
    { "c:\\", 0,      { 1, S_IFDIR, 0 }},
#endif
  };

  static const vfs_result invalid_data[]
  {
    { "", -ENOENT },
    { "jsdjfk", -ENOENT },
    { "sdcard:/", -ENOENT },
    { "verylongrootnamewhywouldyouevennamearootthis:/", -ENOENT },
    { "D:\\", -ENOENT },
  };

  ScopedVFS vfs = vfs_init();
  struct stat st;
  ASSERT(vfs, "");

  SECTION(Valid)
  {
    for(const vfs_stat_result &d : valid_data)
    {
      int ret = vfs_stat(vfs, d.path, &st);
      ASSERTEQ(ret, d.expected_ret, "%s", d.path);
      check_stat(d.path, d.st, st);
    }
  }

  SECTION(Invalid)
  {
    for(const vfs_result &d : invalid_data)
    {
      int ret = vfs_stat(vfs, d.path, &st);
      ASSERTEQ(ret, d.expected_ret, "%s", d.path);
    }
  }
}

UNITTEST(vfs_create_file_at_path)
{
#ifndef CONFIG_VFS
  SKIP();
#endif

  static const vfs_stat_result valid_data[] =
  {
    { "file.txt", 0,                            { 2, S_IFREG, 0 }},
    { "abc.def", 0,                             { 3, S_IFREG, 0 }},
    { "reallylongfilename.reallylongext", 0,    { 4, S_IFREG, 0 }},
    { "../initrd.img", 0,                       { 5, S_IFREG, 0 }},
    { "é", 0,                                   { 6, S_IFREG, 0 }},
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
    { "C:/dkdfjklsjdf", 0,                      { 7, S_IFREG, 0 }},
#endif
  };

  static const vfs_result invalid_data[] =
  {
    { "file.txt",           0 },
    { "file.txt",           -EEXIST },
    { "file.txt/file.txt",  -ENOTDIR },
    { "/",                  -EISDIR },
    { "abchnkdf/file.txt",  -ENOENT },
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
    { "c:\\",               -EISDIR },
#endif
  };

  ScopedVFS vfs = vfs_init();
  struct stat st;
  ASSERT(vfs, "");

  time_t init_time = 0;
  if(!vfs_stat(vfs, "/", &st))
    init_time = st.st_mtime;

  SECTION(Valid)
  {
    for(const vfs_stat_result &d : valid_data)
    {
      int ret = vfs_create_file_at_path(vfs, d.path);
      ASSERTEQ(ret, d.expected_ret, "%s", d.path);

      ret = vfs_stat(vfs, d.path, &st);
      ASSERTEQ(ret, 0, "stat %s", d.path);
      check_stat(d.path, d.st, st, init_time, init_time);
    }
  }

  SECTION(Invalid)
  {
    for(const vfs_result &d : invalid_data)
    {
      int ret = vfs_create_file_at_path(vfs, d.path);
      ASSERTEQ(ret, d.expected_ret, "%s", d.path);
    }
  }
}

UNITTEST(vfs_mkdir)
{
#ifndef CONFIG_VFS
  SKIP();
#endif

  static const vfs_stat_result valid_data[] =
  {
    { "aaa", 0,                         { 2, S_IFDIR, 0 }},
    { "./aaa/b", 0,                     { 3, S_IFDIR, 0 }},
    { "/ccc", 0,                        { 4, S_IFDIR, 0 }},
    { "aaa/..\\ccc/d", 0,               { 5, S_IFDIR, 0 }},
    { "ccc/d\\.././..\\aaa/b\\e/", 0,   { 6, S_IFDIR, 0 }},
    { "0", 0,                           { 7, S_IFDIR, 0 }}, // Test inserting before existing.
    { "1", 0,                           { 8, S_IFDIR, 0 }},
    { "á", 0,                           { 9, S_IFDIR, 0 }},
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
    { "c:/fff/", 0,                     { 10, S_IFDIR, 0 }},
#endif
  };

  static const vfs_result invalid_data[] =
  {
    { "",               -ENOENT },
    { "aaa",            0 },
    { "aaa",            -EEXIST },
    { "wtf/a",          -ENOENT },
    { "/",              -EEXIST },
    { "D:\\",           -ENOENT },
    { "sdcardsfdfds:/", -ENOENT },
    { "file.txt/abc",   -ENOTDIR },
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
    { "C:\\",           -EEXIST },
#endif
  };

  ScopedVFS vfs = vfs_init();
  struct stat st;
  ASSERT(vfs, "");

  time_t init_time = 0;
  if(!vfs_stat(vfs, "/", &st))
    init_time = st.st_mtime;

  SECTION(Valid)
  {
    for(const vfs_stat_result &d : valid_data)
    {
      int ret = vfs_mkdir(vfs, d.path, 0755);
      ASSERTEQ(ret, d.expected_ret, "%s", d.path);

      ret = vfs_stat(vfs, d.path, &st);
      ASSERTEQ(ret, 0, "stat %s", d.path);
      check_stat(d.path, d.st, st, init_time, init_time);
    }
  }

  SECTION(Invalid)
  {
    int r = vfs_create_file_at_path(vfs, "file.txt");
    ASSERTEQ(r, 0, "");

    for(const vfs_result &d : invalid_data)
    {
      int ret = vfs_mkdir(vfs, d.path, 0755);
      ASSERTEQ(ret, d.expected_ret, "%s", d.path);
    }
  }
}

UNITTEST(vfs_chdir_getcwd)
{
#ifndef CONFIG_VFS
  SKIP();
#endif

#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
#define BASE "C:\\"
#else
#define BASE "/"
#endif
#define NAME32 "0123456789.0123456789.0123456789"
#define NAME128 NAME32 "." NAME32 "." NAME32 "." NAME32
#define NAME512 NAME128 "." NAME128 "." NAME128 "." NAME128
  static const vfs_op_result valid_data[] =
  {
    { BASE, "", DO_GETCWD, 0,             0,        { 1, S_IFDIR, 0 }},
    { ".", "", DO_CHDIR, 0,               0,        { 1, S_IFDIR, 0 }},
    { "dir", "", DO_MKDIR, 0,             0,        { 2, S_IFDIR, 0 }},
    { "file", "", DO_CREATE, 0,           0,        { 3, S_IFREG, 0 }},
    { "dir", "", DO_CHDIR, 0,             0,        { 2, S_IFDIR, 0 }},
    { BASE "dir", "", DO_GETCWD, 0,       0,        { 2, S_IFDIR, 0 }},
    { ".", "", DO_STAT, ignore,           0,        { 2, S_IFDIR, 0 }},
    { "..", "", DO_STAT, ignore,          0,        { 1, S_IFDIR, 0 }},
    { "../dir", "", DO_STAT, ignore,      0,        { 2, S_IFDIR, 0 }},
    { "../file", "", DO_STAT, ignore,     0,        { 3, S_IFREG, 0 }},
    { "dir2", "", DO_MKDIR, 0,            0,        { 4, S_IFDIR, 0 }},
    { "file2", "", DO_CREATE, 0,          0,        { 5, S_IFREG, 0 }},
    { "/dir/dir2", "", DO_STAT, ignore,   0,        { 4, S_IFDIR, 0 }},
    { "/dir/file2", "", DO_STAT, ignore,  0,        { 5, S_IFREG, 0 }},
    { "dir2", "", DO_CHDIR, 0,            0,        { 4, S_IFDIR, 0 }},
    { BASE "dir" DIR_SEPARATOR "dir2", "", DO_GETCWD, 0, 0, { 4, S_IFDIR, 0 }},
    { "../../file", "", DO_STAT, ignore,  0,        { 3, S_IFREG, 0 }},
    { "/", "", DO_CHDIR, 0,               0,        { 1, S_IFDIR, 0 }},
    { BASE, "", DO_GETCWD, 0,             0,        { 1, S_IFDIR, 0 }},
  };

  static const vfs_op_result invalid_data[] =
  {
    { "dir", "", DO_MKDIR, 0,             0,        { 2, S_IFDIR, 0 }},
    { "file", "", DO_CREATE, 0,           0,        { 3, S_IFREG, 0 }},

    // Invalid chdir.
    { "", "", DO_CHDIR, -ENOENT,          0,        { 1, S_IFDIR, 0 }},
    { "/nodir", "", DO_CHDIR, -ENOENT,    0,        { 1, S_IFDIR, 0 }},
    { "/nodir/a", "", DO_CHDIR, -ENOENT,  0,        { 1, S_IFDIR, 0 }},
    { "file", "", DO_CHDIR, -ENOTDIR,     0,        { 1, S_IFDIR, 0 }},
    { "file/dir", "", DO_CHDIR, -ENOTDIR, 0,        { 1, S_IFDIR, 0 }},
    // The above shouldn't change the cwd...
    { BASE, "", DO_GETCWD, 0,             0,        { 1, S_IFDIR, 0 }},
    // Path too big for chdir (512).
    { NAME512, "", DO_MKDIR, 0,           0,        { 4, S_IFDIR, 0 }},
    { BASE NAME512, "", DO_CHDIR, -ENAMETOOLONG, 0, { 1, S_IFDIR, 0 }},
    // Path too big for small getcwd buffer (see GETCWD_BUF).
    { NAME32, "", DO_MKDIR, 0,            0,        { 5, S_IFDIR, 0 }},
    { NAME32, "", DO_CHDIR, 0,            0,        { 5, S_IFDIR, 0 }},
    { "", "", DO_GETCWD, -ERANGE,         -ENOENT,  {}},
  };

  ScopedVFS vfs = vfs_init();
  struct stat st;
  ASSERT(vfs, "");

  time_t init_time = 0;
  if(!vfs_stat(vfs, "/", &st))
    init_time = st.st_mtime;

  SECTION(Valid)
  {
    for(const vfs_op_result &d : valid_data)
      do_op_and_stat(vfs, d, init_time, init_time);
  }

  SECTION(Invalid)
  {
    for(const vfs_op_result &d : invalid_data)
      do_op_and_stat(vfs, d, init_time, init_time);
  }

  SECTION(getcwdInvalid)
  {
    char buffer[1];
    int ret;
    ret = vfs_getcwd(vfs, nullptr, 1235);
    ASSERTEQ(ret, -EINVAL, "");
    ret = vfs_getcwd(vfs, buffer, 0);
    ASSERTEQ(ret, -EINVAL, "");
  }
}

UNITTEST(vfs_rename)
{
#ifndef CONFIG_VFS
  SKIP();
#endif

  static const vfs_op_result valid_data[] =
  {
    // No overwrite.
    { "file1", "", DO_CREATE, 0,                0,        { 2, S_IFREG, 0 }},
    { "file1", "file2", DO_RENAME, 0,           0,        { 2, S_IFREG, 0 }},
    { "file1", "", DO_STAT, ignore,             -ENOENT,  {}},
    { "dir1", "", DO_MKDIR, 0,                  0,        { 3, S_IFDIR, 0 }},
    { "dir1", "dir2", DO_RENAME, 0,             0,        { 3, S_IFDIR, 0 }},
    { "file2", "dir2/file", DO_RENAME, 0,       0,        { 2, S_IFREG, 0 }},
    { "file2", "", DO_STAT, ignore,             -ENOENT,  {}},
    { "dir2", "dir3", DO_RENAME, 0,             0,        { 3, S_IFDIR, 0 }},
    { "dir2", "", DO_STAT, ignore,              -ENOENT,  {}},
    { "dir3/file", "", DO_STAT, ignore,         0,        { 2, S_IFREG, 0 }},
    { "dir4", "", DO_MKDIR, 0,                  0,        { 4, S_IFDIR, 0 }},
    { "dir3", "dir4/dir3", DO_RENAME, 0,        0,        { 3, S_IFDIR, 0 }},
    { "dir3", "", DO_STAT, ignore,              -ENOENT,  {}},
    { "dir4/dir3/file", "", DO_STAT, ignore,    0,        { 2, S_IFREG, 0 }},
    // Overwrite file->file.
    { "file", "", DO_CREATE, 0,                 0,        { 5, S_IFREG, 0 }},
    { "dir4/dir3/file", "file", DO_RENAME, 0,   0,        { 2, S_IFREG, 0 }},
    { "dir4/dir3/file", "", DO_STAT, ignore,    -ENOENT,  {}},
    { "file2", "", DO_CREATE, 0,                0,        { 5, S_IFREG, 0 }},
    // Overwrite dir->dir.
    { "dir", "", DO_MKDIR, 0,                   0,        { 6, S_IFDIR, 0 }},
    { "dir4/dir3", "dir", DO_RENAME, 0,         0,        { 3, S_IFDIR, 0 }},
    { "dir4/dir3", "", DO_STAT, ignore,         -ENOENT,  {}},
    { "dir2", "", DO_MKDIR, 0,                  0,        { 6, S_IFDIR, 0 }},
    // Same old/new should return success but change nothing.
    { "dir", "dir", DO_RENAME, 0,               0,        { 3, S_IFDIR, 0 }},
    { "file", "file", DO_RENAME, 0,             0,        { 2, S_IFREG, 0 }},
  };

  static const vfs_op_result invalid_data[] =
  {
    { "file", "", DO_CREATE, 0,                   0,        { 2, S_IFREG, 0 }},
    { "dir", "", DO_MKDIR, 0,                     0,        { 3, S_IFDIR, 0 }},
    { "dir2", "", DO_MKDIR, 0,                    0,        { 4, S_IFDIR, 0 }},
    { "dir2/dir3", "", DO_MKDIR, 0,               0,        { 5, S_IFDIR, 0 }},

    { "", "badsf", DO_RENAME, -ENOENT,            -ENOENT,  {}},
    { "badsf", "", DO_RENAME, -ENOENT,            -ENOENT,  {}},
    { "/", "asdf", DO_RENAME, -EBUSY,             -ENOENT,  {}},
    { "dir", "/",  DO_RENAME, -EBUSY,             0,        { 1, S_IFDIR, 0 }},
    { "dir", "..", DO_RENAME, -EBUSY,             0,        { 1, S_IFDIR, 0 }},
    { "/noexist", "dir", DO_RENAME, -ENOENT,      0,        { 3, S_IFDIR, 0 }},
    { "file", "/nodir/file", DO_RENAME, -ENOENT,  -ENOENT,  {}},
    { "dir", "file/dir", DO_RENAME, -ENOTDIR,     -ENOTDIR, {}},

    // Overwrite dir->file.
    { "dir", "file", DO_RENAME, -ENOTDIR,         0,        { 2, S_IFREG, 0 }},
    // Overwrite file->dir.
    { "file", "dir", DO_RENAME, -EISDIR,          0,        { 3, S_IFDIR, 0 }},
    // Overwrite non-empty dir.
    { "dir", "dir2", DO_RENAME, -EEXIST,          0,        { 4, S_IFDIR, 0 }},
    // Old dir must not be ancestor of new dir.
    { "dir", "dir/dir", DO_RENAME, -EINVAL,       -ENOENT, {}},
    { "dir2", "dir2/dir3/dir4", DO_RENAME, -EINVAL, -ENOENT, {}},
  };

  ScopedVFS vfs = vfs_init();
  struct stat st;
  ASSERT(vfs, "");

  time_t init_time = 0;
  if(!vfs_stat(vfs, "/", &st))
    init_time = st.st_mtime;

  SECTION(Valid)
  {
    for(const vfs_op_result &d : valid_data)
      do_op_and_stat(vfs, d, init_time, init_time);
  }

  SECTION(Invalid)
  {
    for(const vfs_op_result &d : invalid_data)
      do_op_and_stat(vfs, d, init_time, init_time);
  }

  SECTION(BusyFile)
  {
    // Special: attempting to overwrite an open file should always fail with EBUSY.
    uint32_t inode;
    int ret;

    ret = vfs_create_file_at_path(vfs, "file");
    ASSERTEQ(ret, 0, "");
    ret = vfs_create_file_at_path(vfs, "file2");
    ASSERTEQ(ret, 0, "");
    ret = vfs_open_if_exists(vfs, "file", true, &inode);
    ASSERTEQ(ret, 0, "");
    ret = vfs_rename(vfs, "file2", "file");
    ASSERTEQ(ret, -EBUSY, "");
    ret = vfs_close(vfs, inode);
    ASSERTEQ(ret, 0, "");
    ret = vfs_rename(vfs, "file2", "file");
    ASSERTEQ(ret, 0, "");
  }
}

UNITTEST(vfs_unlink)
{
#ifndef CONFIG_VFS
  SKIP();
#endif

  static const vfs_op_result valid_data[] =
  {
    { "file1", "", DO_CREATE, 0,              0,        { 2, S_IFREG, 0 }},
    { "file1", "", DO_UNLINK, 0,              -ENOENT,  {}},
    { "dir", "", DO_MKDIR, 0,                 0,        { 2, S_IFDIR, 0 }},
    { "dir/file2", "", DO_CREATE, 0,          0,        { 3, S_IFREG, 0 }},
    { "dir/file2", "", DO_UNLINK, 0,          -ENOENT,  {}},
    { "file3", "", DO_CREATE, 0,              0,        { 3, S_IFREG, 0 }},
  };

  static const vfs_op_result invalid_data[] =
  {
    { "dir", "", DO_MKDIR, 0,                 0,        { 2, S_IFDIR, 0 }},
    { "file", "", DO_CREATE, 0,               0,        { 3, S_IFREG, 0 }},

    { "", "", DO_UNLINK, -ENOENT,             -ENOENT,  {}},
    { "/", "", DO_UNLINK, -EBUSY,             0,        { 1, S_IFDIR, 0 }},
    { "/noexist", "", DO_UNLINK, -ENOENT,     -ENOENT,  {}},
    { "/nodir/a", "", DO_UNLINK, -ENOENT,     -ENOENT,  {}},
    { "file/file", "", DO_UNLINK, -ENOTDIR,   -ENOTDIR, {}},
    { "dir", "", DO_UNLINK, -EPERM,           0,        { 2, S_IFDIR, 0 }},
  };

  ScopedVFS vfs = vfs_init();
  struct stat st;
  ASSERT(vfs, "");

  time_t init_time = 0;
  if(!vfs_stat(vfs, "/", &st))
    init_time = st.st_mtime;

  SECTION(Valid)
  {
    for(const vfs_op_result &d : valid_data)
      do_op_and_stat(vfs, d, init_time, init_time);
  }

  SECTION(Invalid)
  {
    for(const vfs_op_result &d : invalid_data)
      do_op_and_stat(vfs, d, init_time, init_time);
  }

  SECTION(BusyFile)
  {
    // Special: attempting to unlink an open file should always fail with EBUSY.
    uint32_t inode;
    int ret;

    ret = vfs_create_file_at_path(vfs, "file");
    ASSERTEQ(ret, 0, "");
    ret = vfs_open_if_exists(vfs, "file", true, &inode);
    ASSERTEQ(ret, 0, "");
    ret = vfs_unlink(vfs, "file");
    ASSERTEQ(ret, -EBUSY, "");
    ret = vfs_close(vfs, inode);
    ASSERTEQ(ret, 0, "");
    ret = vfs_unlink(vfs, "file");
    ASSERTEQ(ret, 0, "");
  }
}

UNITTEST(vfs_rmdir)
{
#ifndef CONFIG_VFS
  SKIP();
#endif

  static const vfs_op_result valid_data[] =
  {
    { "file1", "", DO_CREATE, 0,              0,        { 2, S_IFREG, 0 }},
    { "dir", "", DO_MKDIR, 0,                 0,        { 3, S_IFDIR, 0 }},
    { "dir", "", DO_RMDIR, 0,                 -ENOENT,  {}},
    { "dir", "", DO_MKDIR, 0,                 0,        { 3, S_IFDIR, 0 }},
    { "dir/dir2", "", DO_MKDIR, 0,            0,        { 4, S_IFDIR, 0 }},
    { "dir", "", DO_RMDIR, -ENOTEMPTY,        0,        { 3, S_IFDIR, 0 }},
    { "dir/dir2", "", DO_RMDIR, 0,            -ENOENT,  {}},
    { "dir", "", DO_RMDIR, 0,                 -ENOENT,  {}},
  };

  static const vfs_op_result invalid_data[] =
  {
    { "file", "", DO_CREATE, 0,               0,        { 2, S_IFREG, 0 }},

    { "", "", DO_RMDIR, -ENOENT,              -ENOENT,  {}},
    { "/", "", DO_RMDIR, -EBUSY,              0,        { 1, S_IFDIR, 0 }},
    { "file", "", DO_RMDIR, -ENOTDIR,         0,        { 2, S_IFREG, 0 }},
    { "/noexist", "", DO_RMDIR, -ENOENT,      -ENOENT,  {}},
    { "/nodir/a", "", DO_RMDIR, -ENOENT,      -ENOENT,  {}},
    { "file/dir", "", DO_RMDIR, -ENOTDIR,     -ENOTDIR, {}},
  };

  ScopedVFS vfs = vfs_init();
  struct stat st;
  ASSERT(vfs, "");

  time_t init_time = 0;
  if(!vfs_stat(vfs, "/", &st))
    init_time = st.st_mtime;

  SECTION(Valid)
  {
    for(const vfs_op_result &d : valid_data)
      do_op_and_stat(vfs, d, init_time, init_time);
  }

  SECTION(Invalid)
  {
    for(const vfs_op_result &d : invalid_data)
      do_op_and_stat(vfs, d, init_time, init_time);
  }
}

UNITTEST(vfs_access)
{
#ifndef CONFIG_VFS
  SKIP();
#endif

  static const vfs_op_result valid_data[] =
  {
    { "/", "", DO_ACCESS, 0,                  0,        { 1, S_IFDIR, 0 }},
    { "file", "", DO_CREATE, 0,               0,        { 2, S_IFREG, 0 }},
    { "file", "", DO_ACCESS, 0,               0,        { 2, S_IFREG, 0 }},
    { "dir", "", DO_MKDIR, 0,                 0,        { 3, S_IFDIR, 0 }},
    { "dir", "", DO_ACCESS, 0,                0,        { 3, S_IFDIR, 0 }},
  };

  static const vfs_op_result invalid_data[] =
  {
    { "file", "", DO_CREATE, 0,               0,        { 2, S_IFREG, 0 }},

    { "", "", DO_RMDIR, -ENOENT,              -ENOENT,  {}},
    { "/noexist", "", DO_RMDIR, -ENOENT,      -ENOENT,  {}},
    { "/nodir/a", "", DO_RMDIR, -ENOENT,      -ENOENT,  {}},
    { "file/file", "", DO_RMDIR, -ENOTDIR,    -ENOTDIR, {}},
  };

  ScopedVFS vfs = vfs_init();
  struct stat st;
  ASSERT(vfs, "");

  time_t init_time = 0;
  if(!vfs_stat(vfs, "/", &st))
    init_time = st.st_mtime;

  SECTION(Valid)
  {
    for(const vfs_op_result &d : valid_data)
      do_op_and_stat(vfs, d, init_time, init_time);
  }

  SECTION(Invalid)
  {
    for(const vfs_op_result &d : invalid_data)
      do_op_and_stat(vfs, d, init_time, init_time);
  }
}

UNITTEST(vfs_readdir)
{
#ifndef CONFIG_VFS
  SKIP();
#endif

  struct vfs_readdir_result
  {
    const char *path;
    boolean is_dir;
  };

  // Keep this sorted.
  static const vfs_readdir_result valid_files[] =
  {
    { "!", true },
    { "0", false },
    { "9123125", false },
    { "@", true },
#ifndef VIRTUAL_FILESYSTEM_CASE_INSENSITIVE
    { "AaaaaAAa", true },
    { "X", true },
    { "Z", false },
#endif
    { "[", true },
    { "afdsf", false },
    { "b5455ttyhn", false },
    { "zzzzzzzz", true },
    { "~", false },
  };

  static const vfs_result invalid_data[] =
  {
    { "", -ENOENT },
    { "file", -ENOTDIR },
    { "file/dir", -ENOTDIR },
  };

  ScopedVFS vfs = vfs_init();
  ASSERT(vfs, "");

  ScopedVFSDir vfsd;
  int ret;

  SECTION(ValidEmpty)
  {
    ret = vfs_readdir(vfs, ".", &vfsd);
    ASSERTEQ(ret, 0, "readdir");
    ASSERTEQ(vfsd.files, nullptr, "");
    ASSERTEQ(vfsd.num_files, 0, "");
  }

  SECTION(ValidFiles)
  {
    for(const vfs_readdir_result &d : valid_files)
    {
      if(d.is_dir)
      {
        ret = vfs_mkdir(vfs, d.path, 0755);
        ASSERTEQ(ret, 0, "mkdir: %s", d.path);
      }
      else
      {
        ret = vfs_create_file_at_path(vfs, d.path);
        ASSERTEQ(ret, 0, "create: %s", d.path);
      }
    }

    ret = vfs_readdir(vfs, ".", &vfsd);
    ASSERTEQ(ret, 0, "readdir");
    ASSERT(vfsd.files, "");
    ASSERTEQ(vfsd.num_files, (size_t)arraysize(valid_files), "");

    size_t i = 0;
    for(const vfs_readdir_result &d : valid_files)
    {
      const struct vfs_dir_file *f = vfsd.files[i++];
      ASSERT(f, "%s", d.path);
      if(d.is_dir)
        ASSERT(f->type == DIR_TYPE_DIR, "%s", d.path);
      else
        ASSERT(f->type == DIR_TYPE_FILE, "%s", d.path);

      ASSERTCMP(f->name, d.path, "");
    }
  }

  SECTION(Invalid)
  {
    ret = vfs_create_file_at_path(vfs, "file");
    ASSERTEQ(ret, 0, "");

    for(const vfs_result &d : invalid_data)
    {
      ret = vfs_readdir(vfs, d.path, &vfsd);
      ASSERTEQ(ret, d.expected_ret, "%s", d.path);
    }
  }

  // Make sure double free is safe (destructor will call again).
  vfs_readdir_free(&vfsd);
}

UNITTEST(FileIO)
{
#ifndef CONFIG_VFS
  SKIP();
#endif

  static const char name[] = "file.ext";
  static const char *strs[] =
  {
    "a test string",
    "",
    "asjdfl;kjsd;lfjksdl;fjksdl;fkjsdl;kfmsdl;kfsd;lmkl;mfsdl;mkdsl;kdsm;l",
  };

  uint32_t inode;
  int ret;

  // Write lock vars.
  unsigned char **_data;
  size_t *_length;
  size_t *_alloc;
  // Read lock vars.
  const unsigned char *data;
  size_t length;

  ScopedVFS vfs = vfs_init();
  struct stat st;
  ASSERT(vfs, "");

  SECTION(Valid)
  {
    ret = vfs_create_file_at_path(vfs, name);
    ASSERTEQ(ret, 0, "create: %s", name);

    ret = vfs_open_if_exists(vfs, name, true, &inode);
    ASSERTEQ(ret, 0, "open(w): %s", name);

    for(const char *str : strs)
    {
      unsigned char *buf = reinterpret_cast<unsigned char *>(strdup(str));
      size_t len = strlen(str);
      ASSERT(buf, "strdup: %s", str);

      // 1. write.
      ret = vfs_lock_file_write(vfs, inode, &_data, &_length, &_alloc);
      ASSERTEQ(ret, 0, "lock(w): %s", str);

      unsigned char *tmp = *_data;
      *_data = buf;
      *_length = len;
      *_alloc = len + 1;

      ret = vfs_unlock_file_write(vfs, inode);
      ASSERTEQ(ret, 0, "unlock(w): %s", str);
      free(tmp);

      // 2. stat, check length.
      ret = vfs_stat(vfs, name, &st);
      ASSERTEQ(ret, 0, "stat: %s", str);
      ASSERTEQ((size_t)st.st_size, len, "length: %s", str);

      // 3. read.
      ret = vfs_lock_file_read(vfs, inode, &data, &length);
      ASSERTEQ(ret, 0, "lock(r): %s", str);

      ASSERTMEM(str, data, len, "compare: %s", str);

      ret = vfs_unlock_file_read(vfs, inode);
      ASSERTEQ(ret, 0, "unlock(r): %s", str);
    }

    ret = vfs_close(vfs, inode);
    ASSERTEQ(ret, 0, "close: %s", name);
  }

  SECTION(Invalid)
  {
    // Opening a nonexistant file should fail.
    ret = vfs_open_if_exists(vfs, "abhjfd", false, &inode);
    ASSERTEQ(ret, -ENOENT, "");

    // Opening a dir should fail.
    ret = vfs_open_if_exists(vfs, "/", false, &inode);
    ASSERTEQ(ret, -EISDIR, "");
  }

  SECTION(InvalidInode)
  {
    // Attempting to lock or unlock a nonexistant file should fail.
    ret = vfs_lock_file_write(vfs, 12345, &_data, &_length, &_alloc);
    ASSERTEQ(ret, -EBADF, "lock(w)");

    ret = vfs_unlock_file_write(vfs, 6523);
    ASSERTEQ(ret, -EBADF, "unlock(w)");

    ret = vfs_lock_file_read(vfs, 12456, &data, &length);
    ASSERTEQ(ret, -EBADF, "lock(r)");

    ret = vfs_unlock_file_read(vfs, 2222);
    ASSERTEQ(ret, -EBADF, "unlock(r)");
  }
}
