/* MegaZeux
 *
 * Alternative CLI check alloc implementations for utils derived from util.c.
 * These work without linking util or hacking in an error() replacement.
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2017 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __UTILS_CHECKALLOC_H
#define __UTILS_CHECKALLOC_H

#ifdef CONFIG_CHECK_ALLOC

static inline void out_of_memory_check(void *p, const char *file, int line)
{
  if(!p)
  {
    fprintf(stderr, "Out of memory in %s:%d", file, line);
    exit(-1);
  }
}

CORE_LIBSPEC
void *check_calloc(size_t count, size_t size, const char *file, int line)
{
  void *result = calloc(count, size);
  out_of_memory_check(result, file, line);
  return result;
}

CORE_LIBSPEC
void *check_malloc(size_t size, const char *file, int line)
{
  void *result = malloc(size);
  out_of_memory_check(result, file, line);
  return result;
}

CORE_LIBSPEC
void *check_realloc(void *ptr, size_t size, const char *file, int line)
{
  void *result = realloc(ptr, size);
  out_of_memory_check(result, file, line);
  return result;
}

#endif // CONFIG_CHECK_ALLOC

#endif // __UTILS_CHECKALLOC_H
