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
 * Test the alignment of several name fields in structs.
 * If enough inputs into memcmp and memcasecmp are consistently 4 aligned,
 * this allows faster comparisons to be performed and also allows some code
 * to assume alignment will be the general case.
 */

#include "../Unit.hpp"
#include "../../src/io/bitstream.h"

#include <stdio.h>

struct read_sequence
{
  uint16_t length;
  int expected_value;
};

/**
 * Test reading large numbers of bits similar to how LZW codes are stored in
 * Shrink compression. Shrink stops at 13-bit codes but test up to 16 anyway.
 */
UNITTEST(UnShrinkLike)
{
  static const unsigned char data[] =
  {
    // 9 bits
    // 1 2 3 4 5 6 255 511
    0x01, 0x04, 0x0C, 0x20, 0x50, 0xC0, 0xC0, 0xBF, 0xFF,

    // 12 bits
    // 4080 255 2730 1365 0 4095
    0xF0, 0xFF, 0x0F, 0xAA, 0x5A, 0x55, 0x00, 0xF0, 0xFF,

    // 16 bits
    // 256 1 43497 60659
    0xE9, 0xA9, 0x00, 0x01, 0x01, 0x00, 0xF3, 0xEC,
  };
  static const int expected9[] =
  {
    1, 2, 3, 4, 5, 6, 255, 511,
  };
  static const int expected12[] =
  {
    4080, 255, 2730, 1365, 0, 4095,
  };
  static const int expected16[] =
  {
    43497, 256, 1, 60659,
  };

  struct bitstream b{};
  int out;
  int i;

  b.input = data;
  b.input_left = arraysize(data);

  for(i = 0; i < arraysize(expected9); i++)
  {
    out = BS_READ(&b, 9);
    ASSERTEQ(out, expected9[i], "code size 9, position %d", i);
  }

  for(i = 0; i < arraysize(expected12); i++)
  {
    out = BS_READ(&b, 12);
    ASSERTEQ(out, expected12[i], "code size 12, position %d", i);
  }

  for(i = 0; i < arraysize(expected16); i++)
  {
    out = BS_READ(&b, 16);
    ASSERTEQ(out, expected16[i], "code size 16, position %d", i);
  }

  ASSERTEQ(b.input, nullptr, "");
  ASSERTEQ(b.input_left, 0, "");

  // NOTE: this won't always be true as streams are byte-aligned, but this
  // test data has been designed so that this should be true.
  ASSERTEQ(b.buf_left, 0, "");
}

/**
 * Test reading smaller numbers of bits, like might be expected from an
 * Implode compression stream.
 */
UNITTEST(ExplodeLike)
{
  static const unsigned char data[] =
  {
    0xFF, 0x12, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD,
    0xFE, 0x02, 0x5A, 0x1F, 0x24,
    0x62, 0x14, 0xA5, 0x3F,
    0xFF, 0x05, 0xF8, 0xA9,
  };
  static constexpr const read_sequence sequence[] =
  {
    { 8, 0xFF },
    { 8, 0x12 },
    { 8, 0x23 },
    { 8, 0x45 },
    { 8, 0x67 },
    { 8, 0x89 },
    { 8, 0xAB },
    { 8, 0xCD },

    { 1, 0 }, { 8, 0x7F, },
    { 1, 1 }, { 7, 0x00, }, { 1, 1 }, { 1, 0 }, { 1, 1 },
      { 1, 1 }, { 1, 0 }, { 1, 1 }, { 1, 0 },
    { 1, 1 }, { 7, 0x0F, },
    { 1, 0 }, { 7, 0x12, },

    { 8, 0x62 },
    { 8, 0x14 },
    { 8, 0xA5 },
    { 8, 0x3F },

    { 1, 1 }, { 8, 0xFF }, { 1, 0 }, { 1, 1 }, { 1, 0 }, { 8, 0x80 },
    { 1, 1 }, { 8, 0x4F }, { 1, 1 }, { 1, 0 }, { 1, 1 },
  };

  struct bitstream b{};
  int out;
  int i;

  b.input = data;
  b.input_left = arraysize(data);

  for(i = 0; i < arraysize(sequence); i++)
  {
    out = BS_READ(&b, sequence[i].length);
    ASSERTEQ(out, sequence[i].expected_value, "position %d, %u bit(s)", i,
     sequence[i].length);
  }

  ASSERTEQ(b.input, nullptr, "");
  ASSERTEQ(b.input_left, 0, "");

  // NOTE: this won't always be true as streams are byte-aligned, but this
  // test data has been designed so that this should be true.
  ASSERTEQ(b.buf_left, 0, "");
}

/**
 * Make sure out-of-bounds reads always return EOF.
 */
UNITTEST(OutOfBounds)
{
  static const unsigned char data[][8] =
  {
    {
      0xFF, 0x00, 0xAB, 0x00, 0x0B, 0xB0, 0x00, 0x00,
    },
    {
      0x15, 0x60, 0x55, 0xAA, 0x11, 0x22, 0x33, 0x44,
    },
    {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
    {
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    },
  };
  static constexpr const read_sequence sequence[][16] =
  {
    {
      { 16, 0x00FF },
      { 16, 0x00AB },
      { 16, 0xB00B },
      { 16, 0x0000 },
      { 16, EOF    },
      { 16, EOF    },
    },
    {
      { 8, 0x15 },
      { 8, 0x60 },
      { 8, 0x55 },
      { 8, 0xAA },
      { 8, 0x11 },
      { 8, 0x22 },
      { 8, 0x33 },
      { 8, 0x44 },
      { 8, EOF  },
      { 8, EOF  },
    },
    {
      { 13, 0x0000 },
      { 13, 0x0000 },
      { 13, 0x0000 },
      { 13, 0x0000 },
      { 13, EOF },
      { 13, EOF },
    },
    {
      { 5, 0x1F },
      { 5, 0x1F },
      { 5, 0x1F },
      { 5, 0x1F },
      { 5, 0x1F },
      { 5, 0x1F },
      { 5, 0x1F },
      { 5, 0x1F },
      { 5, 0x1F },
      { 5, 0x1F },
      { 5, 0x1F },
      { 5, 0x1F },
      { 5, EOF  },
      { 5, EOF  },
    },
  };
  samesize(data, sequence);

  int out;
  int i;
  int j;

  for(i = 0; i < arraysize(sequence); i++)
  {
    struct bitstream b{};

    b.input = data[i];
    b.input_left = arraysize(data[i]);

    for(j = 0; i < arraysize(sequence[i]) && sequence[i][j].length; j++)
    {
      out = BS_READ(&b, sequence[i][j].length);
      ASSERTEQ(out, sequence[i][j].expected_value,
       "array %d, position %d, %u bit(s)", i, j, sequence[i][j].length);
    }

    ASSERTEQ(b.input, nullptr, "");
    ASSERTEQ(b.input_left, 0, "");

    // NOTE: may be bits left in this test, so don't try to test it...
  }
}
