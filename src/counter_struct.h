/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1999 Charles Goetzman
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

#ifndef __COUNTER_STRUCT_H
#define __COUNTER_STRUCT_H

#include "compat.h"

__M_BEGIN_DECLS

#include <inttypes.h>

struct counter
{
  int32_t value;
#ifdef CONFIG_COUNTER_HASH_TABLES
  uint32_t hash;
#endif
  uint16_t name_length;
  uint8_t gateway_write;
  uint8_t unused;

  /**
   * This struct will be allocated with extra space to contain the entire
   * counter name, null-terminated. This field MUST be at least 4-aligned and
   * it must be the last field (any extra padding after it will be used).
   */
  char name[1];
};

struct counter_list
{
  unsigned int num_counters;
  unsigned int num_counters_allocated;
  struct counter **counters;
#ifdef CONFIG_COUNTER_HASH_TABLES
  void *hash_table;
#endif
};

// Name and storage share space. It's messy, but fast and
// space efficient.

// The storage space doesn't actually have to have anything,
// however (in fact, neither does name). Strings can
// be used as pointers to hold intermediate or immediate
// values. For instance, string literals and spliced
// strings. Anyone who uses strings in this way has a few
// responsibilities, however:
// The string must be properly initialized so length and
// value are set (allocated_length need not be set)
// The actual string list must not contain any of these
// kinds of temporary strings.
// Any deallocation (of value?) must be done by the
// initializer.
// get_string returns temporary strings like this (for
// speed and memory efficiency reasons), and the string
// mutating functions work on strings of this format as
// the second operand.

// In a normal (persistent string, initialized by
// add_string) string the struct will be over-initialized
// to contain room for both the name and the value; value
// will then point into the storage space somewhere. The
// name comes first and is a fixed size. The value comes
// after it and is of a dynamic length (allocated_length).
// Allocated_length will typically end up larger than
// length if the string's size is changed.

struct string
{
  char *value;
  uint32_t length;
  uint32_t allocated_length;

  // Back reference to the string's position in the main list, mandatory as
  // a hash table search needs to be able to determine this value.
  uint32_t list_ind;

#ifdef CONFIG_COUNTER_HASH_TABLES
  uint32_t hash;
#endif

  uint16_t name_length;
  uint8_t unused;
  uint8_t unused2;

  /**
   * This struct will be allocated with extra space to contain the string name
   * and string value. This field MUST be at least 4-aligned and must be the
   * last field in the struct (any extra padding after it will be used).
   */
  char name[1];
};

struct string_list
{
  unsigned int num_strings;
  unsigned int num_strings_allocated;
  struct string **strings;
#ifdef CONFIG_COUNTER_HASH_TABLES
  void *hash_table;
#endif
};

// Special counter returns for opening files

enum special_counter_return
{
  FOPEN_NONE = 0,
  FOPEN_FREAD,
  FOPEN_FWRITE,
  FOPEN_FAPPEND,
  FOPEN_FMODIFY,
  FOPEN_SMZX_PALETTE,
  FOPEN_SMZX_INDICES,
  FOPEN_LOAD_GAME,
  FOPEN_SAVE_GAME,
  FOPEN_LOAD_COUNTERS,
  FOPEN_SAVE_COUNTERS,
  FOPEN_LOAD_ROBOT,
  FOPEN_LOAD_BC,
  FOPEN_SAVE_ROBOT,
  FOPEN_SAVE_BC
};

__M_END_DECLS

#endif // __COUNTER_STRUCT_H
