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
 * Test the custom Robotic interpreter replacements for sprintf %d and %x.
 * These should output exactly the same thing as sprintf %d and %x.
 */

#include "Unit.hpp"
#include "../src/expr.h"

#include <limits.h>

struct tr_int_data
{
  int value;
  char dec[12];
  char hex[9];
};

static const tr_int_data data[] =
{
  { 0,            "0",            "0" },
  { 1,            "1",            "1" },
  { 10,           "10",           "a" },
  { 16,           "16",           "10" },
  { 100,          "100",          "64" },
  { 1024,         "1024",         "400" },
  { 32768,        "32768",        "8000" },
  { 65535,        "65535",        "ffff" },
  { 54321,        "54321",        "d431" },
  { 123454321,    "123454321",    "75bc371" },
  { 2147483647,   "2147483647",   "7fffffff" },
  { -2147483648,  "-2147483648",  "80000000" },
  { -1,           "-1",           "ffffffff" },
  { -12345,       "-12345",       "ffffcfc7" },
  { -123454321,   "-123454321",   "f8a43c8f" },
  { -1000000000,  "-1000000000",  "c4653600" },
};

UNITTEST(tr_int_to_string)
{
  char tr_buf[12];
  char *tmp;
  size_t len;

  for(const tr_int_data &d : data)
  {
    tmp = tr_int_to_string(tr_buf, d.value, &len);

    ASSERTEQ(len, strlen(d.dec), "%s", d.dec);
    ASSERTCMP(tmp, d.dec, "");
  }
}

UNITTEST(tr_int_to_hex_string)
{
  char tr_buf[9];
  char *tmp;
  size_t len;

  for(const tr_int_data &d : data)
  {
    tmp = tr_int_to_hex_string(tr_buf, d.value, &len);

    ASSERTEQ(len, strlen(d.hex), "%s", d.hex);
    ASSERTCMP(tmp, d.hex, "");
  }
}
