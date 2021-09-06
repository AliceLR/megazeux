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

#include "../../src/io/vfs.h"

#include <errno.h>
#include <sys/stat.h>

static constexpr dev_t MZX_DEVICE = ('M'<<24) | ('Z'<<16) | ('X'<<8) | ('V');

class ScopedVFS
{
  vfilesystem *vfs;

public:
  ScopedVFS(vfilesystem *p = nullptr) : vfs(p) {}
  ~ScopedVFS() { if(vfs) vfs_free(vfs); }

  operator vfilesystem *() { return vfs; }
};

struct vfs_result
{
  const char *path;
  int expected_ret;
  int expected_error;
};

struct vfs_stat_result
{
  const char *path;
  int expected_ret;
  int expected_error;
  ino_t inode;
  mode_t filetype;
  off_t size;
};

static void check_stat(const vfs_stat_result &d, struct stat &st)
{
  ASSERTEQ(st.st_dev, MZX_DEVICE, "%s", d.path);
  ASSERTEQ(st.st_ino, d.inode, "%s", d.path);
  ASSERTEQ(st.st_mode & S_IFMT, d.filetype, "%s", d.path);
  ASSERTEQ(st.st_size, d.size, "%s", d.path);
}

UNITTEST(vfs_stat)
{
  // Simple test to make sure it works on known valid/invalid default files.
  // This function will be further tested in other tests that use it to fetch
  // info to verify other calls worked.

  static const vfs_stat_result valid_data[]
  {
    // These are all the default root since that's the only default file...
    { "..", 0, 0,       1, S_IFDIR, 0 },
    { ".", 0, 0,        1, S_IFDIR, 0 },
    { "./", 0, 0,       1, S_IFDIR, 0 },
    { "/", 0, 0,        1, S_IFDIR, 0 },
    { "/./../", 0, 0,   1, S_IFDIR, 0 },
    { "\\", 0, 0,       1, S_IFDIR, 0 },
    { "\\..\\.", 0, 0,  1, S_IFDIR, 0 },
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
    { "C:/", 0, 0,      1, S_IFDIR, 0 },
    { "C:\\", 0, 0,     1, S_IFDIR, 0 },
    { "c:\\", 0, 0,     1, S_IFDIR, 0 },
#endif
  };

  static const vfs_result invalid_data[]
  {
    { "", -1, ENOENT },
    { "jsdjfk", -1, ENOENT },
    { "sdcard:/", -1, ENOENT },
    { "verylongrootnamewhywouldyouevennamearootthis:/", -1, ENOENT },
    { "D:\\", -1, ENOENT },
  };

  ScopedVFS vfs = vfs_init();
  struct stat st;
  ASSERT(vfs, "");

  SECTION(Valid)
  {
    for(const vfs_stat_result &d : valid_data)
    {
      int ret = vfs_stat(vfs, d.path, &st);
      int err = vfs_error(vfs);
      ASSERTEQ(err, d.expected_error, "%s", d.path);
      ASSERTEQ(ret, d.expected_ret, "%s", d.path);
      check_stat(d, st);
    }
  }

  SECTION(Invalid)
  {
    for(const vfs_result &d : invalid_data)
    {
      int ret = vfs_stat(vfs, d.path, &st);
      int err = vfs_error(vfs);
      ASSERTEQ(err, d.expected_error, "%s", d.path);
      ASSERTEQ(ret, d.expected_ret, "%s", d.path);
      // vfs_error should return 0 when called again.
      err = vfs_error(vfs);
      ASSERTEQ(err, 0, "%s", d.path);
    }
  }
}

UNITTEST(vfs_create_file_at_path)
{
  static const vfs_stat_result valid_data[] =
  {
    { "file.txt", 0, 0,                           2, S_IFREG, 0 },
    { "abc.def", 0, 0,                            3, S_IFREG, 0 },
    { "reallylongfilename.reallylongext", 0, 0,   4, S_IFREG, 0 },
    { "../initrd.img", 0, 0,                      5, S_IFREG, 0 },
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
    { "C:/dkdfjklsjdf", 0, 0,                     6, S_IFREG, 0 },
#endif
  };

  static const vfs_result invalid_data[] =
  {
    { "file.txt",           0, 0 },
    { "file.txt",           -1, EEXIST },
    { "file.txt/file.txt",  -1, ENOTDIR },
    { "/",                  -1, EISDIR },
    { "abchnkdf/file.txt",  -1, ENOENT },
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
    { "c:\\",               -1, EISDIR },
#endif
  };

  ScopedVFS vfs = vfs_init();
  struct stat st;
  ASSERT(vfs, "");

  SECTION(Valid)
  {
    for(const vfs_stat_result &d : valid_data)
    {
      int ret = vfs_create_file_at_path(vfs, d.path);
      int err = vfs_error(vfs);
      ASSERTEQ(err, d.expected_error, "%s", d.path);
      ASSERTEQ(ret, d.expected_ret, "%s", d.path);

      ret = vfs_stat(vfs, d.path, &st);
      err = vfs_error(vfs);
      ASSERTEQ(err, 0, "stat %s", d.path);
      ASSERTEQ(ret, 0, "stat %s", d.path);
      check_stat(d, st);
    }
  }

  SECTION(Invalid)
  {
    for(const vfs_result &d : invalid_data)
    {
      int ret = vfs_create_file_at_path(vfs, d.path);
      int err = vfs_error(vfs);
      ASSERTEQ(err, d.expected_error, "%s", d.path);
      ASSERTEQ(ret, d.expected_ret, "%s", d.path);
    }
  }
}

UNITTEST(vfs_mkdir)
{
  static const vfs_stat_result valid_data[] =
  {
    { "aaa", 0, 0,                        2, S_IFDIR, 0 },
    { "./aaa/b", 0, 0,                    3, S_IFDIR, 0 },
    { "/ccc", 0, 0,                       4, S_IFDIR, 0 },
    { "aaa/..\\ccc/d", 0, 0,              5, S_IFDIR, 0 },
    { "ccc/d\\.././..\\aaa/b\\e/", 0, 0,  6, S_IFDIR, 0 },
    { "0", 0, 0,                          7, S_IFDIR, 0 }, // Test inserting before existing.
    { "1", 0, 0,                          8, S_IFDIR, 0 },
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
    { "c:/fff/", 0, 0,                    9, S_IFDIR, 0 },
#endif
  };

  static const vfs_result invalid_data[] =
  {
    { "",               -1, ENOENT },
    { "aaa",            0, 0 },
    { "aaa",            -1, EEXIST },
    { "wtf/a",          -1, ENOENT },
    { "/",              -1, EEXIST },
    { "D:\\",           -1, ENOENT },
    { "sdcardsfdfds:/", -1, ENOENT },
    { "file.txt/abc",   -1, ENOTDIR },
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
    { "C:\\",           -1, EEXIST },
#endif
  };

  ScopedVFS vfs = vfs_init();
  struct stat st;
  ASSERT(vfs, "");

  SECTION(Valid)
  {
    for(const vfs_stat_result &d : valid_data)
    {
      int ret = vfs_mkdir(vfs, d.path, 0755);
      int err = vfs_error(vfs);
      ASSERTEQ(err, d.expected_error, "%s", d.path);
      ASSERTEQ(ret, d.expected_ret, "%s", d.path);

      ret = vfs_stat(vfs, d.path, &st);
      err = vfs_error(vfs);
      ASSERTEQ(err, 0, "stat %s", d.path);
      ASSERTEQ(ret, 0, "stat %s", d.path);
      check_stat(d, st);
    }
  }

  SECTION(Invalid)
  {
    int r = vfs_create_file_at_path(vfs, "file.txt");
    ASSERTEQ(r, 0, "");

    for(const vfs_result &d : invalid_data)
    {
      int ret = vfs_mkdir(vfs, d.path, 0755);
      int err = vfs_error(vfs);
      ASSERTEQ(err, d.expected_error, "%s", d.path);
      ASSERTEQ(ret, d.expected_ret, "%s", d.path);
    }
  }
}
