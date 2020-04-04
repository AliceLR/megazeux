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

#ifndef __IO_MEMCASECMP_H
#define __IO_MEMCASECMP_H

#include "compat.h"

__M_BEGIN_DECLS

#include <inttypes.h>
#include "platform_endian.h"

static const unsigned char memtolower_table[256] =
{
   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,
  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,
  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,
  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,
  64,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122,  91,  92,  93,  94,  95,
  96,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
};

static inline int memtolower(unsigned char c)
{
  return memtolower_table[c];
}

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

      difference = memtolower(val_a_32b & 0xFF) -
       memtolower(val_b_32b & 0xFF);

      if(difference)
        return difference;

      difference = memtolower((val_a_32b >> 8) & 0xFF) -
       memtolower((val_b_32b >> 8) & 0xFF);

      if(difference)
        return difference;

      difference = memtolower((val_a_32b >> 16) & 0xFF) -
       memtolower((val_b_32b >> 16) & 0xFF);

      if(difference)
        return difference;

      difference =
       memtolower(val_a_32b >> 24) - memtolower(val_b_32b >> 24);

      if(difference)
        return difference;

#else // PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN

      difference =
       memtolower(val_a_32b >> 24) - memtolower(val_b_32b >> 24);

      if(difference)
        return difference;

      difference = memtolower((val_a_32b >> 16) & 0xFF) -
       memtolower((val_b_32b >> 16) & 0xFF);

      if(difference)
        return difference;

      difference = memtolower((val_a_32b >> 8) & 0xFF) -
       memtolower((val_b_32b >> 8) & 0xFF);

      if(difference)
        return difference;

      difference = memtolower(val_a_32b & 0xFF) -
       memtolower(val_b_32b & 0xFF);

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
      difference = memtolower((int)val_a) - memtolower((int)val_b);

      if(difference)
        return difference;
    }
  }

  return 0;
}

__M_END_DECLS

#endif // __IO_MEMCASECMP_H
