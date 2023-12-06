/* MegaZeux
 *
 * Copyright (C) 2021-2023 Alice Rowan <petrifiedrowan@gmail.com>
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
  DO_MAKE_ROOT,
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
  ASSERTEQ(st.st_dev, VFS_MZX_DEVICE, "%s", path);
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
  char buffer[MAX_PATH];
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
      char getcwd_buffer[GETCWD_BUF];
      ret = vfs_getcwd(vfs, getcwd_buffer, sizeof(getcwd_buffer));
      ASSERTEQ(ret, d.expected_ret, "getcwd: %s", d.path);
      if(!ret)
        ASSERTCMP(getcwd_buffer, d.path, "getcwd");
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
    case DO_MAKE_ROOT:
      ret = vfs_make_root(vfs, d.path);
      ASSERTEQ(ret, d.expected_ret, "make_root: %s", d.path);
      snprintf(buffer, MAX_PATH, "%s:/", d.path);
      st_path = buffer;
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

#ifndef VIRTUAL_FILESYSTEM
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

UNITTEST(vfs_make_root)
{
#ifndef VIRTUAL_FILESYSTEM
  SKIP();
#endif

  static const vfs_result valid_data[] =
  {
    { "fat",    0 },
    { "D",      0 },
    { "sdcard", 0 },
  };

  static const vfs_result invalid_data[] =
  {
    { "/",      -EEXIST },
    { "fat",    0 },
    { "fat",    -EEXIST },
    { nullptr,  -EINVAL },
    { "",       -EINVAL },
    { "wowe!",  -EINVAL },
    { "fat:/",  -EINVAL },
    { "D:",     -EINVAL },
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
    { "C",      -EEXIST },
#endif
    //
  };

  ScopedVFS vfs = vfs_init();
  struct stat st;
  ASSERT(vfs, "");

  time_t init_time = 0;
  if(!vfs_stat(vfs, "/", &st))
    init_time = st.st_mtime;

  const auto &do_test = [&vfs, &init_time](const vfs_result &d)
  {
    int ret = vfs_make_root(vfs, d.path);
    ASSERTEQ(ret, d.expected_ret, "%s", d.path);

    if(ret == 0)
    {
      struct stat st;
      char buffer[MAX_PATH];
      snprintf(buffer, MAX_PATH, "%s:/", d.path);
      ret = vfs_stat(vfs, buffer, &st);
      ASSERTEQ(ret, 0, "stat %s", buffer);
      ASSERTEQ(S_ISDIR(st.st_mode), 1, "stat s_isdir %s", buffer);
      ASSERT(st.st_mtime >= init_time, "stat st_mtime %s", buffer);
    }
  };

  SECTION(Valid)
  {
    for(auto &d : valid_data)
      do_test(d);
  }

  SECTION(Invalid)
  {
    for(auto &d : invalid_data)
      do_test(d);
  }
}

UNITTEST(vfs_create_file_at_path)
{
#ifndef VIRTUAL_FILESYSTEM
  SKIP();
#endif

  static const vfs_stat_result valid_data[] =
  {
    { "file.txt", 0,                            { 3, S_IFREG, 0 }},
    { "abc.def", 0,                             { 4, S_IFREG, 0 }},
    { "reallylongfilename.reallylongext", 0,    { 5, S_IFREG, 0 }},
    { "../initrd.img", 0,                       { 6, S_IFREG, 0 }},
    { "é", 0,                                   { 7, S_IFREG, 0 }},
    { "fat:/testfile", 0,                       { 8, S_IFREG, 0 }},
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
    { "C:/dkdfjklsjdf", 0,                      { 9, S_IFREG, 0 }},
#endif
  };

  static const vfs_result invalid_data[] =
  {
    { "file.txt",           0 },
    { "file.txt",           -EEXIST },
    { "file.txt/file.txt",  -ENOTDIR },
    { "/",                  -EISDIR },
    { ".",                  -EISDIR },
    { "..",                 -EISDIR },
    { "./",                 -EISDIR },
    { "",                   -ENOENT },
    { "abchnkdf/file.txt",  -ENOENT },
    { "fat:\\",             -EISDIR },
    { "sdcard:/testfile",   -ENOENT },
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

  int t = vfs_make_root(vfs, "fat");
  ASSERTEQ(t, 0, "");

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
#ifndef VIRTUAL_FILESYSTEM
  SKIP();
#endif

  static const vfs_stat_result valid_data[] =
  {
    { "aaa", 0,                         { 3, S_IFDIR, 0 }},
    { "./aaa/b", 0,                     { 4, S_IFDIR, 0 }},
    { "/ccc", 0,                        { 5, S_IFDIR, 0 }},
    { "aaa/..\\ccc/d", 0,               { 6, S_IFDIR, 0 }},
    { "ccc/d\\.././..\\aaa/b\\e/", 0,   { 7, S_IFDIR, 0 }},
    { "0", 0,                           { 8, S_IFDIR, 0 }}, // Test inserting before existing.
    { "1", 0,                           { 9, S_IFDIR, 0 }},
    { "á", 0,                           { 10, S_IFDIR, 0 }},
    { "fat:\\somedir", 0,               { 11, S_IFDIR, 0 }},
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
    { "c:/fff/", 0,                     { 12, S_IFDIR, 0 }},
#endif
  };

  static const vfs_result invalid_data[] =
  {
    { "",               -ENOENT },
    { "aaa",            0 },
    { "aaa",            -EEXIST },
    { "wtf/a",          -ENOENT },
    { "/",              -EEXIST },
    { ".",              -EEXIST },
    { "..",             -EEXIST },
    { "./",             -EEXIST },
    { "D:\\",           -ENOENT },
    { "sdcardsfdfds:/", -ENOENT },
    { "file.txt/abc",   -ENOTDIR },
    { "fat:/",          -EEXIST },
    { "sdcard:\\a",     -ENOENT },
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

  int t = vfs_make_root(vfs, "fat");
  ASSERTEQ(t, 0, "");

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
#ifndef VIRTUAL_FILESYSTEM
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
    { "fat", "", DO_MAKE_ROOT, 0,         0,        { 6, S_IFDIR, 0 }},
    { "fat:/", "", DO_CHDIR, 0,           0,        { 6, S_IFDIR, 0 }},
    { "fat:" DIR_SEPARATOR, "", DO_GETCWD, 0, 0,    { 6, S_IFDIR, 0 }},
    { "dir", "", DO_MKDIR, 0,             0,        { 7, S_IFDIR, 0 }},
    { "dir", "", DO_CHDIR, 0,             0,        { 7, S_IFDIR, 0 }},
    { "fat:" DIR_SEPARATOR "dir", "", DO_GETCWD, 0, 0, { 7, S_IFDIR, 0 }},
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
    // "/" is the root of the current drive in these OSes.
    { "/",  "", DO_CHDIR, 0,              0,        { 6, S_IFDIR, 0 }},
    { "fat:" DIR_SEPARATOR, "", DO_GETCWD, 0, 0,    { 6, S_IFDIR, 0 }},
#endif
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
#ifndef VIRTUAL_FILESYSTEM
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
    // Long names that are over the threshold to switch to name allocation.
    { "file", "abcdefghijklmnopqrstuvwxyz", DO_RENAME, 0, 0, { 2, S_IFREG, 0 }},
    { "abcdefghijklmnopqrstuvwxyz", "file", DO_RENAME, 0, 0, { 2, S_IFREG, 0 }},
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
    { "dir", "dir2", DO_RENAME, -ENOTEMPTY,       0,        { 4, S_IFDIR, 0 }},
    // Old dir must not be ancestor of new dir.
    { "dir", "dir/dir", DO_RENAME, -EINVAL,       -ENOENT, {}},
    { "dir2", "dir2/dir3/dir4", DO_RENAME, -EINVAL, -ENOENT, {}},
    // Don't allow renaming CWD or overwriting CWD.
    { "dir", "", DO_CHDIR, 0,                       0,        { 3, S_IFDIR, 0 }},
    { ".", "deez", DO_RENAME, -EBUSY,               -2,       {}},
    { "../dir", "../deez", DO_RENAME, -EBUSY,       -2,       {}},
    { "../dir2", ".", DO_RENAME, -EBUSY,            0,        { 3, S_IFDIR, 0 }},
    { "../dir2", "../dir", DO_RENAME, -EBUSY,       0,        { 3, S_IFDIR, 0 }},
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
#ifndef VIRTUAL_FILESYSTEM
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
#ifndef VIRTUAL_FILESYSTEM
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
    // Can't remove the CWD.
    { "dir", "", DO_MKDIR, 0,                 0,        { 3, S_IFDIR, 0 }},
    { "dir", "", DO_CHDIR, 0,                 0,        { 3, S_IFDIR, 0 }},
    { ".", "", DO_RMDIR, -EBUSY,              0,        { 3, S_IFDIR, 0 }},
    { "../dir", "", DO_RMDIR, -EBUSY,        0,        { 3, S_IFDIR, 0 }},
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
#ifndef VIRTUAL_FILESYSTEM
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
#ifndef VIRTUAL_FILESYSTEM
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
#ifndef VIRTUAL_FILESYSTEM
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
  ssize_t length_ret;

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
      // vfs_filelength should return the same thing.
      length_ret = vfs_filelength(vfs, inode);
      ASSERT(length_ret >= 0, "vfs_filelength: %s", str);
      ASSERTEQ((size_t)length_ret, len, "vfs_filelength: %s", str);

      // 3. read.
      ret = vfs_lock_file_read(vfs, inode, &data, &length);
      ASSERTEQ(ret, 0, "lock(r): %s", str);

      ASSERTMEM(str, data, len, "compare: %s", str);

      ret = vfs_unlock_file_read(vfs, inode);
      ASSERTEQ(ret, 0, "unlock(r): %s", str);

      // 4. truncate.
      ret = vfs_truncate(vfs, inode);
      ASSERTEQ(ret, 0, "truncate: %s", str);
      ret = vfs_stat(vfs, name, &st);
      ASSERTEQ(ret, 0, "stat: %s", str);
      ASSERTEQ(st.st_size, 0, "length: %s", str);
      length_ret = vfs_filelength(vfs, inode);
      ASSERTEQ(length_ret, 0, "vfs_filelength: %s", str);
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


/**
 * Caching functions.
 */

struct vfs_cache_result
{
  const char *path;
  int expected_ret;
  int64_t modified_time;
};

struct vfs_cache_content
{
  const char *path;
  int expected_ret;
  int64_t modified_time;
  const char *content;
};

UNITTEST(vfs_cache_directory)
{
#ifndef VIRTUAL_FILESYSTEM
  SKIP();
#endif

  ScopedVFS vfs = vfs_init();
  ASSERT(vfs, "");

  // Preset a cached file, virtual file, and virtual dir.
  char buf[32]{};
  int ret = vfs_cache_file(vfs, "file", buf, 32);
  ASSERTEQ(ret, 0, "");
  ret = vfs_create_file_at_path(vfs, "vfile");
  ASSERTEQ(ret, 0, "");
  ret = vfs_mkdir(vfs, "vdir", 0644);
  ASSERTEQ(ret, 0, "");
  ret = vfs_make_root(vfs, "fat");
  ASSERTEQ(ret, 0, "");

  static const vfs_cache_result valid_data[] =
  {
    { "dir1",           0, 1 },
    { "dir1/dir2",      0, 2 },
    { "/dir3",          0, 3 },
    { "dir3/../dir4",   0, 4 },
    { "vdir/dir5",      0, 5 },
    { "fat:/dir6",      0, 6 },
  };

  static const vfs_cache_result invalid_data[] =
  {
    { "",           -ENOENT,  0 },
    { "/",          -EEXIST,  0 },
    { ".",          -EEXIST,  0 },
    { "..",         -EEXIST,  0 },
    { "./",         -EEXIST,  0 },
    { "dirA/dirB",  -ENOENT,  0 },
    { "dirC",       0,        12345 },
    { "dirC",       -EEXIST,  0 },
    { "file",       -EEXIST,  0 },
    { "file/dirD",  -ENOTDIR, 0 },
    { "vdir",       -EEXIST,  0 },
    { "vfile",      -EEXIST,  0 },
    { "vfile/dirE", -ENOTDIR, 0 },
    { "fat:/",      -EEXIST,  0 },
    { "fat:/dirF",  0,        56789 },
    { "fat:/dirG/dirH", -ENOENT, 0 },
    { "sdcard:/dirI",   -ENOENT, 0 },
  };

  const auto &do_test = [&vfs](const struct vfs_cache_result &d)
  {
    struct stat st{};
    st.st_mtime = d.modified_time;

    int ret = vfs_cache_directory(vfs, d.path, &st);
    ASSERTEQ(ret, d.expected_ret, "%s", d.path);

    if(ret != 0)
      return;

    st.st_mtime = 0;
    ret = vfs_stat(vfs, d.path, &st);
    ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "%s", d.path);
    ASSERTEQ(st.st_mtime, d.modified_time, "%s", d.path);

    // vfs_unlink should detect an existing cached file and do nothing.
    ret = vfs_unlink(vfs, d.path);
    ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "unlink %s", d.path);
    ret = vfs_stat(vfs, d.path, &st);
    ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "unlink %s", d.path);

    // vfs_rmdir should detect an existing cached file and do nothing.
    ret = vfs_rmdir(vfs, d.path);
    ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "rmdir %s", d.path);
    ret = vfs_stat(vfs, d.path, &st);
    ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "rmdir %s", d.path);

    // vfs_access should detect an existing cached file.
    ret = vfs_access(vfs, d.path, R_OK);
    ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "access %s", d.path);
  };

  SECTION(Valid)
  {
    for(auto &d : valid_data)
      do_test(d);
  }

  SECTION(Invalid)
  {
    for(auto &d : invalid_data)
      do_test(d);
  }

  SECTION(vfs_rename)
  {
    // vfs_rename should detect an existing cached file, but still *work*.
    static constexpr const char *targets[] =
    {
      "dirC",
      "dirB/dirA",
      "dirV",
      "dirA",
      "dirA",
    };
    struct stat st{};
    ret = vfs_cache_directory(vfs, "dirA", &st);
    ASSERTEQ(ret, 0, "");
    ret = vfs_cache_directory(vfs, "dirB", &st);
    ASSERTEQ(ret, 0, "");
    ret = vfs_mkdir(vfs, "dirV", 0755);
    ASSERTEQ(ret, 0, "");

    const char *prev = "dirA";
    for(const char *t : targets)
    {
      ret = vfs_rename(vfs, prev, t);
      ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "%s -> %s", prev, t);
      ret = vfs_stat(vfs, t, &st);
      ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "%s -> %s", prev, t);
      prev = t;
    }

    // Virtual should be moveable over cached too.
    ret = vfs_mkdir(vfs, "dirX", 0755);
    ASSERTEQ(ret, 0, "");
    ret = vfs_rename(vfs, "dirX", "dirA");
    ASSERTEQ(ret, 0, "");
    ret = vfs_stat(vfs, "dirA", &st);
    ASSERTEQ(ret, 0, "");
  }

  SECTION(rmdirCachedNotEmpty)
  {
    // Special: vfs_rmdir should return -ENOTEMPTY even for cached
    // directories (to signal to the caller not to delete a real directory
    // with virtual files inside of it).
    struct stat st{};
    ret = vfs_cache_directory(vfs, "dirA", &st);
    ASSERTEQ(ret, 0, "cache directory");
    ret = vfs_create_file_at_path(vfs, "dirA/child");
    ASSERTEQ(ret, 0, "create virtual file in cached dir");
    ret = vfs_rmdir(vfs, "dirA");
    ASSERTEQ(ret, -ENOTEMPTY,
     "vfs_rmdir on non-empty cached dir should return -ENOTEMPTY");
  }
}

struct read_fn_data
{
  const char *pos;
  size_t left;
};
static size_t read_fn(void * RESTRICT dest, size_t nbytes, void * RESTRICT priv)
{
  read_fn_data *d = reinterpret_cast<read_fn_data *>(priv);
  if(nbytes > d->left)
    nbytes = d->left;

  if(nbytes)
  {
    memcpy(dest, d->pos, nbytes);
    d->pos += nbytes;
    d->left -= nbytes;
  }
  return nbytes;
}

UNITTEST(vfs_cache_file)
{
#ifndef VIRTUAL_FILESYSTEM
  SKIP();
#endif

  ScopedVFS vfs = vfs_init();
  ASSERT(vfs, "");

  // Preset a cached dir, virtual file, and virtual dir.
  struct stat dummy{};
  int ret = vfs_cache_directory(vfs, "dirA", &dummy);
  ASSERTEQ(ret, 0, "");
  ret = vfs_create_file_at_path(vfs, "vfile");
  ASSERTEQ(ret, 0, "");
  ret = vfs_mkdir(vfs, "vdir", 0644);
  ASSERTEQ(ret, 0, "");
  ret = vfs_make_root(vfs, "fat");
  ASSERTEQ(ret, 0, "");

  static const vfs_cache_content valid_data[] =
  {
    { "file1",          0, 0, "abc" },
    { "file2",          0, 0, "def" },
    { "dirA/file3",     0, 0, "djsdjflksjdgklsd" },
    { "/file4",         0, 0, "" },
    { "file5",          0, 0, nullptr },
    { "vdir/file6",     0, 0, "jffjddf" },
    { "fat:/file7",     0, 0, "toptwtpo"},
  };

  static const vfs_cache_content invalid_data[] =
  {
    { "",               -ENOENT,  0,  "fdskfds" },
    { "/",              -EISDIR,  0,  "sdfsdf" },
    { ".",              -EISDIR,  0,  nullptr },
    { "..",             -EISDIR,  0,  "dfssd" },
    { "./",             -EISDIR,  0,  "fkd" },
    { "fileA",          0,        0,  "sdjflksd" },
    { "fileA",          -EEXIST,  0,  nullptr },
    { "dirA",           -EISDIR,  0,  "fsdfdsf" },
    { "dirB/fileB",     -ENOENT,  0,  "fdkgfk" },
    { "fileA/fileB",    -ENOTDIR, 0,  "fffff" },
    { "vfile",          -EEXIST,  0,  "iriqrg" },
    { "vdir",           -EISDIR,  0,  "oregfg" },
    { "vfile/fileC",    -ENOTDIR, 0,  "vnccnm" },
    { "fat:/dir/fileD", -ENOENT,  0,  "eijety" },
    { "sdcard:/fileE",  -ENOENT,  0,  "hhhhh" },
  };

  const auto &check_test = [&vfs](const struct vfs_cache_content &d)
  {
    size_t content_len = d.content ? strlen(d.content) : 0;
    uint32_t inode;
    int ret = vfs_open_if_exists(vfs, d.path, false, &inode);
    ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "%s", d.path);

    const unsigned char *in;
    size_t in_len;
    ret = vfs_lock_file_read(vfs, inode, &in, &in_len);
    ASSERTEQ(ret, 0, "%s", d.path);
    ASSERTEQ(in_len, content_len, "%s", d.path);
    if(in_len)
      ASSERTMEM(in, d.content, in_len, "%s", d.path);

    ret = vfs_unlock_file_read(vfs, inode);
    ASSERTEQ(ret, 0, "%s", d.path);

    ret = vfs_close(vfs, inode);
    ASSERTEQ(ret, 0, "%s", d.path);

    // vfs_stat should work and detect that this file is cached.
    struct stat st{};
    ret = vfs_stat(vfs, d.path, &st);
    ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "stat %s", d.path);
    ASSERTEQ(st.st_dev, VFS_MZX_DEVICE, "stat %s", d.path);

    // vfs_unlink should detect an existing cached file and do nothing.
    ret = vfs_unlink(vfs, d.path);
    ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "unlink %s", d.path);
    ret = vfs_stat(vfs, d.path, &st);
    ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "unlink %s", d.path);

    // vfs_rmdir should detect an existing cached file and do nothing.
    ret = vfs_rmdir(vfs, d.path);
    ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "rmdir %s", d.path);
    ret = vfs_stat(vfs, d.path, &st);
    ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "rmdir %s", d.path);

    // vfs_access should detect an existing cached file.
    ret = vfs_access(vfs, d.path, R_OK);
    ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "access %s", d.path);
  };

  const auto &do_test = [&vfs, &check_test](const struct vfs_cache_content &d)
  {
    size_t content_len = d.content ? strlen(d.content) : 0;

    int ret = vfs_cache_file(vfs, d.path, d.content, content_len);
    ASSERTEQ(ret, d.expected_ret, "%s", d.path);

    if(ret == 0)
      check_test(d);
  };

  const auto &do_test_cb = [&vfs, &check_test](const struct vfs_cache_content &d)
  {
    size_t content_len = d.content ? strlen(d.content) : 0;
    read_fn_data data{ d.content, content_len };

    int ret = vfs_cache_file_callback(vfs, d.path, read_fn, &data, 64);
    ASSERTEQ(ret, d.expected_ret, "%s", d.path);

    if(ret == 0)
      check_test(d);
  };

  SECTION(Valid)
  {
    for(auto &d : valid_data)
      do_test(d);
  }

  SECTION(ValidCallback)
  {
    for(auto &d : valid_data)
      do_test_cb(d);
  }

  SECTION(Invalid)
  {
    for(auto &d : invalid_data)
      do_test(d);
  }

  SECTION(InvalidCallback)
  {
    for(auto &d : invalid_data)
      do_test_cb(d);
  }

  SECTION(vfs_rename)
  {
    // vfs_rename should detect an existing cached file, but still *work*.
    static constexpr const char *targets[] =
    {
      "fileC",
      "dirB/fileA",
      "dirV/fileA",
      "fileV",
      "fileA",
      "fileA",
    };
    struct stat st{};
    ret = vfs_cache_file(vfs, "fileA", NULL, 0);
    ASSERTEQ(ret, 0, "");
    ret = vfs_cache_directory(vfs, "dirB", &st);
    ASSERTEQ(ret, 0, "");
    ret = vfs_mkdir(vfs, "dirV", 0755);
    ASSERTEQ(ret, 0, "");
    ret = vfs_create_file_at_path(vfs, "fileV");
    ASSERTEQ(ret, 0, "");

    const char *prev = "fileA";
    for(const char *t : targets)
    {
      ret = vfs_rename(vfs, prev, t);
      ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "%s -> %s", prev, t);
      ret = vfs_stat(vfs, t, &st);
      ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "%s -> %s", prev, t);
      prev = t;
    }

    // Virtual should be moveable over cached too.
    ret = vfs_create_file_at_path(vfs, "fileX");
    ASSERTEQ(ret, 0, "");
    ret = vfs_rename(vfs, "fileX", "fileA");
    ASSERTEQ(ret, 0, "");
    ret = vfs_stat(vfs, "fileA", &st);
    ASSERTEQ(ret, 0, "");
  }

  SECTION(vfs_create_file_at_path)
  {
    // vfs_create_file_at_path should be able to unlink a cache entry.
    static constexpr char data[] = "lol";
    struct stat st{};

    ret = vfs_cache_file(vfs, "fileA", data, sizeof(data));
    ASSERTEQ(ret, 0, "create cached file");
    ret = vfs_create_file_at_path(vfs, "fileA");
    ASSERTEQ(ret, 0, "create virtual file over cached file");
    ret = vfs_stat(vfs, "fileA", &st);
    ASSERTEQ(ret, 0, "should be virtual i.e. 0");
    ASSERTEQ(st.st_size, 0, "new virtual file should be length 0");
  }
}

// Common VFS initial state for the cache size management tests.
// The following cache tests all rely on the exact paths and sizes
// set up here!
static constexpr char fileopen_data[] = "abcdefgh12345678ZYXWVUTS...:...:";
static constexpr size_t fileopen_len = sizeof(fileopen_data) - 1;

static size_t setup_cache_testing_vfs(ScopedVFS &vfs)
{
  static const char *dirs[] =
  {
    "dirA",
    "dirA/dirB",
    "dirA/dirC",
    "fat:/dirX",
  };

  static const struct
  {
    const char *path;
    size_t size;
  } files[] =
  {
    { "file1", 256 },
    { "dirA/filedel", 200 },
    { "dirA/file2", 64 },
    { "dirA/dirB/file3", 128 },
    { "dirA/dirB/file4", 256 },
    { "dirA/dirC/file5", 224 },
    { "dirA/dirC/file6", 72 },
    { "fat:/fileX", 192 },
    { "fat:/dirX/fileY", 96 },
  };

  static const char file_buf[256]{};
  size_t sz;
  int ret;

  // Initial size should be zero.
  sz = vfs_get_cache_total_size(vfs);
  ASSERTEQ(sz, 0, "");

  // Making a root should not affect the cache size.
  ret = vfs_make_root(vfs, "fat");
  ASSERTEQ(ret, 0, "");

  // Init several cache directories and verify they do NOT
  // count toward the total cache size.
  for(auto &d : dirs)
  {
    struct stat dummy{};
    ret = vfs_cache_directory(vfs, d, &dummy);
    ASSERTEQ(ret, 0, "%s", d);
    sz = vfs_get_cache_total_size(vfs);
    ASSERTEQ(sz, 0, "%s", d);
  }

  // Size should update after adding files.
  size_t total = 0;
  for(auto &f : files)
  {
    total += f.size;
    ret = vfs_cache_file(vfs, f.path, file_buf, f.size);
    ASSERTEQ(ret, 0, "%s", f.path);
    sz = vfs_get_cache_total_size(vfs);
    ASSERTEQ(sz, total, "%s", f.path);
  }

  // Create an extra file with non-trivial content to test invalidation.
  total += fileopen_len;
  ret = vfs_cache_file(vfs, "fileopen", fileopen_data, fileopen_len);
  ASSERTEQ(ret, 0, "fileopen");
  sz = vfs_get_cache_total_size(vfs);
  ASSERTEQ(sz, total, "fileopen");
  return total;
}

static uint32_t fileopen_prologue(ScopedVFS &vfs)
{
  uint32_t inode;
  int ret = vfs_open_if_exists(vfs, "fileopen", false, &inode);
  ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "");
  return inode;
}

static void fileopen_epilogue(ScopedVFS &vfs, uint32_t inode)
{
  // fileopen should have been invalidated at this point.
  // However, because it has an open reference, it hasn't been freed yet
  // and still counts toward the cache total size.
  size_t sz;
  int ret;

  sz = vfs_get_cache_total_size(vfs);
  ASSERTEQ(sz, fileopen_len, "");

  const unsigned char *content;
  size_t content_len;
  ret = vfs_lock_file_read(vfs, inode, &content, &content_len);
  ASSERTEQ(ret, 0, "");
  ASSERTEQ(content_len, fileopen_len, "");
  ASSERTMEM(content, fileopen_data, fileopen_len, "");

  ret = vfs_unlock_file_read(vfs, inode);
  ASSERTEQ(ret, 0, "");

  ret = vfs_close(vfs, inode);
  ASSERTEQ(ret, 0, "");

  sz = vfs_get_cache_total_size(vfs);
  ASSERTEQ(sz, 0, "");
}

template<typename T>
static void test_invalidate_current_dir(ScopedVFS &vfs, T invalidate)
{
  // The current working directory should NEVER be invalidated, but
  // descendents should be.
  static const struct
  {
    const char *path;
    boolean is_dir;
    int expected_ret;
  } data[] =
  {
    { "/dirA",                  true,   -VFS_ERR_IS_CACHED },
    { "/dirA/dirB",             true,   -VFS_ERR_IS_CACHED },
    { "/dirA/dirB/dirC",        true,   -ENOENT },
    { "/dirA/dirD",             true,   -ENOENT },
    { "/dirA/file1",            false,  -ENOENT },
    { "/dirA/dirB/file2",       false,  -ENOENT },
    { "/dirA/dirB/dirC/file3",  false,  -ENOENT },
    { "/dirA/dirD/file4",       false,  -ENOENT },
  };

  static const char file_buf[32]{};
  int ret;

  for(auto &d : data)
  {
    if(d.is_dir)
    {
      struct stat st{};
      ret = vfs_cache_directory(vfs, d.path, &st);
    }
    else
      ret = vfs_cache_file(vfs, d.path, file_buf, 32);

    ASSERTEQ(ret, 0, "%s", d.path);
  }

  ret = vfs_chdir(vfs, "/dirA/dirB");
  ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "");
  char buffer[MAX_PATH];
  ret = vfs_getcwd(vfs, buffer, MAX_PATH);
  ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "");
  ASSERTCMP(buffer, BASE "dirA" DIR_SEPARATOR "dirB", "");

  ret = invalidate(vfs, "/dirA");
  ASSERTEQ(ret, 0, "");

  for(auto &d : data)
  {
    struct stat st{};
    ret = vfs_stat(vfs, d.path, &st);
    ASSERTEQ(ret, d.expected_ret, "%s", d.path);
  }
}

UNITTEST(vfs_get_cache_total_size)
{
#ifndef VIRTUAL_FILESYSTEM
  SKIP();
#endif

  ScopedVFS vfs = vfs_init();
  ASSERT(vfs, "");
  size_t total_minus_file1;
  size_t total = 0;
  size_t sz;
  int ret;

  total = setup_cache_testing_vfs(vfs);
  total_minus_file1 = total - 256;

  // Size should not be affected by creating a virtual file.
  ret = vfs_create_file_at_path(vfs, "vfile");
  ASSERTEQ(ret, 0, "");
  sz = vfs_get_cache_total_size(vfs);
  ASSERTEQ(sz, total, "");

  // Size should not be affected by removing a virtual file.
  ret = vfs_unlink(vfs, "vfile");
  ASSERTEQ(ret, 0, "");
  sz = vfs_get_cache_total_size(vfs);
  ASSERTEQ(sz, total, "");

  uint32_t inode;
  ret = vfs_open_if_exists(vfs, "file1", true, &inode);
  ASSERTEQ(ret, -VFS_ERR_IS_CACHED, "");

  // Size should update after a truncation.
  // This function forces a minimum allocation size.
  ret = vfs_truncate(vfs, inode);
  ASSERTEQ(ret, 0, "");
  sz = vfs_get_cache_total_size(vfs);
  ASSERTEQ(sz, total_minus_file1 + 32, "");

  // Size should update after an expanding write.
  unsigned char **data;
  size_t *data_length;
  size_t *data_alloc;
  ret = vfs_lock_file_write(vfs, inode, &data, &data_length, &data_alloc);
  ASSERTEQ(ret, 0, "");

  void *tmp = realloc(*data, 256);
  ASSERT(tmp, "");

  *data = reinterpret_cast<unsigned char *>(tmp);
  *data_alloc = 256;
  *data_length = 128;
  memset(tmp, 'a', *data_length);

  ret = vfs_unlock_file_write(vfs, inode);
  ASSERTEQ(ret, 0, "");
  sz = vfs_get_cache_total_size(vfs);
  ASSERTEQ(sz, total_minus_file1 + 256, "");

  // Size should update after a shrinking write.
  ret = vfs_lock_file_write(vfs, inode, &data, &data_length, &data_alloc);
  ASSERTEQ(ret, 0, "");

  tmp = realloc(*data, 128);
  ASSERT(tmp, "");

  *data = reinterpret_cast<unsigned char *>(tmp);
  *data_alloc = 128;

  ret = vfs_unlock_file_write(vfs, inode);
  ASSERTEQ(ret, 0, "");
  sz = vfs_get_cache_total_size(vfs);
  ASSERTEQ(sz, total_minus_file1 + 128, "");
}

UNITTEST(vfs_invalidate_at_path)
{
#ifndef VIRTUAL_FILESYSTEM
  SKIP();
#endif

  static constexpr struct
  {
    const char *path;
    int expected_ret;
    ptrdiff_t diff;
  } data[] =
  {
    { "noexist",      -ENOENT,  0 },
    { "sdcard:/",     -ENOENT,  0 },
    { "dirA/fjsdf",   -ENOENT,  0 },
    { "file1",        0,        -256 },
    { "file1",        -ENOENT,  0 },
    { "dirA/filedel", 0,        -200 },
    { "dirA/dirB",    0,        -384 },
    { "dirA/dirB",    -ENOENT,  0 },
    { "dirA",         0,        -360 },
    { ".",            0,        0 },
    { "..",           0,        0 },
    { "/",            0,        0 },
    { "fat:/",        0,        -288 },
    { "fat:/",        0,        0 },
  };

  ScopedVFS vfs = vfs_init();
  ASSERT(vfs, "");
  ptrdiff_t diff;
  size_t sz;
  int ret;

  size_t total = setup_cache_testing_vfs(vfs);
  uint32_t inode = fileopen_prologue(vfs);

  for(auto &d : data)
  {
    ret = vfs_invalidate_at_path(vfs, d.path);
    ASSERTEQ(ret, d.expected_ret, "%s %d", d.path, d.expected_ret);
    if(ret != 0)
      continue;

    sz = vfs_get_cache_total_size(vfs);
    ASSERT(total >= sz, "%s %d", d.path, d.expected_ret);
    diff = sz - total;
    ASSERTEQ(diff, d.diff, "%s %d", d.path, d.expected_ret);
    total = sz;
  }

  fileopen_epilogue(vfs, inode);

  auto fn = [](vfilesystem *v, const char *p)
   { return vfs_invalidate_at_path(v, p); };
  test_invalidate_current_dir(vfs, fn);
}

UNITTEST(vfs_invalidate_at_least)
{
#ifndef VIRTUAL_FILESYSTEM
  SKIP();
#endif

  static constexpr struct
  {
    int expected_ret;
    size_t request_diff;
    size_t remaining_diff;
    size_t remaining_total;
    struct
    {
      const char *path;
      int expected_stat;
    } check[16];
  } data[] =
  {
    { 0, 0, 0, 1520,
     {{ "file1", -VFS_ERR_IS_CACHED }}},
    { 0, 256, 0, 1264,
     {{ "file1", -ENOENT }, { "dirA/dirB/file4", -VFS_ERR_IS_CACHED }}},
    { 0, 800, 0, 392,
     {
      { "dirA/filedel", -ENOENT },
      { "dirA/dirB/file3", -VFS_ERR_IS_CACHED },
      { "dirA/dirB/file4", -ENOENT },
      { "dirA/dirC/file5", -ENOENT },
      { "fat:/fileX", -ENOENT }}},
    { 0, 1000, 640, fileopen_len,
     {
      { "dirA/file2", -ENOENT },
      { "dirA/dirB/file3", -ENOENT },
      { "dirA/dirB/file6", -ENOENT },
      { "fat:/dirX/fileY", -ENOENT }}},
    { 0, 256, 256, fileopen_len, {}},
  };

  ScopedVFS vfs = vfs_init();
  ASSERT(vfs, "");
  size_t sz;
  int ret;

  // The sort used by this function uses timestamps to prioritize
  // stale files for deletion. Disable them so this test is deterministic...
  vfs_set_timestamps_enabled(vfs, false);

  /*size_t total =*/ setup_cache_testing_vfs(vfs);
  uint32_t inode = fileopen_prologue(vfs);

  ret = vfs_invalidate_at_least(vfs, nullptr);
  ASSERTEQ(ret, -EINVAL, "");

  char fmt[256];
  for(auto &d : data)
  {
    snprintf(fmt, sizeof(fmt), "%d %zu %zu %zu : %s", d.expected_ret,
     d.request_diff, d.remaining_diff, d.remaining_total, d.check[0].path);

    size_t left = d.request_diff;
    ret = vfs_invalidate_at_least(vfs, &left);
    ASSERTEQ(ret, d.expected_ret, "%s", fmt);
    ASSERTEQ(left, d.remaining_diff, "%s", fmt);
    sz = vfs_get_cache_total_size(vfs);
    ASSERTEQ(sz, d.remaining_total, "%s", fmt);

    for(auto &c : d.check)
    {
      struct stat st{};
      if(!c.path)
        break;
      ret = vfs_stat(vfs, c.path, &st);
      ASSERTEQ(ret, c.expected_stat, "%s (check: %s)", fmt, c.path);
    }
  }

  // Special: vfs_invalidate_at_least doesn't even try to invalidate
  // open files.
  ret = vfs_close(vfs, inode);
  ASSERTEQ(ret, 0, "");
  sz = vfs_get_cache_total_size(vfs);
  ASSERTEQ(sz, fileopen_len, "");

  //fileopen_epilogue(vfs, inode);
}

UNITTEST(vfs_invalidate_all)
{
#ifndef VIRTUAL_FILESYSTEM
  SKIP();
#endif

  ScopedVFS vfs = vfs_init();
  ASSERT(vfs, "");
  int ret;

  size_t total = setup_cache_testing_vfs(vfs);
  uint32_t inode = fileopen_prologue(vfs);

  // This function should reduce the size to JUST the size of fileopen.
  ret = vfs_invalidate_all(vfs);
  ASSERTEQ(ret, 0, "");
  total = vfs_get_cache_total_size(vfs);
  ASSERTEQ(total, fileopen_len, "");

  fileopen_epilogue(vfs, inode);

  auto fn = [](vfilesystem *v, const char *p)
   { return vfs_invalidate_all(v); };
  test_invalidate_current_dir(vfs, fn);
}
