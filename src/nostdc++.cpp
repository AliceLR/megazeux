/* MegaZeux
 *
 * Copyright (C) 2020 Alice Rowan <petrifiedrowan@gmail.com>
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

/**
 * Compatibility functions for C++ when not linking libstdc++/libc++.
 * For platforms (Xcode, Nintendo Switch) or configurations (currently none)
 * that require linking one of those, this file can be disregarded.
 */

#include "compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <new>

void __cxa_pure_virtual()
{
  fprintf(stderr, "Attempted to call pure virtual function! Aborting!\n");
  fflush(stderr);
  exit(1);
}

void __cxa_deleted_virtual()
{
  fprintf(stderr, "Attempted to call deleted virtual function! Aborting!\n");
  fflush(stderr);
  exit(1);
}

void *operator new(size_t count) noexcept
{
  void *ptr = cmalloc(count);
  if(!ptr)
  {
    // Allocation failed in thread--abort!
    fprintf(stderr, "Failed to allocate memory for new! Aborting!\n");
    fflush(stderr);
    exit(1);
  }
  return ptr;
}

void *operator new[](size_t count) noexcept
{
  void *ptr = cmalloc(count);
  if(!ptr)
  {
    // Allocation failed in thread--abort!
    fprintf(stderr, "Failed to allocate memory for new[]! Aborting!\n");
    fflush(stderr);
    exit(1);
  }
  return ptr;
}

void operator delete(void *ptr) noexcept
{
  free(ptr);
}

void operator delete[](void *ptr) noexcept
{
  free(ptr);
}
