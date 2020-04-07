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

#include <array>
#include <tuple>


static const alignstr<uint32_t> a("abcde");

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
  static const std::tuple<const char *, const char *> pairs[] =
  {
    { "", "" },
    { " ", " " },
    { "abcde", "ABCDE" },
    { "testTESTtestTEST", "TESTtestTESTtest" },
    { "@!%$*^!@$&#!*([]", "@!%$*^!@$&#!*([]" },
    { u8"śśśśśśśś", u8"śśśśśśśś" },
  };

  static const std::tuple<alignstr<uint64_t>, alignstr<uint64_t>> pairs64[] =
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
      std::tie(a,b) = pairs[i];
      ASSERTEQX(strlen(a), strlen(b), a);
      ASSERTX(!memcasecmp(a, b, strlen(a)), a);
    }

    for(i = 0; i < arraysize(pairs64); i++)
    {
      a = std::get<0>(pairs64[i]).c_str();
      b = std::get<1>(pairs64[i]).c_str();

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
      std::tie(a,b) = pairs[i];
      ASSERTEQX(strlen(a), strlen(b), a);
      ASSERTX(!memcasecmp32(a, b, strlen(a)), a);
    }

    for(i = 0; i < arraysize(pairs64); i++)
    {
      a = std::get<0>(pairs64[i]).c_str();
      b = std::get<1>(pairs64[i]).c_str();

      ASSERTEQ((size_t)a & 0x3, 0);
      ASSERTEQ((size_t)b & 0x3, 0);
      ASSERTEQX(strlen(a), strlen(b), a);
      ASSERTX(!memcasecmp32(a, b, strlen(a)), a);
    }
  }
}

UNITTEST(NoMatch)
{
  static const std::tuple<const char *, const char *> pairs[] =
  {
    { "-", "_" },
    { "abcde", "ABCDF" },
    { "testTESTtestTEST", "TASTtastTASTtast" },
    { "@!%$*^!@$&#!*([a", "@!%$*^!@$&#!*([b" },
    { u8"śśŚśśśŚś", u8"ŚśśśŚśśś" },
  };

  static const std::tuple<alignstr<uint64_t>, alignstr<uint64_t>> pairs64[] =
  {
    { "-", "_" },
    { "testTESTtestTEST", "TASTtastTASTtast" },
    { "aaaaaaaa*aaaaaaaBAAAAAAAbaaaaaaa", "aaaaaaaaBAAAAAAAbaaaaaaabaaaaaaa" },
    { "aaaaaaaab*aaaaaaBAAAAAAAbaaaaaaa", "aaaaaaaaBAAAAAAAbaaaaaaabaaaaaaa" },
    { "aaaaaaaaba*aaaaaBAAAAAAAbaaaaaaa", "aaaaaaaaBAAAAAAAbaaaaaaabaaaaaaa" },
    { "aaaaaaaabaa*aaaaBAAAAAAAbaaaaaaa", "aaaaaaaaBAAAAAAAbaaaaaaabaaaaaaa" },
    { "aaaaaaaabaaa*aaaBAAAAAAAbaaaaaaa", "aaaaaaaaBAAAAAAAbaaaaaaabaaaaaaa" },
    { "aaaaaaaabaaaa*aaBAAAAAAAbaaaaaaa", "aaaaaaaaBAAAAAAAbaaaaaaabaaaaaaa" },
    { "aaaaaaaabaaaaa*aBAAAAAAAbaaaaaaa", "aaaaaaaaBAAAAAAAbaaaaaaabaaaaaaa" },
    { "aaaaaaaabaaaaaa*BAAAAAAAbaaaaaaa", "aaaaaaaaBAAAAAAAbaaaaaaabaaaaaaa" },
    { "@!%$*^!@$&#!*([a", "@!%$*^!@$&#!*([b" },
    { u8"śśŚśśśŚś", u8"ŚśśśŚśśś" },
  };
  const char *a;
  const char *b;
  int i;

  SECTION(memcasecmp)
  {
    for(i = 0; i < arraysize(pairs); i++)
    {
      std::tie(a,b) = pairs[i];
      ASSERTEQX(strlen(a), strlen(b), a);
      ASSERTX(memcasecmp(a, b, strlen(a)), a);
    }

    for(i = 0; i < arraysize(pairs64); i++)
    {
      a = std::get<0>(pairs64[i]).c_str();
      b = std::get<1>(pairs64[i]).c_str();

      ASSERTEQ((size_t)a & 0x7, 0);
      ASSERTEQ((size_t)b & 0x7, 0);
      ASSERTEQX(strlen(a), strlen(b), a);
      ASSERTX(memcasecmp(a, b, strlen(a)), a);
    }
  }

  SECTION(memcasecmp32)
  {
    for(i = 0; i < arraysize(pairs); i++)
    {
      std::tie(a,b) = pairs[i];
      ASSERTEQX(strlen(a), strlen(b), a);
      ASSERTX(memcasecmp32(a, b, strlen(a)), a);
    }

    for(i = 0; i < arraysize(pairs64); i++)
    {
      a = std::get<0>(pairs64[i]).c_str();
      b = std::get<1>(pairs64[i]).c_str();

      ASSERTEQ((size_t)a & 0x3, 0);
      ASSERTEQ((size_t)b & 0x3, 0);
      ASSERTEQX(strlen(a), strlen(b), a);
      ASSERTX(memcasecmp32(a, b, strlen(a)), a);
    }
  }
}
