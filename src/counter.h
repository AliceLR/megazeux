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

#ifndef __COUNTER_H
#define __COUNTER_H

#include "compat.h"

__M_BEGIN_DECLS

#include <stdio.h>

#include "world_struct.h"
#include "counter_struct.h"

CORE_LIBSPEC int match_function_counter(const char *dest, const char *src);
CORE_LIBSPEC int get_counter(struct world *mzx_world, const char *name, int id);
CORE_LIBSPEC void set_counter(struct world *mzx_world, const char *name,
 int value, int id);
CORE_LIBSPEC int get_string(struct world *mzx_world, const char *name,
 struct string *dest, int id);
CORE_LIBSPEC void set_string(struct world *mzx_world, const char *name,
 struct string *src, int id);
CORE_LIBSPEC void counter_fsg(void);

void initialize_gateway_functions(struct world *mzx_world);
void inc_counter(struct world *mzx_world, const char *name, int value, int id);
void dec_counter(struct world *mzx_world, const char *name, int value, int id);
void mul_counter(struct world *mzx_world, const char *name, int value, int id);
void div_counter(struct world *mzx_world, const char *name, int value, int id);
void mod_counter(struct world *mzx_world, const char *name, int value, int id);

void inc_string(struct world *mzx_world, const char *name, struct string *src,
 int id);
void dec_string_int(struct world *mzx_world, const char *name, int value,
 int id);
int compare_strings(struct string *dest, struct string *src);

void load_string_board(struct world *mzx_world, const char *expression,
 int w, int h, char l, char *src, int width);
int set_counter_special(struct world *mzx_world, char *char_value,
 int value, int id);
bool is_string(char *buffer);

struct counter *load_counter(struct world *mzx_world, FILE *fp);
struct string *load_string(FILE *fp);
void save_counter(FILE *fp, struct counter *src_counter);
void save_string(FILE *fp, struct string *src_string);
void free_counter_list(struct counter **counter_list, int num_counters);
void free_string_list(struct string **string_list, int num_strings);

// Even old games tended to use at least this many.
#define MIN_COUNTER_ALLOCATE 32

// These take up more room...
#define MIN_STRING_ALLOCATE 4

// Strings cannot be longer than 4M (orig 1M)
#define MAX_STRING_LEN (1 << 22)

// Maximum space board can consume
#define MAX_BOARD_SIZE 16 * 1024 * 1024

__M_END_DECLS

#endif // __COUNTER_H
