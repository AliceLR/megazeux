/* MegaZeux
 *
 * Copyright (C) 2019 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "../Unit.hpp"

#include <string.h>
#include <tuple>

#include "../../src/editor/stringsearch.c"

struct string_pair
{
  const char *needle;
  const char *haystack;
};

struct string_pair_idx
{
  const char *needle;
  const char *haystack;
  ssize_t where;
};

UNITTEST(Search)
{
  // left: needle, right: haystack
  static const string_pair haystack_none[] =
  {
    { "", "literally anything" },
    { "literally anything", "" },
    { "abcd", "abdacbdacbdabdcabd" },
    {
      "abce",
      "lomgstringlomgstringlomgstringlomgstringlomgstring"
      "lomgstringlomgstringlomgstringlomgstringlomgstring"
    },
    { "abcf", "s" },
    { "some needle", "some needl" },
    { "dome needle", "aome needle" },
    { "case", "CASE" },
    { "abcde", "cbdebcedbcdebcdbecdbedcbebcdebcdebcedbcbdcbedcbedbcdebcedbc" },
    { "abcde", "cbdebcedbcdebcdbecAbcdEbebcdebABCDEedbcbdcbedcbedbcdebAbcde" },
    {
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "baaaaaaaaaaaaaaaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAAAAA"
    },
    { "abcdabc", "abceabceabceabceabc" },
  };
  static const string_pair_idx haystack_once[] =
  {
    { "abcde", "abcde", 0 },
    { "abcd", "aaaaaaaaaabcaaaabcdaaaaa", 15 },
    {
      "------------separator",
      "fhs-----------fhsdklfhsdlseparator"
      "aflskdfhsdjklsfhdj------------separator", 52
    },
    { "case", "CASEcaseCASE", 4 },
    {
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      141
    },
    { "abcdabc", "abceabcdabceabceabc", 4 },
    { "pfatt",
      "set \"pf\" to "
      "\"('pldir'*6+('pfatt'>0*3)+('pfwalk'=1 ? 1 : 'pfwalk'=3 ? 2 : 0))\"", 26
    },
    {
      "textdone", "set \"macroar_!textdone\" to 0", 14
    },
    {
      "!text", "set \"$macrof_!text:Info\" to \"$\"", 13
    },
    {
      "scripts.txt", "set \"scripts.txt\" to \"FWRITE_OPEN\"", 5
    },
    {
      "$local", "set \"$@\" to \"R&&p_id&&.&$local_('local12')&\"", 24
    },
  };
  static const string_pair haystack_twice[] =
  {
    { "abcde", "abcdeabcde" },
    { "abcd", "aaaaaaaaaabcaaaabcdaaaaabcdaaaaaaaaa" },
    { "abcd", "aaaaaaaaaabcaaaabcdaaaaabcd" },
    { "abcd", "abcdabcabcabcabcabcabcbcdbcdbcdbcdbcdbcdbcdbcdbcdbcdbcdabcd" },
    {
      "------------separator",
      "fhsfhsdklfhsdl------------separator"
      "aflskdfhsdjklsfhdj------------separator--"
    },
    { "case", "caseCASEcase" },
    { "abba", "abbabba" },
    {
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    },
    { "abcdabc", "abceabcdabcdabceabc" },
  };
  struct string_search_data data;
  const char *A;
  const char *B;
  const void *dest;

  SECTION(OneOff)
  {
    for(int i = 0; i < arraysize(haystack_none); i++)
    {
      A = haystack_none[i].haystack;
      B = haystack_none[i].needle;

      ASSERTX(!string_search(A, strlen(A), B, strlen(B), nullptr, false), A);
    }

    for(int i = 0; i < arraysize(haystack_once); i++)
    {
      A = haystack_once[i].haystack;
      B = haystack_once[i].needle;
      ssize_t where = haystack_once[i].where;

      dest = string_search(A, strlen(A), B, strlen(B), nullptr, false);
      ASSERTX(dest, A);
      ASSERT(dest >= A && dest < A + strlen(A));
      ASSERTEQX((const char *)dest - A, where, A);
    }

    for(int i = 0; i < arraysize(haystack_twice); i++)
    {
      A = haystack_twice[i].haystack;
      B = haystack_twice[i].needle;

      dest = string_search(A, strlen(A), B, strlen(B), nullptr, false);
      ASSERTX(dest, A);
      ASSERT(dest >= A && dest < A + strlen(A));
    }
  }

  SECTION(Repeating)
  {
    for(int i = 0; i < arraysize(haystack_once); i++)
    {
      const char *dest;
      A = haystack_once[i].haystack;
      B = haystack_once[i].needle;
      ssize_t where = haystack_once[i].where;
      ssize_t a_len = strlen(A);
      ssize_t b_len = strlen(B);

      string_search_index(B, b_len, &data, false);

      dest = (const char *)string_search(A, a_len, B, b_len, &data, false);
      ASSERTX(dest, A);
      ASSERT(dest >= A && dest < A + a_len);
      ASSERTEQX((const char *)dest - A, where, A);
      dest++;
      a_len -= dest - A;
      A = dest;

      ASSERTX(!string_search(A, a_len, B, b_len, &data, false), A);
    }

    for(int i = 0; i < arraysize(haystack_twice); i++)
    {
      const char *dest;
      A = haystack_twice[i].haystack;
      B = haystack_twice[i].needle;
      ssize_t a_len = strlen(A);
      ssize_t b_len = strlen(B);

      string_search_index(B, b_len, &data, false);

      dest = (const char *)string_search(A, a_len, B, b_len, &data, false);
      ASSERTX(dest, A);
      ASSERT(dest >= A && dest < A + a_len);
      dest++;
      a_len -= dest - A;
      A = dest;

      dest = (const char *)string_search(A, a_len, B, b_len, &data, false);
      ASSERTX(dest, A);
      ASSERT(dest >= A && dest < A + a_len);
      dest++;
      a_len -= dest - A;
      A = dest;

      ASSERTX(!string_search(A, a_len, B, b_len, &data, false), A);
    }
  }
}

UNITTEST(SearchCaseInsensitive)
{
  // left: needle, right: haystack
  static const string_pair haystack_none[] =
  {
    { "", "literally anything" },
    { "literally anything", "" },
    { "abcd", "abdacbdacbdabdcabd" },
    {
      "abce",
      "lomgstringlomgstringlomgstringlomgstringlomgstring"
      "lomgstringlomgstringlomgstringlomgstringlomgstring"
    },
    { "abcf", "s" },
    { "some needle", "some needl" },
    { "dome needle", "aome needle" },
    { "abcde", "cbdebcedbcdebcdbecdbedcbebcdebcdebcedbcbdcbedcbedbcdebcedbc" },
    {
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAAAAAaaaaaaaa",
      "baaaaaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "baaaaaaaaaaaaaaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    },
    { "abcdabc", "abceABCeabceABCeabc" },
  };
  static const string_pair haystack_once[] =
  {
    { "abcde", "abcde" },
    { "abcd", "aaaaaaaaaabcaaaabcdaaaaa" },
    {
      "------------separator",
      "fhs-----------fhsdklfhsdlseparator"
      "aflskdfhsdjklsfhdj------------separator"
    },
    { "case", "CASE" },
    {
      "aAAAAAaaaaaaaaaaaaaaaaaaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAAAAAaaaaaaaaaaaaaaa"
      "baaaaaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "baaaaaaaaaaaaaaaaaaaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAAAAAaaaaaaa"
    },
    { "abcdabc", "abceaBcdAbCeabceabc" },
  };
  static const string_pair haystack_twice[] =
  {
    { "abcde", "abcdeabcde" },
    { "abcd", "aaaaaaaaaabcaaaabcdaaaaabcdaaaaaaaaa" },
    { "abcd", "aaaaaaaaaabcaaaabcdaaaaabcd" },
    { "abcd", "abcdabcabcabcabcabcabcbcdbcdbcdbcdbcdbcdbcdbcdbcdbcdbcdabcd" },
    {
      "------------separator",
      "fhsfhsdklfhsdl------------separator"
      "aflskdfhsdjklsfhdj------------separator--"
    },
    { "case", "caseCASE" },
    { "abba", "abbabba" },
    { "abcde", "cbdebcedbcdebcdbecAbcdEbebcdebABCDEedbcbdcbedcbedbcdebAbcdb" },
    {
      "aaaaaaaaaaaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAAAAAaa",
      "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaa"
      "baaaaaaaaaaaaaaaaaaaaaaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAAAAA"
      "baaaaaAAAAAaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaAAAAAaaaaaaaa"
    },
    { "abcdabc", "abceABCDabcDabceABC" },
  };
  struct string_search_data data;
  const char *A;
  const char *B;
  const void *dest;

  SECTION(OneOff)
  {
    for(int i = 0; i < arraysize(haystack_none); i++)
    {
      A = haystack_none[i].haystack;
      B = haystack_none[i].needle;

      ASSERTX(!string_search(A, strlen(A), B, strlen(B), NULL, true), A);
    }

    for(int i = 0; i < arraysize(haystack_once); i++)
    {
      A = haystack_once[i].haystack;
      B = haystack_once[i].needle;

      dest = string_search(A, strlen(A), B, strlen(B), NULL, true);
      ASSERTX(dest, A);
      ASSERT(dest >= A && dest < A + strlen(A));
    }

    for(int i = 0; i < arraysize(haystack_twice); i++)
    {
      A = haystack_twice[i].haystack;
      B = haystack_twice[i].needle;

      dest = string_search(A, strlen(A), B, strlen(B), NULL, true);
      ASSERTX(dest, A);
      ASSERT(dest >= A && dest < A + strlen(A));
    }
  }

  SECTION(Repeating)
  {
    for(int i = 0; i < arraysize(haystack_once); i++)
    {
      const char *dest;
      A = haystack_once[i].haystack;
      B = haystack_once[i].needle;
      ssize_t a_len = strlen(A);
      ssize_t b_len = strlen(B);

      string_search_index(B, b_len, &data, true);

      dest = (const char *)string_search(A, a_len, B, b_len, &data, true);
      ASSERTX(dest, A);
      ASSERT(dest >= A && dest < A + a_len);
      dest++;
      a_len -= dest - A;
      A = dest;

      ASSERTX(!string_search(A, a_len, B, b_len, &data, true), A);
    }

    for(int i = 0; i < arraysize(haystack_twice); i++)
    {
      const char *dest;
      A = haystack_twice[i].haystack;
      B = haystack_twice[i].needle;
      ssize_t a_len = strlen(A);
      ssize_t b_len = strlen(B);

      string_search_index(B, b_len, &data, true);

      dest = (const char *)string_search(A, a_len, B, b_len, &data, true);
      ASSERTX(dest, A);
      ASSERT(dest >= A && dest < A + a_len);
      dest++;
      a_len -= dest - A;
      A = dest;

      dest = (const char *)string_search(A, a_len, B, b_len, &data, true);
      ASSERTX(dest, A);
      ASSERT(dest >= A && dest < A + a_len);
      dest++;
      a_len -= dest - A;
      A = dest;

      ASSERTX(!string_search(A, a_len, B, b_len, &data, true), A);
    }
  }
}
