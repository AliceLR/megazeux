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

static inline int memcasecmp(const void *A, const void *B, size_t cmp_length)
{
  // Compare 32bits at a time, should be mostly portable now
  const char *src_value = (const char *)A;
  const char *dest_value = (const char *)B;
  char val_src;
  char val_dest;
  size_t length_32b = cmp_length / 4;
  uint32_t *src_value_32b = (uint32_t *)A;
  uint32_t *dest_value_32b = (uint32_t *)B;
  uint32_t val_src_32b;
  uint32_t val_dest_32b;
  uint32_t difference;
  size_t i;

  for(i = 0; i < length_32b; i++)
  {
    val_src_32b = src_value_32b[i];
    val_dest_32b = dest_value_32b[i];

    if(val_src_32b != val_dest_32b)
    {
      difference =
       toupper(val_dest_32b >> 24) - toupper(val_src_32b >> 24);

      if(difference)
        return difference;

      difference = toupper((val_dest_32b >> 16) & 0xFF) -
       toupper((val_src_32b >> 16) & 0xFF);

      if(difference)
        return difference;

      difference = toupper((val_dest_32b >> 8) & 0xFF) -
       toupper((val_src_32b >> 8) & 0xFF);

      if(difference)
        return difference;

      difference = toupper(val_dest_32b & 0xFF) -
       toupper(val_src_32b & 0xFF);

      if(difference)
        return difference;
    }
  }

  for(i = length_32b * 4; i < cmp_length; i++)
  {
    val_src = src_value[i];
    val_dest = dest_value[i];

    if(val_src != val_dest)
    {
      difference = toupper((int)val_dest) - toupper((int)val_src);

      if(difference)
        return difference;
    }
  }

  return 0;
}

#endif // __MEMCASECMP_H
