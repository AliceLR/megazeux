/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef __IO_PATH_H
#define __IO_PATH_H

#include "../compat.h"

__M_BEGIN_DECLS

#include <stddef.h>

#ifndef DIR_SEPARATOR
#if defined(__WIN32__) || defined(CONFIG_DJGPP)
#define DIR_SEPARATOR "\\"
#define DIR_SEPARATOR_CHAR '\\'
#else /* !__WIN32__ && !CONFIG_DJGPP */
#define DIR_SEPARATOR "/"
#define DIR_SEPARATOR_CHAR '/'
#endif
#endif /* DIR_SEPARATOR */

enum path_create_error
{
  PATH_CREATE_SUCCESS,
  PATH_CREATE_ERR_BUFFER,
  PATH_CREATE_ERR_STAT_ERROR,
  PATH_CREATE_ERR_MKDIR_FAILED,
  PATH_CREATE_ERR_FILE_EXISTS
};

/**
 * Determine if a character is a path separating slash.
 * @param  chr  Character to check.
 * @return      True if the character is a slash, otherwise false.
 */
static inline boolean isslash(const char chr)
{
  return (chr == '\\') || (chr == '/');
}

UTILS_LIBSPEC char *path_tokenize(char **next);
UTILS_LIBSPEC char *path_reverse_tokenize(char **base, size_t *base_len);

UTILS_LIBSPEC boolean path_force_ext(char *path, size_t buffer_len, const char *ext);
UTILS_LIBSPEC ssize_t path_get_ext_offset(const char *path);

UTILS_LIBSPEC ssize_t path_is_absolute(const char *path);
UTILS_LIBSPEC boolean path_is_root(const char *path);
UTILS_LIBSPEC boolean path_has_directory(const char *path);
UTILS_LIBSPEC ssize_t path_to_directory(char *path, size_t buffer_len);
UTILS_LIBSPEC ssize_t path_to_filename(char *path, size_t buffer_len);
UTILS_LIBSPEC ssize_t path_get_directory(char *dest, size_t dest_len,
 const char *path);
UTILS_LIBSPEC ssize_t path_get_filename(char *dest, size_t dest_len,
 const char *path);
UTILS_LIBSPEC boolean path_get_directory_and_filename(char *d_dest, size_t d_len,
 char *f_dest, size_t f_len, const char *path);
UTILS_LIBSPEC ssize_t path_get_parent(char *dest, size_t dest_len,
 const char *path);

UTILS_LIBSPEC size_t path_clean_slashes(char *path, size_t path_len);
UTILS_LIBSPEC size_t path_clean_slashes_copy(char *dest, size_t dest_len,
 const char *path);
UTILS_LIBSPEC ssize_t path_append(char *path, size_t buffer_len, const char *rel);
UTILS_LIBSPEC ssize_t path_join(char *dest, size_t dest_len, const char *base,
 const char *rel);
UTILS_LIBSPEC ssize_t path_remove_prefix(char *path, size_t buffer_len,
 const char *prefix, size_t prefix_len);

UTILS_LIBSPEC ssize_t path_navigate(char *path, size_t path_len,
 const char *target);
UTILS_LIBSPEC ssize_t path_navigate_no_check(char *path, size_t path_len,
 const char *target);

UTILS_LIBSPEC enum path_create_error path_create_parent_recursively(
 const char *filename);

__M_END_DECLS

#endif /* __IO_PATH_H */
