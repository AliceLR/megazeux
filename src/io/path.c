/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2012, 2020 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <errno.h>
#include <sys/stat.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "path.h"
#include "vio.h"

/**
 * Force a given filename path to use the provided file extension. If the
 * filename already has the given extension, the string will not be modified.
 *
 * @param  path         Path to force the extension of.
 * @param  buffer_len   Size of the buffer of the path.
 * @param  ext          Extension to add to the path.
 * @return              true on success, otherwise false.
 */
boolean path_force_ext(char *path, size_t buffer_len, const char *ext)
{
  size_t path_len = strlen(path);
  size_t ext_len = strlen(ext);

  if((path_len < ext_len) || (path[path_len - ext_len] != '.') ||
   strcasecmp(path + path_len - ext_len, ext))
  {
    if(path_len + ext_len >= buffer_len)
      return false;

    snprintf(path + path_len, MAX_PATH - path_len, "%s", ext);
    path[buffer_len - 1] = '\0';
  }
  return true;
}

/**
 * Get the position of the extension in a filename path.
 *
 * @param  path   Path to get the extension position of.
 * @return        The index of the start of the extension or -1 if not found.
 */
ssize_t path_get_ext_offset(const char *path)
{
  ssize_t path_len = strlen(path);
  ssize_t ext_pos;

  for(ext_pos = path_len - 1; ext_pos >= 0; ext_pos--)
  {
    // Don't let this detect an "extension" of a directory!
    if(isslash(path[ext_pos]))
      break;

    if(path[ext_pos] == '.')
      return ext_pos;
  }
  return -1;
}

/**
 * Get the location of the first char of the filename in a filename path.
 * If there is no filename, the returned index will be the location of the
 * null terminator. If the string is empty, -1 will be returned.
 */
static ssize_t path_get_filename_offset(const char *path)
{
  struct stat stat_info;
  ssize_t pos;
  if(!path || !path[0])
    return -1;

  // If this path stats and happens to be a directory without a terminating
  // slash, the filename offset is the end of the string...
  if(vstat(path, &stat_info) >= 0 && S_ISDIR(stat_info.st_mode))
    return strlen(path);

  // Otherwise, find the last directory separator (if any).
  pos = (ssize_t)strlen(path) - 1;
  while(pos >= 0)
  {
    if(isslash(path[pos]))
      return pos + 1;

    pos--;
  }
  return 0;
}

/**
 * Determine if the given path is an absolute path.
 *
 * @param  path   Path to test.
 * @return        length of root token if this is an absolute path, otherwise 0.
 */
ssize_t path_is_absolute(const char *path)
{
  size_t len;
  size_t i;

  // Unix-style root.
  if(isslash(path[0]))
    return 1;

  // DOS-style root.
  len = strlen(path);
  for(i = 0; i < len; i++)
  {
    if(isslash(path[i]))
      break;

    if(path[i] == ':')
    {
      if(i == 0)
        break;

      i++;
      if(!path[i])
        return i;

      if(isslash(path[i]))
      {
        while(isslash(path[i]))
          i++;
        return i;
      }
      break;
    }
  }
  return 0;
}

/**
 * Determine if the given path is a root path.
 *
 * @param  path   Path to test.
 * @return        `true` if the path is a root path, otherwise `false`.
 */
boolean path_is_root(const char *path)
{
  ssize_t root_len = path_is_absolute(path);
  return root_len && !path[root_len];
}

/**
 * Determine if the given path contains a directory.
 *
 * @param  path   Path to test.
 * @return        True if the path contains a directory, otherwise false.
 */
boolean path_has_directory(const char *path)
{
  struct stat stat_info;
  size_t len;
  size_t i;

  if(!path || !path[0])
    return false;

  // Check for slashes first to avoid a stat call...
  len = strlen(path);
  for(i = 0; i < len; i++)
    if(isslash(path[i]))
      return true;

  return vstat(path, &stat_info) >= 0 && S_ISDIR(stat_info.st_mode);
}

/**
 * Truncate a path to its directory portion, if any.
 *
 * @param  path         Path to truncate.
 * @param  buffer_len   Size of the path buffer.
 * @return              The length of the directory string, or -1 on error.
 */
ssize_t path_to_directory(char *path, size_t buffer_len)
{
  ssize_t filename_pos = path_get_filename_offset(path);

  // Invalid path.
  if(filename_pos < 0 || (size_t)filename_pos >= buffer_len)
  {
    if(buffer_len > 0)
      path[0] = '\0';

    return -1;
  }

  path[filename_pos] = '\0';
  if(filename_pos > 0)
    filename_pos = path_clean_slashes(path, buffer_len);

  return filename_pos;
}

/**
 * Remove the directory portion of a filename path, if any.
 *
 * @param  path         Filename path to remove the directory portion of.
 * @param  buffer_len   Size of the path buffer.
 * @return              The length of the filename string, or -1 on error.
 */
ssize_t path_to_filename(char *path, size_t buffer_len)
{
  ssize_t filename_pos = path_get_filename_offset(path);
  size_t path_len = strlen(path);
  size_t filename_len;

  // Invalid path.
  if(filename_pos < 0 || path_len - (size_t)filename_pos >= buffer_len)
  {
    if(buffer_len > 0)
      path[0] = '\0';

    return -1;
  }

  // If there isn't a directory prefix, don't modify the buffer...
  if(filename_pos == 0)
    return path_len;

  filename_len = path_len - filename_pos;
  if(filename_len > 0)
    memmove(path, path + filename_pos, filename_len);
  path[filename_len] = '\0';

  return filename_len;
}

/**
 * Copy the directory portion of a filename or directory path to the
 * destination buffer.
 *
 * @param  dest       Destination buffer for the directory.
 * @param  dest_len   Size of the destination buffer.
 * @param  path       Path to get the directory of.
 * @return            The length of the directory string, or -1 on error.
 */
ssize_t path_get_directory(char *dest, size_t dest_len, const char *path)
{
  ssize_t filename_pos = path_get_filename_offset(path);

  // Invalid path, or it's too long to store.
  if(filename_pos < 0 || (size_t)filename_pos >= dest_len)
  {
    if(dest_len > 0)
      dest[0] = '\0';

    return -1;
  }

  dest[filename_pos] = '\0';
  if(filename_pos > 0)
  {
    memcpy(dest, path, filename_pos);
    filename_pos = path_clean_slashes(dest, dest_len);
  }
  return filename_pos;
}

/**
 * Copy the filename portion of a filename path to the destination buffer.
 *
 * @param  dest       Destination buffer for the filename.
 * @param  dest_len   Size of the destination buffer.
 * @param  path       Path to get the filename of.
 * @return            The length of the filename string, or -1 on error.
 */
ssize_t path_get_filename(char *dest, size_t dest_len, const char *path)
{
  ssize_t filename_pos = path_get_filename_offset(path);
  size_t path_len = strlen(path);
  size_t filename_len;

  // Invalid path, or it's too long to store.
  if(filename_pos < 0 || path_len - (size_t)filename_pos >= dest_len)
  {
    if(dest_len > 0)
      dest[0] = '\0';

    return -1;
  }

  filename_len = path_len - filename_pos;
  dest[filename_len] = '\0';
  if(filename_len > 0)
    memcpy(dest, path + filename_pos, filename_len);

  return filename_len;
}

/**
 * Copy both the directory and the filename portions of a filename path to
 * individual destination buffers.
 *
 * @param  d_dest   Destination buffer for the directory.
 * @param  d_len    Size of the directory destination buffer.
 * @param  f_dest   Destination buffer for the filename.
 * @param  f_len    Size of the filename destination buffer.
 * @param  path     Path to get the directory and filename of.
 * @return          true on success, otherwise false.
 */
boolean path_get_directory_and_filename(char *d_dest, size_t d_len,
 char *f_dest, size_t f_len, const char *path)
{
  ssize_t filename_pos = path_get_filename_offset(path);
  size_t path_len = strlen(path);
  size_t filename_len;

  // Invalid path, or one of the components is too long to store.
  if(filename_pos < 0 || (size_t)filename_pos >= d_len ||
   path_len - (size_t)filename_pos >= f_len)
  {
    if(d_len > 0)
      d_dest[0] = '\0';
    if(f_len > 0)
      f_dest[0] = '\0';

    return false;
  }

  d_dest[filename_pos] = '\0';
  if(filename_pos > 0)
  {
    memcpy(d_dest, path, filename_pos);
    path_clean_slashes(d_dest, d_len);
  }

  filename_len = path_len - filename_pos;
  f_dest[filename_len] = '\0';
  if(filename_len > 0)
    memcpy(f_dest, path + filename_pos, filename_len);

  return true;
}

/**
 * Clean duplicate directory separators out of a path and normalize them to
 * the correct slash for the current platform.
 *
 * @param  path       Path to clean slashes of.
 * @param  path_len   Size of the path buffer.
 * @return            The length of the destination path.
 */
size_t path_clean_slashes(char *path, size_t path_len)
{
  boolean need_copy = false;
  size_t i = 0;
  size_t j = 0;

  while((i < path_len) && path[i])
  {
    if(isslash(path[i]))
    {
      while(isslash(path[i]))
        i++;

      if(i > j + 1)
        need_copy = true;

      path[j++] = DIR_SEPARATOR_CHAR;
    }
    else
    {
      if(need_copy)
        path[j] = path[i];

      i++;
      j++;
    }
  }
  path[j] = '\0';

  if((j >= 2) && (path[j - 1] == DIR_SEPARATOR_CHAR) && (path[j - 2] != ':'))
    path[--j] = '\0';

  return j;
}

/**
 * Create a duplicate of a path with duplicate directory separators removed
 * and with the directory separators normalized to the correct slash for the
 * current platform.
 *
 * @param  dest       Destination buffer for the cleaned path.
 * @param  dest_len   Size of the destination buffer.
 * @param  path       Path to clean slashes of.
 * @return            The length of the destination path.
 */
size_t path_clean_slashes_copy(char *dest, size_t dest_len, const char *path)
{
  size_t path_len = strlen(path);
  size_t i = 0;
  size_t j = 0;

  while((i < path_len) && (j < dest_len - 1))
  {
    if(isslash(path[i]))
    {
      while(isslash(path[i]))
        i++;

      dest[j++] = DIR_SEPARATOR_CHAR;
    }
    else
      dest[j++] = path[i++];
  }
  dest[j] = '\0';

  if((j >= 2) && (dest[j - 1] == DIR_SEPARATOR_CHAR) && (dest[j - 2] != ':'))
    dest[--j] = '\0';

  return j;
}

/**
 * Append a relative directory or filename path to an existing base path.
 * This function does not handle ./, ../, etc; to resolve relative paths
 * containing those, use path_navigate() instead. On succes, the destination
 * is guaranteed to have cleaned slashes (see path_clean_slashes).
 *
 * @param  path         Existing base path.
 * @param  buffer_len   Size of the base path buffer.
 * @param  rel          Relative path to be joined to the end of the base path.
 * @return              The new length of the base path, or -1 on error.
 */
ssize_t path_append(char *path, size_t buffer_len, const char *rel)
{
  size_t path_len = strlen(path);
  size_t rel_len = strlen(rel);

  // Needs to be able to fit the worst case size: path + separator + rel + \0.
  if(path_len && rel_len && path_len + rel_len + 2 <= buffer_len)
  {
    path_len = path_clean_slashes(path, buffer_len);
    path[path_len++] = DIR_SEPARATOR_CHAR;

    rel_len = path_clean_slashes_copy(path + path_len, buffer_len - path_len, rel);
    return path_len + rel_len;
  }
  return -1;
}

/**
 * Join a base directory path to a relative directory or filename path.
 * This function does not handle ./, ../, etc; to resolve relative paths
 * containing those, use path_navigate() instead. On success, the destination
 * is guaranteed to have cleaned slashes (see path_clean_slashes)
 *
 * @param  dest       Destination buffer for the joined path.
 * @param  dest_len   Size of the destination buffer.
 * @param  base       Base directory path.
 * @param  rel        Relative path to be joined to the end of the base path.
 * @return            The length of the resulting path, or -1 on error.
 */
ssize_t path_join(char *dest, size_t dest_len, const char *base, const char *rel)
{
  size_t base_len = strlen(base);
  size_t rel_len = strlen(rel);

  // Needs to be able to fit the worst case size: base + separator + rel + \0.
  if(base_len && rel_len && base_len + rel_len + 2 <= dest_len)
  {
    base_len = path_clean_slashes_copy(dest, dest_len, base);
    dest[base_len++] = DIR_SEPARATOR_CHAR;

    rel_len = path_clean_slashes_copy(dest + base_len, dest_len - base_len, rel);
    return base_len + rel_len;
  }
  return -1;
}

/**
 * Determine if `path` is prefixed by `prefix`. Returns the index of the first
 * non-prefix and non-slash char of `path` if `path` is prefixed by `prefix`,
 * otherwise -1.
 */
static ssize_t path_has_prefix(const char *path, size_t buffer_len,
 const char *prefix, size_t prefix_len)
{
  // Normal string compare, but allow different kinds of slashes.
  size_t i = 0;
  size_t j = 0;
  while(i < prefix_len && prefix[i])
  {
    if(j >= buffer_len || !path[j])
      return -1;

    if(isslash(prefix[i]))
    {
      if(!isslash(path[j]))
        return -1;

      // Skip duplicate slashes.
      while(isslash(prefix[i]))
        i++;
      while(isslash(path[j]))
        j++;
    }
    else
    {
      if(prefix[i++] != path[j++])
        return -1;
    }
  }

  // Make sure this was actually a valid prefix--the prefix should either have
  // a trailing slash or the next character of the path should be a slash.
  if(!isslash(prefix[i - 1]) && !isslash(path[j]))
    return -1;

  // The prefix likely does not have trailing slashes, so skip them.
  while(isslash(path[j]))
    j++;

  return j;
}

/**
 * Remove a directory prefix from a path if it exists. The prefix does not
 * need trailing slashes, but it must represent a complete directory name.
 *
 * @param  path         Path to remove a prefix from.
 * @param  buffer_len   Size of the path buffer.
 * @param  prefix       Prefixed directory to remove from path if found.
 * @param  prefix_len   Length of prefix to test or 0 to test the entire prefix.
 * @return              New length of path, or -1 if the prefix was not found.
 */
ssize_t path_remove_prefix(char *path, size_t buffer_len,
 const char *prefix, size_t prefix_len)
{
  prefix_len = prefix_len ? prefix_len : strlen(prefix);

  if(prefix_len)
  {
    ssize_t offset = path_has_prefix(path, buffer_len, prefix, prefix_len);
    if(offset < 0)
      return -1;

    return path_clean_slashes_copy(path, buffer_len, path + offset);
  }
  return -1;
}

/**
 * Navigate a directory path to a target like chdir. The provided directory
 * path must be a valid directory. The target may be a relative path or an
 * absolute path in either Unix or Windows style. If "." or ".." is found in
 * the target, it will be handled appropriately. If the final resulting path
 * successfully stats, the provided path will be overwritten with the
 * destination. If the final path is not valid or if an error occurs, the
 * provided path will not be modified.
 *
 * @param  path       Directory path to navigate from.
 * @param  path_len   Size of path buffer.
 * @param  target     Target to navigate to.
 * @return            The new length of the path, or -1 on error.
 */
ssize_t path_navigate(char *path, size_t path_len, const char *target)
{
  struct stat stat_info;
  char buffer[MAX_PATH];
  const char *current;
  const char *next;
  const char *end;
  char current_char;
  size_t len;

  if(!path || !target || !target[0])
    return -1;

  current = target;
  end = target + strlen(target);

  next = strchr(target, ':');
  if(next)
  {
    /**
     * Destination starts with a Windows-style root directory.
     * Aside from Windows, these are often used by console SDKs (albeit with /
     * instead of \) to distinguish SD cards and the like.
     */
    // Make sure this is actually a well-formed absolute path.
    if(!path_is_absolute(target))
      return -1;

    snprintf(buffer, MAX_PATH, "%.*s" DIR_SEPARATOR, (int)(next - target + 1),
     target);
    buffer[MAX_PATH - 1] = '\0';

    if(vstat(buffer, &stat_info) < 0)
      return -1;

    current = next + 1;
    if(isslash(current[0]))
      current++;
  }
  else

  if(isslash(target[0]))
  {
    /**
     * Destination starts with a Unix-style root directory.
     * Aside from Unix-likes, these are also supported by console platforms.
     * Even Windows (back through XP at least) doesn't seem to mind them.
     */
    snprintf(buffer, MAX_PATH, DIR_SEPARATOR);
    buffer[MAX_PATH - 1] = '\0';
    current = target + 1;
  }

  else
  {
    /**
     * Destination is relative--start from the current path. Make sure there's
     * a trailing separator.
     */
    len = path_clean_slashes_copy(buffer, MAX_PATH, path);
    if(!len)
      return -1;

    if(!isslash(buffer[len - 1]) && len + 1 < MAX_PATH)
    {
      buffer[len++] = DIR_SEPARATOR_CHAR;
      buffer[len] = '\0';
    }
  }

  current_char = current[0];
  len = strlen(buffer);

  // Apply directory fragments to the path.
  while(current_char != '\0')
  {
    // Increment next to skip the separator so it will be copied over.
    next = strpbrk(current, "/\\");
    if(!next) next = end;
    else      next++;

    // . does nothing, .. goes back one level
    if(current_char == '.')
    {
      if(current[1] == '.')
      {
        // Skip the rightmost separator (current level) and look for the
        // previous separator. If found, truncate the path to it.
        char *pos = buffer + len - 1;
        do
        {
          pos--;
        }
        while(pos >= buffer && !isslash(*pos));

        if(pos >= buffer)
        {
          pos[1] = '\0';
          len = strlen(buffer);
        }
      }
    }
    else
    {
      snprintf(buffer + len, MAX_PATH - len, "%.*s", (int)(next - current),
       current);
      buffer[MAX_PATH - 1] = '\0';
      len = strlen(buffer);
    }

    current = next;
    current_char = current[0];
  }

  // This needs to be done before the stat for some platforms (e.g. 3DS)
  len = path_clean_slashes(buffer, MAX_PATH);
  if(len < path_len && vstat(buffer, &stat_info) >= 0 &&
   S_ISDIR(stat_info.st_mode) && !vaccess(buffer, R_OK|X_OK))
  {
    memcpy(path, buffer, len + 1);
    path[path_len - 1] = '\0';
    return len;
  }

  return -1;
}

/**
 * Create the parent directory of a given filename if it doesn't exist
 * (similar to mkdir -p). This function will call vstat multiple times and
 * will call vmkdir to create directories as needed. This function will not
 * make recursive calls.
 *
 * @param filename    Filename to create the parent directory of.
 * @return            `0` on success or a non-zero value on error.
 *                    See `enum path_create_error`.
 */
enum path_create_error path_create_parent_recursively(const char *filename)
{
  struct stat stat_info;
  char parent_directory[MAX_PATH];
  ssize_t pos;

  ssize_t parent_len = path_get_directory(parent_directory, MAX_PATH, filename);
  if(parent_len < 0)
    return PATH_CREATE_ERR_BUFFER;

  // No parent directory? Don't need to do anything...
  if(parent_len == 0)
    return PATH_CREATE_SUCCESS;

  /**
   * Step 1: walk the parent directory backwards and stat it successively until
   * it finds something that exists. Replace slashes with nuls; they can be
   * added back when needed again.
   */
  pos = parent_len;
  do
  {
    parent_directory[pos] = '\0';
    if(vstat(parent_directory, &stat_info))
    {
      // Make sure the error is that the dir is missing and not something else.
      if(errno != ENOENT)
        return PATH_CREATE_ERR_STAT_ERROR;
    }
    else

    /**
     * If a file exists where a directory needs to be placed there isn't
     * really anything else that can be done.
     */
    if(!S_ISDIR(stat_info.st_mode))
    {
      return PATH_CREATE_ERR_FILE_EXISTS;
    }
    else
      break;

    // Find the next slash.
    while(pos > 0 && !isslash(parent_directory[pos]))
      pos--;
  }
  while(pos > 0);

  // The entire path already exists? Nothing else needs to be done...
  if(pos == parent_len)
    return PATH_CREATE_SUCCESS;

  /**
   * Step 2: restore slashes and mkdir until the original end of the parent
   * directory is found.
   */
  while(pos < parent_len)
  {
    /**
     * If pos==0, the base of the path didn't exist and needs to be created.
     * Otherwise, look for slashes that were removed, fix them, and create
     * the next directory.
     */
    if(!pos || !parent_directory[pos])
    {
      if(!parent_directory[pos])
        parent_directory[pos] = DIR_SEPARATOR_CHAR;

      if(vmkdir(parent_directory, 0755))
        return PATH_CREATE_ERR_MKDIR_FAILED;
    }

    while(pos < parent_len && parent_directory[pos])
      pos++;
  }
  return PATH_CREATE_SUCCESS;
}
