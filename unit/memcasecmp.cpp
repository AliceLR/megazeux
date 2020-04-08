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

#include "Unit.hpp"
#include "../src/memcasecmp.h"

#include <string.h>
#include <tuple>

struct string_pair
{
  const char *a;
  const char *b;
};

struct string_pair_aligned
{
  alignstr<uint64_t> a;
  alignstr<uint64_t> b;
};

struct bad_string_pair
{
  const char *a;
  const char *b;
  int expected;
};

struct bad_string_pair_aligned
{
  alignstr<uint64_t> a;
  alignstr<uint64_t> b;
  int expected;
};

UNITTEST(memtolower)
{
  int i;
  for(i = 0; i < 256; i++)
  {
    if(i >= 'A' && i <= 'Z')
      ASSERTEQ(memtolower(i), (i - 'A' + 'a'));
    else
      ASSERTEQ(memtolower(i), i);
  }
}

UNITTEST(Matching)
{
  static const string_pair pairs[] =
  {
    { "", "" },
    { " ", " " },
    { "abcde", "ABCDE" },
    { "testTESTtest", "TESTtestTEST" },
    { "@!%$*^!@$&#!*([]", "@!%$*^!@$&#!*([]" },
    { u8"śśśśśśśś", u8"śśśśśśśś" },
  };

  static const string_pair_aligned pairs64[] =
  {
    { "", "" },
    { " ", " " },
    { "abcde", "ABCDE" },
    { "testTESTtestTEST", "TESTtestTESTtest" },
    { "aaaaaaaabaaaaaaabaaaaaaabaaaaaaa", "aaaaaaaabaaaaaaabaaaaaaabaaaaaaa" },
    { "aaaaaaaabaaaaaaaBAAAAAAAbaaaaaaa", "aaaaaaaaBAAAAAAAbaaaaaaabaaaaaaa" },
    { "@!%$*^!@$&#!*([]", "@!%$*^!@$&#!*([]" },
    { u8"śśśśśśśś", u8"śśśśśśśś" },
  };
  const char *a;
  const char *b;
  int i;

  SECTION(memcasecmp)
  {
    for(i = 0; i < arraysize(pairs); i++)
    {
      a = pairs[i].a;
      b = pairs[i].b;
      ASSERTEQX(strlen(a), strlen(b), a);
      ASSERTX(!memcasecmp(a, b, strlen(a)), a);
    }

    for(i = 0; i < arraysize(pairs64); i++)
    {
      a = pairs64[i].a.c_str();
      b = pairs64[i].b.c_str();

      ASSERTEQ((size_t)a & 0x7, 0);
      ASSERTEQ((size_t)b & 0x7, 0);
      ASSERTEQX(strlen(a), strlen(b), a);
      ASSERTX(!memcasecmp(a, b, strlen(a)), a);
    }
  }

  SECTION(memcasecmp32)
  {
    for(i = 0; i < arraysize(pairs); i++)
    {
      a = pairs[i].a;
      b = pairs[i].b;
      ASSERTEQX(strlen(a), strlen(b), a);
      ASSERTX(!memcasecmp32(a, b, strlen(a)), a);
    }

    for(i = 0; i < arraysize(pairs64); i++)
    {
      a = pairs64[i].a.c_str();
      b = pairs64[i].b.c_str();

      ASSERTEQ((size_t)a & 0x3, 0);
      ASSERTEQ((size_t)b & 0x3, 0);
      ASSERTEQX(strlen(a), strlen(b), a);
      ASSERTX(!memcasecmp32(a, b, strlen(a)), a);
    }
  }
}

UNITTEST(NoMatch)
{
  static const bad_string_pair pairs[] =
  {
    { "-", "_", ('-' - '_') },
    { "abcde", "ABCDF", ('e' - 'f') },
    { "testTESTtest", "TASTtastTAST", ('e' - 'a') },
    { "@!%$*^!@$&#!*([a", "@!%$*^!@$&#!*([b", ('a' - 'b') },
    { u8"śśŚśśśŚś", u8"ŚśśśŚśśś", 1 },
  };

  static const bad_string_pair_aligned pairs64[] =
  {
    { "-", "_", ('-' - '_') },
    { "testTESTtestTEST", "TASTtastTASTtast", ('e' - 'a') },
    { "aaaaaaaa*aaaaaaaBAAAAAAA", "aaaaaaaaBAAAAAAAbaaaaaaa", ('*' - 'b') },
    { "aaaaaaaab*aaaaaaBAAAAAAA", "aaaaaaaaBAAAAAAAbaaaaaaa", ('*' - 'a') },
    { "aaaaaaaaba*aaaaaBAAAAAAA", "aaaaaaaaBAAAAAAAbaaaaaaa", ('*' - 'a') },
    { "aaaaaaaabaa*aaaaBAAAAAAA", "aaaaaaaaBAAAAAAAbaaaaaaa", ('*' - 'a') },
    { "aaaaaaaabaaa*aaaBAAAAAAA", "aaaaaaaaBAAAAAAAbaaaaaaa", ('*' - 'a') },
    { "aaaaaaaabaaaa*aaBAAAAAAA", "aaaaaaaaBAAAAAAAbaaaaaaa", ('*' - 'a') },
    { "aaaaaaaabaaaaa*aBAAAAAAA", "aaaaaaaaBAAAAAAAbaaaaaaa", ('*' - 'a') },
    { "aaaaaaaabaaaaaa*BAAAAAAA", "aaaaaaaaBAAAAAAAbaaaaaaa", ('*' - 'a') },
    { "@!%$*^!@$&#!*([a", "@!%$*^!@$&#!*([b", ('a' - 'b') },
    { u8"śśŚśśśŚś", u8"ŚśśśŚśśś", 1 },
  };
  const char *a;
  const char *b;
  int expected;
  int i;

  SECTION(memcasecmp)
  {
    for(i = 0; i < arraysize(pairs); i++)
    {
      a = pairs[i].a;
      b = pairs[i].b;
      expected = pairs[i].expected;
      ASSERTEQX(strlen(a), strlen(b), a);
      ASSERTEQX(memcasecmp(a, b, strlen(a)), expected, a);
    }

    for(i = 0; i < arraysize(pairs64); i++)
    {
      a = pairs64[i].a.c_str();
      b = pairs64[i].b.c_str();
      expected = pairs64[i].expected;

      ASSERTEQ((size_t)a & 0x7, 0);
      ASSERTEQ((size_t)b & 0x7, 0);
      ASSERTEQX(strlen(a), strlen(b), a);
      ASSERTEQX(memcasecmp(a, b, strlen(a)), expected, a);
    }
  }

  SECTION(memcasecmp32)
  {
    for(i = 0; i < arraysize(pairs); i++)
    {
      a = pairs[i].a;
      b = pairs[i].b;
      expected = pairs[i].expected;
      ASSERTEQX(strlen(a), strlen(b), a);
      ASSERTEQX(memcasecmp32(a, b, strlen(a)), expected, a);
    }

    for(i = 0; i < arraysize(pairs64); i++)
    {
      a = pairs64[i].a.c_str();
      b = pairs64[i].b.c_str();
      expected = pairs64[i].expected;

      ASSERTEQ((size_t)a & 0x3, 0);
      ASSERTEQ((size_t)b & 0x3, 0);
      ASSERTEQX(strlen(a), strlen(b), a);
      ASSERTEQX(memcasecmp32(a, b, strlen(a)), expected, a);
    }
  }
}
