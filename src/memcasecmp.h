/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef __MEMCASECMP_H
#define __MEMCASECMP_H

#include <inttypes.h>
#include "platform_endian.h"

static inline int memcasecmp(const void *A, const void *B, size_t cmp_length)
{
  // Compare 32bits at a time, should be mostly portable now
  const char *a_value = (const char *)A;
  const char *b_value = (const char *)B;
  char val_a;
  char val_b;
  size_t length_32b = cmp_length / 4;
  uint32_t *a_value_32b = (uint32_t *)A;
  uint32_t *b_value_32b = (uint32_t *)B;
  uint32_t val_a_32b;
  uint32_t val_b_32b;
  uint32_t difference;
  size_t i;

  for(i = 0; i < length_32b; i++)
  {
    val_a_32b = a_value_32b[i];
    val_b_32b = b_value_32b[i];

    if(val_a_32b != val_b_32b)
    {
#if PLATFORM_BYTE_ORDER == PLATFORM_LIL_ENDIAN

      difference = toupper(val_a_32b & 0xFF) -
       toupper(val_b_32b & 0xFF);

      if(difference)
        return difference;

      difference = toupper((val_a_32b >> 8) & 0xFF) -
       toupper((val_b_32b >> 8) & 0xFF);

      if(difference)
        return difference;

      difference = toupper((val_a_32b >> 16) & 0xFF) -
       toupper((val_b_32b >> 16) & 0xFF);

      if(difference)
        return difference;

      difference =
       toupper(val_a_32b >> 24) - toupper(val_b_32b >> 24);

      if(difference)
        return difference;

#else // PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN

      difference =
       toupper(val_a_32b >> 24) - toupper(val_b_32b >> 24);

      if(difference)
        return difference;

      difference = toupper((val_a_32b >> 16) & 0xFF) -
       toupper((val_b_32b >> 16) & 0xFF);

      if(difference)
        return difference;

      difference = toupper((val_a_32b >> 8) & 0xFF) -
       toupper((val_b_32b >> 8) & 0xFF);

      if(difference)
        return difference;

      difference = toupper(val_a_32b & 0xFF) -
       toupper(val_b_32b & 0xFF);

      if(difference)
        return difference;
#endif
    }
  }

  for(i = length_32b * 4; i < cmp_length; i++)
  {
    val_a = a_value[i];
    val_b = b_value[i];

    if(val_a != val_b)
    {
      difference = toupper((int)val_a) - toupper((int)val_b);

      if(difference)
        return difference;
    }
  }

  return 0;
}

#endif // __MEMCASECMP_H
