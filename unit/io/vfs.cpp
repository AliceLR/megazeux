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
};

struct vfs_stat_result
{
  const char *path;
  int expected_ret;
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
    { "..", 0,        1, S_IFDIR, 0 },
    { ".", 0,         1, S_IFDIR, 0 },
    { "./", 0,        1, S_IFDIR, 0 },
    { "/", 0,         1, S_IFDIR, 0 },
    { "/./../", 0,    1, S_IFDIR, 0 },
    { "\\", 0,        1, S_IFDIR, 0 },
    { "\\..\\.", 0,   1, S_IFDIR, 0 },
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
    { "C:/", 0,       1, S_IFDIR, 0 },
    { "C:\\", 0,      1, S_IFDIR, 0 },
    { "c:\\", 0,      1, S_IFDIR, 0 },
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
      check_stat(d, st);
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
  static const vfs_stat_result valid_data[] =
  {
    { "file.txt", 0,                            2, S_IFREG, 0 },
    { "abc.def", 0,                             3, S_IFREG, 0 },
    { "reallylongfilename.reallylongext", 0,    4, S_IFREG, 0 },
    { "../initrd.img", 0,                       5, S_IFREG, 0 },
    { "é", 0,                                   6, S_IFREG, 0 },
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
    { "C:/dkdfjklsjdf", 0,                      7, S_IFREG, 0 },
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

  SECTION(Valid)
  {
    for(const vfs_stat_result &d : valid_data)
    {
      int ret = vfs_create_file_at_path(vfs, d.path);
      ASSERTEQ(ret, d.expected_ret, "%s", d.path);

      ret = vfs_stat(vfs, d.path, &st);
      ASSERTEQ(ret, 0, "stat %s", d.path);
      check_stat(d, st);
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
  static const vfs_stat_result valid_data[] =
  {
    { "aaa", 0,                         2, S_IFDIR, 0 },
    { "./aaa/b", 0,                     3, S_IFDIR, 0 },
    { "/ccc", 0,                        4, S_IFDIR, 0 },
    { "aaa/..\\ccc/d", 0,               5, S_IFDIR, 0 },
    { "ccc/d\\.././..\\aaa/b\\e/", 0,   6, S_IFDIR, 0 },
    { "0", 0,                           7, S_IFDIR, 0 }, // Test inserting before existing.
    { "1", 0,                           8, S_IFDIR, 0 },
    { "á", 0,                           9, S_IFDIR, 0 },
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
    { "c:/fff/", 0,                     10, S_IFDIR, 0 },
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

  SECTION(Valid)
  {
    for(const vfs_stat_result &d : valid_data)
    {
      int ret = vfs_mkdir(vfs, d.path, 0755);
      ASSERTEQ(ret, d.expected_ret, "%s", d.path);

      ret = vfs_stat(vfs, d.path, &st);
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
      ASSERTEQ(ret, d.expected_ret, "%s", d.path);
    }
  }
}

UNITTEST(FileIO)
{
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
