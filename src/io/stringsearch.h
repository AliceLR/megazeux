/* MegaZeux
 *
 * Copyright (C) 2012 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __IO_STRINGSEARCH_H
#define __IO_STRINGSEARCH_H

#include "../compat.h"

__M_BEGIN_DECLS

#include <stddef.h>

struct string_search_data
{
  unsigned int index[256];
};

UTILS_LIBSPEC void string_search_index(const void *B, const size_t b_len,
 struct string_search_data *data, boolean ignore_case);
UTILS_LIBSPEC const void *string_search(const void *A, const size_t a_len,
 const void *B, const size_t b_len, const struct string_search_data *data,
 boolean ignore_case);

__M_END_DECLS

#endif /* __IO_STRINGSEARCH_H */
