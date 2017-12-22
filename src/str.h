/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1999 Charles Goetzman
 * Copyright (C) 2017 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __STR_H
#define __STR_H

#include "compat.h"

__M_BEGIN_DECLS

#include <stdio.h>

#include "world_struct.h"
#include "counter_struct.h"

// These take up more room...
#define MIN_STRING_ALLOCATE 4

// Strings cannot be longer than 4M (orig 1M)
#define MAX_STRING_LEN (1 << 22)

CORE_LIBSPEC int get_string(struct world *mzx_world, const char *name,
 struct string *dest, int id);
CORE_LIBSPEC int set_string(struct world *mzx_world, const char *name,
 struct string *src, int id);
CORE_LIBSPEC struct string *new_string(struct world *mzx_world,
 const char *name, size_t length, int id);
CORE_LIBSPEC bool is_string(char *buffer);
CORE_LIBSPEC void sort_string_list(struct string **string_list,
 int num_strings);

int string_read_as_counter(struct world *mzx_world,
 const char *name, int id);
void string_write_as_counter(struct world *mzx_world,
 const char *name, int value, int id);

void load_string_board(struct world *mzx_world, const char *name,
 char *src_chars, int src_width, int block_width, int block_height,
 char terminator);

void inc_string(struct world *mzx_world, const char *name, struct string *src,
 int id);
void dec_string_int(struct world *mzx_world, const char *name, int value,
 int id);
int compare_strings(struct string *dest, struct string *src,
 int exact_case, int allow_wildcards);

struct string *load_new_string(struct string **string_list, int index,
 const char *name, int name_length, int str_length);

void free_string_list(struct string **string_list, int num_strings);

__M_END_DECLS

#endif // __STR_H
