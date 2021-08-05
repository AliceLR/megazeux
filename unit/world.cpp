/* MegaZeux
 *
 * Copyright (C) 2021 Alice Rowan <petrifiedrowan@gmail.com>
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
 * Unit tests for misc. world format functionality. More comprehensive world
 * world loader testing for valid worlds is performed by the test worlds.
 */

#include "Unit.hpp"

#include "../src/world.h"
#include "../src/world_format.h"
#include "../src/io/memfile.h"
#include "../src/io/zip.h"

#include <memory>

/**
 * world.c functions.
 */

struct world_magic_data
{
  const char *world_magic;
  const char *save_magic;
  const char *version_string;
  int expected;
};

UNITTEST(Magic)
{
  static const char UNKNOWN[] = "<unknown 0000h>";
  static const world_magic_data magic_data[] =
  {
    // Valid (DOS).
    { "MZX",        "MZSAV",        "1.xx",         V100   },
    { "MZ2",        "MZSV2",        "2.xx/2.51",    V200   },
    { "MZA",        "MZXSA",        "2.51s1",       V251s1 },
    { "M\x02\x09",  "MZS\x02\x09",  "2.51s2/2.61",  V251s2 },
    { "M\x02\x11",  nullptr,        "<decrypted>",  VERSION_DECRYPT }, // Only generated by world decryption.
    { "M\x02\x32",  "MZS\x02\x32",  "2.62/2.62b",   V262 },
    { "M\x02\x41",  "MZS\x02\x41",  "2.65/2.68",    V265 },
    { "M\x02\x44",  "MZS\x02\x44",  "2.68",         V268 },
    { "M\x02\x45",  "MZS\x02\x45",  "2.69",         V269 },
    { "M\x02\x46",  "MZS\x02\x46",  "2.69b",        V269b },
    { "M\x02\x48",  "MZS\x02\x48",  "2.69c",        V269c },
    { "M\x02\x49",  "MZS\x02\x49",  "2.70",         V270 },

    // Valid (port).
    { "M\x02\x50",  "MZS\x02\x50",  "2.80",   V280 },
    { "M\x02\x51",  "MZS\x02\x51",  "2.81",   V281 },
    { "M\x02\x52",  "MZS\x02\x52",  "2.82",   V282 },
    { "M\x02\x53",  "MZS\x02\x53",  "2.83",   V283 },
    { "M\x02\x54",  "MZS\x02\x54",  "2.84",   V284 },
    { "M\x02\x5A",  "MZS\x02\x5A",  "2.90",   V290 },
    { "M\x02\x5B",  "MZS\x02\x5B",  "2.91",   V291 },
    { "M\x02\x5C",  "MZS\x02\x5C",  "2.92",   V292 },
    { "M\x02\x5D",  "MZS\x02\x5D",  "2.93",   0x025D },
    { "M\x02\x5E",  "MZS\x02\x5E",  "2.94",   0x025E },
    { "M\x02\x5F",  "MZS\x02\x5F",  "2.95",   0x025F },
    { "M\x03\x00",  "MZS\x03\x00",  "3.00",   0x0300 },
    { "M\x09\xFF",  "MZS\x09\xFF",  "9.255",  0x09FF },

    // Invalid.
    { "M\x01\x00",  "MZS\x01\x00",  UNKNOWN,  0 },
    { "MZB",        "MZXZB",        UNKNOWN,  0 },
    { "MZM",        "MZSZM",        UNKNOWN,  0 },
    { "MTM",        "MZDTM",        UNKNOWN,  0 },
    { "MOD",        "MOD",          UNKNOWN,  0 },
    { "NZX",        "NNNN",         UNKNOWN,  0 },
    { "\xff\x00",   "\xff\x00",     UNKNOWN,  0 },
    { "",           "",             UNKNOWN,  0 },
    { "M",          "M",            UNKNOWN,  0 },
    { "MZ",         "MZ",           UNKNOWN,  0 },
    { nullptr,      "MZS",          UNKNOWN,  0 },
    { nullptr,      "MZSV",         UNKNOWN,  0 },
    { nullptr,      "MZX",          UNKNOWN,  0 },
    { nullptr,      "MZXS",         UNKNOWN,  0 },
  };

  SECTION(world_magic)
  {
    for(const world_magic_data &d : magic_data)
    {
      if(!d.world_magic)
        continue;

      int value = world_magic(d.world_magic);
      ASSERTEQ(value, d.expected, "%s", d.world_magic);
    }
  }

  SECTION(save_magic)
  {
    for(const world_magic_data &d : magic_data)
    {
      if(!d.save_magic)
        continue;

      int value = save_magic(d.save_magic);
      ASSERTEQ(value, d.expected, "%s", d.save_magic);
    }
  }

  SECTION(get_version_string)
  {
    char buffer[16];
    for(const world_magic_data &d : magic_data)
    {
      size_t len = get_version_string(buffer, (enum mzx_version)d.expected);
      ASSERTCMP(buffer, d.version_string, "");
      ASSERTEQ(len, strlen(buffer), "return value should be length: %s", buffer);
    }
  }
}

/**
 * world_format.h functions.
 */
/*
UNITTEST(world_assign_file_ids)
{
  // FIXME
  UNIMPLEMENTED();
}
*/
UNITTEST(Properties)
{
  uint8_t buffer[32];
  struct memfile mf;
  struct memfile prop;

  mfopen(buffer, sizeof(buffer), &mf);
  memset(buffer, '\xff', sizeof(buffer));

  SECTION(save_prop_eof)
  {
    static const uint8_t expected[] = { '\0', '\0' };
    save_prop_eof(&mf);
    ASSERTMEM(buffer, expected, sizeof(expected), "");
    ASSERTEQ((size_t)mftell(&mf), sizeof(expected), "");
  }

  SECTION(save_prop_c)
  {
    static const uint8_t expected[] = { 'A', 'b', 0x01, '\0', '\0', '\0', 'X' };
    save_prop_c(0x6241, 'X', &mf);
    ASSERTMEM(buffer, expected, sizeof(expected), "");
    ASSERTEQ((size_t)mftell(&mf), sizeof(expected), "");
  }

  SECTION(save_prop_w)
  {
    static const uint8_t expected[] = { 'A', 'b', 0x02, '\0', '\0', '\0', 0x34, 0x12 };
    save_prop_w(0x6241, 0x1234, &mf);
    ASSERTMEM(buffer, expected, sizeof(expected), "");
    ASSERTEQ((size_t)mftell(&mf), sizeof(expected), "");
  }

  SECTION(save_prop_d)
  {
    static const uint8_t expected[] = { 'A', 'b', 0x04, '\0', '\0', '\0', 0x78, 0x56, 0x34, 0x12 };
    save_prop_d(0x6241, 0x12345678, &mf);
    ASSERTMEM(buffer, expected, sizeof(expected), "");
    ASSERTEQ((size_t)mftell(&mf), sizeof(expected), "");
  }

  SECTION(save_prop_s)
  {
    const char *str = "value! 1234567890";
    static const uint8_t expected[] =
    {
      'A', 'b', 0x11, '\0', '\0', '\0',
      'v', 'a', 'l', 'u', 'e', '!', ' ',
      '1', '2', '3', '4', '5', '6', '7', '8', '9', '0'
    };

    save_prop_s(0x6241, str, strlen(str), 1, &mf);
    ASSERTMEM(buffer, expected, sizeof(expected), "");
    ASSERTEQ((size_t)mftell(&mf), sizeof(expected), "");

    mfseek(&mf, 0, SEEK_SET);

    static const uint8_t arr[4][2] =
    {
      { 1, 2, },
      { 3, 4, },
      { 5, 6, },
      { 7, 8, },
    };
    static const uint8_t expected2[] =
    {
      'A', 'b', 0x08, '\0', '\0', '\0',
      1, 2, 3, 4, 5, 6, 7, 8
    };
    save_prop_s(0x6241, arr, 2, 4, &mf);
    ASSERTMEM(buffer, expected2, sizeof(expected2), "");
    ASSERTEQ((size_t)mftell(&mf), sizeof(expected2), "");
  }

  SECTION(save_prop_v)
  {
    const char *str = "value! 1234567890";
    size_t len = strlen(str);
    static const uint8_t expected[] =
    {
      'A', 'b', 0x11, '\0', '\0', '\0',
      'v', 'a', 'l', 'u', 'e', '!', ' ',
      '1', '2', '3', '4', '5', '6', '7', '8', '9', '0'
    };

    save_prop_v(0x6241, len, &prop, &mf);
    size_t ret = mfwrite(str, len, 1, &prop);
    ASSERTEQ(ret, 1, "");

    ASSERTMEM(buffer, expected, sizeof(expected), "");
    ASSERTEQ((size_t)mftell(&mf), sizeof(expected), "");

    // The following tests won't be writing past the property header,
    // but to protect this against checks/asserts...
    std::unique_ptr<uint8_t[]> bigbuffer(new uint8_t[0x20000]);
    mfopen(bigbuffer.get(), 0x20000, &mf);

    static const uint8_t expected2[] = { 0x01, 0x00, 0xff, 0x7f, '\0', '\0' };
    save_prop_v(0x0001, 0x7fff, &prop, &mf);
    ASSERTMEM(bigbuffer.get(), expected2, sizeof(expected2), "");
    ASSERTEQ(mftell(&mf), PROP_HEADER_SIZE + 0x7fff, "");

    mfseek(&mf, 0, SEEK_SET);

    static const uint8_t expected3[] = { 0x01, 0x00, 0xff, 0xff, 0x01, '\0' };
    save_prop_v(0x0001, 0x1ffff, &prop, &mf);
    ASSERTMEM(bigbuffer.get(), expected3, sizeof(expected3), "");
    ASSERTEQ(mftell(&mf), PROP_HEADER_SIZE + 0x1ffff, "");
  }

  SECTION(next_prop)
  {
    static const uint8_t input[] =
    {
      0x01, 0x00, 0x05, 0x00, 0x00, 0x00, 'A', 'B', 'C', 'D', 'E',
      0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF,
      0x00, 0x00,
    };
    boolean ret;
    int ident;
    int length;

    mfopen(input, sizeof(input), &mf);
    ret = next_prop(&prop, &ident, &length, &mf);
    ASSERT(ret, "next_prop (1)");
    ASSERTEQ(ident, 0x0001, "ident (1)");
    ASSERTEQ(length, 0x00000005, "length (1)");
    mfread(buffer, length, 1, &prop);
    ASSERTMEM(buffer, "ABCDE", 5, "value (1)");

    ret = next_prop(&prop, &ident, &length, &mf);
    ASSERT(ret, "next_prop (2)");
    ASSERTEQ(ident, 0x0002, "ident (2)");
    ASSERTEQ(length, 0x00000001, "length (2)");
    mfread(buffer, length, 1, &prop);
    ASSERTMEM(buffer, "\xff", 1, "value (2)");

    ret = next_prop(&prop, &ident, &length, &mf);
    ASSERT(ret == false, "next_prop (EOF)");

    // Currently doesn't actually read the EOF, do it manually.
    uint16_t value = mfgetw(&mf);
    ASSERTEQ(value, 0x0000, "ident (EOF)");
    ASSERTEQ((size_t)mftell(&mf), sizeof(input), "final pos");

    // Truncated header.
    mfopen(input, PROP_HEADER_SIZE - 2, &mf);
    ret = next_prop(&prop, &ident, &length, &mf);
    ASSERT(ret == false, "next_prop (truncated header)");

    // Truncated value.
    mfopen(input, PROP_HEADER_SIZE + 2, &mf);
    ret = next_prop(&prop, &ident, &length, &mf);
    ASSERT(ret == false, "next_prop (truncated value)");
  }

  SECTION(load_prop_int)
  {
    struct load_prop_int_data
    {
      int ident;
      int length;
      int value;
    };

    static const uint8_t input[] =
    {
      0x00, 0xff, 0x00, 0x00, 0x00, 0x00,
      0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 'a',
      0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 'b', 'b',
      0x03, 0x03, 0x03, 0x00, 0x00, 0x00, 'c', 'c', 'c',
      0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 'd', 'd', 'd', 'd',
      0x05, 0x05, 0x05, 0x00, 0x00, 0x00, 'e', 'e', 'e', 'e', 'e',
      0x00, 0x00,
    };
    static const struct load_prop_int_data expected[] =
    {
      { 0xff00, 0, 0 },
      { 0x0101, 1, 'a' },
      { 0x0202, 2, ('b' << 8) | 'b' },
      { 0x0303, 3, 0 },
      { 0x0404, 4, ('d' << 24) | ('d' << 16) | ('d' << 8) | 'd', },
      { 0x0505, 5, 0 },
    };
    boolean ret;
    int ident;
    int length;
    int value;

    mfopen(input, sizeof(input), &mf);

    for(const load_prop_int_data &d : expected)
    {
      ret = next_prop(&prop, &ident, &length, &mf);
      ASSERT(ret, "%04x", d.ident);

      ASSERTEQ(ident, d.ident, "%04x", d.ident);
      ASSERTEQ(length, d.length, "%04x", d.ident);

      value = load_prop_int(length, &prop);
      ASSERTEQ(value, d.value, "%04x", d.ident);
    }

    ret = next_prop(&prop, &ident, &length, &mf);
    ASSERT(ret == false, "EOF");
    value = mfgetw(&mf);
    ASSERTEQ(value, 0x0000, "ident (EOF)");
  }
}
