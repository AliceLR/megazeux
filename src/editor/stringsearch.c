/* MegaZeux
 *
 * Copyright (C) 2012 Alice Rowan <petrifiedrowan@gmail.com>
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
 * Boyer-Moore string search implementation.
 * These functions are currently only used by the editor.
 */

#include "../memcasecmp.h"
#include "../util.h"

#include "stringsearch.h"

/**
 * Initialize a string search index for a needle substring to be used in
 * string_search(). This only needs to be performed once per search string.
 * This may be called subsequently on the same search_string_data struct to
 * prepare it for a different needle string. The needle string does not need
 * to be null-terminated.
 *
 * @param B             Pointer to needle substring.
 * @param b_len         Length of needle substring.
 * @param data          Struct to store string search index info to.
 * @param ignore_case   True for case-insensitive; false for case-sensitive.
 */
void string_search_index(const void *B, const size_t b_len,
 struct string_search_data *data, boolean ignore_case)
{
  unsigned int *index = data->index;
  const unsigned char *start = (const unsigned char *)B;
  const unsigned char *end = start + b_len - 1;
  const unsigned char *pos;
  int i;

  for(i = 0; i < 256; i++)
    index[i] = b_len;

  if(!ignore_case)
  {
    for(pos = start; pos < end; pos++)
      index[*pos] = end - pos;
  }
  else
  {
    for(pos = start; pos < end; pos++)
      index[memtolower(*pos)] = end - pos;

    // Duplicating the lowercase values over the uppercase values helps avoid
    // an extra tolower in the search function.
    memcpy(index + 'A', index + 'a', sizeof(unsigned int) * 26);
  }
}

/**
 * Search for needle substring B in haystack A. This implementation uses a
 * simplified Boyer-Moore algorithm to improve search performance, particularly
 * for repeated searches. The provided string_search_data struct must be
 * initialized by calling string_search_index() before it is used. Neither the
 * needle string nor the haystack string need to be null-terminated.
 *
 * If a string search will only be performed once for a given needle or if the
 * needle is short, and if both strings are null-terminated, the standard
 * library function strstr should probably be used instead.
 *
 * @param A             Pointer to haystack string.
 * @param a_len         Length of haystack string.
 * @param B             Pointer to needle substring.
 * @param b_len         Length of needle substring.
 * @param data          Struct containing string search index information.
 * @param ignore_case   True for case-insensitive; false for case-sensitive.
 */
const void *string_search(const void *A, const size_t a_len,
 const void *B, const size_t b_len, const struct string_search_data *data,
 boolean ignore_case)
{
  const unsigned int *index = data->index;
  const unsigned char *a = (const unsigned char *)A;
  const unsigned char *b = (const unsigned char *)B;
  size_t i = b_len - 1;
  size_t idx;
  ssize_t j;

  if(!ignore_case)
  {
    while(i < a_len)
    {
      j = b_len - 1;

      while(j >= 0 && a[i] == b[j])
        j--, i--;

      if(j == -1)
        return (const void *)(a + i + 1);

      idx = index[a[i]];
      i += MAX(b_len - j, idx);
    }
  }
  else
  {
    while(i < a_len)
    {
      j = b_len - 1;

      while(j >= 0 && memtolower(a[i]) == memtolower(b[j]))
        j--, i--;

      if(j == -1)
        return (const void *)(a + i + 1);

      idx = index[a[i]];
      i += MAX(b_len - j, idx);
    }
  }
  return NULL;
}
