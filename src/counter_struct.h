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

typedef struct _counter
{
  int value;
  void *gateway_write;
  void *gateway_dec;
  char name[1];
} counter;

// TODO - Give strings a dynamic length. It would expand
// set ends up being larger than the current size.

// "storage_space" is just there for show.. the truth is,
// name and storage share space. It's messy, but fast and
// space efficient.

// storage_space doesn't actually have to have anything,
// however (in fact, neither does name). mzx_strings can
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

typedef struct
{
  unsigned int length;
  unsigned int allocated_length;
  char *value;

  char name[1];
  char storage_space[1];
} mzx_string;

__M_END_DECLS

#endif // __COUNTER_STRUCT_H
