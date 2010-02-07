/* MegaZeux
 *
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
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

// Provide some compatibility macros for combined C/C++ binary

#ifndef __COMPAT_H
#define __COMPAT_H

#ifdef __cplusplus
#define __M_BEGIN_DECLS extern "C" {
#define __M_END_DECLS   }
#else
#define __M_BEGIN_DECLS
#define __M_END_DECLS
#define true 1
#define false 0
#endif /* __cplusplus */

#ifdef _MSC_VER
#define inline
#endif

#include <time.h>

// Random function, returns an integer between 0 and Range
static inline int Random(int Range)
{
  static unsigned long long seed = 0;
  unsigned long long value;

  // If the seed is 0, initialise it with time and clock
  if (seed == 0)
  {
    seed = time(NULL) + clock();
  }

  seed = seed * 1664525 + 1013904223;

  value = (seed & 0xFFFFFFFF) * Range / 0xFFFFFFFF;
  return (int)value;
}

#endif // __COMPAT_H
