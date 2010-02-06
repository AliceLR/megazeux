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

static const int STRING_INITIAL_ALLOCATE = 16;

#include "counter_struct.h"
#include "world_struct.h"

typedef struct _function_counter
{
  char name[20];
  int minimum_version;
  int (*function_read)(World *mzx_world, struct _function_counter *counter,
   char *name, int id);
  void (*function_write)(World *mzx_world, struct _function_counter *counter,
   char *name, int value, int id);
} function_counter;

typedef int (*gateway_write_function)(World *mzx_world,
 counter *counter, char *name, int value, int id);

// functions

int match_function_counter(char *dest, char *src);
void set_gateway(World *mzx_world, char *name,
 gateway_write_function function);
void initialize_gateway_functions(World *mzx_world);
// Return a pointer to the list position of the named counter
counter *find_counter(World *mzx_world, char *name, int *next);
// Return a pointer to the named function counter
function_counter *find_function_counter(char *name);
// Return a pointer to the list position of the named string
mzx_string *find_string(World *mzx_world, char *name, int *next);
// Get the contents of a counter. Include robot id (0 if unimportant)
int get_counter(World *mzx_world, char *name, int id);
// Sets the value of a counter. Include robot id if a robot is running.
void set_counter(World *mzx_world, char *name, int value, int id);
// Decrease or increase a counter.
void inc_counter(World *mzx_world, char *name, int value, int id);
void dec_counter(World *mzx_world, char *name, int value, int id);
void mul_counter(World *mzx_world, char *name, int value, int id);
void div_counter(World *mzx_world, char *name, int value, int id);
void mod_counter(World *mzx_world, char *name, int value, int id);
void add_counter(World *mzx_world, char *name, int value, int position);
mzx_string *add_string_preallocate(World *mzx_world, char *name,
 int length, int position);
void add_string(World *mzx_world, char *name, mzx_string *src, int position);
mzx_string *reallocate_string(World *mzx_world, mzx_string *src, int pos,
 int length);
char *set_function_string(World *mzx_world, char *name, int id, char *buffer);
int get_string(World *mzx_world, char *name, mzx_string *dest, int id);
void set_string(World *mzx_world, char *name, mzx_string *src, int id);
// Concatenates a string with another
void inc_string(World *mzx_world, char *name, mzx_string *src, int id);
// Decreases the end of a string by N characters
void dec_string_int(World *mzx_world, char *name, int value, int id);
int compare_strings(mzx_string *dest, mzx_string *src);
int load_string_board_direct(World *mzx_world, mzx_string *str,
 int next, int w, int h, char l, char *src, int width);
void load_string_board(World *mzx_world, char *expression,
 int w, int h, char l, char *src, int width);
int set_counter_special(World *mzx_world, int spec_type,
 char *char_value, int value, int id);
int translate_coordinates(char *src, unsigned int *x, unsigned int *y);
int is_string(char *buffer);
void get_string_size_offset(char *name, unsigned int *ssize,
 unsigned int *soffset);

void counter_fsg();

void debug_counters(World *mzx_world);

counter *load_counter(FILE *fp);
mzx_string *load_string(FILE *fp);
void save_counter(FILE *fp, counter *src_counter);
void save_string(FILE *fp, mzx_string *src_string);

// Even old games tended to use at least this many.
#define MIN_COUNTER_ALLOCATE 32

// These take up more room...
#define MIN_STRING_ALLOCATE 4

// Special counter returns for opening files

#define NONE               0
#define FOPEN_FREAD        1
#define FOPEN_FWRITE       2
#define FOPEN_FAPPEND      3
#define FOPEN_FMODIFY      4
#define FOPEN_SMZX_PALETTE 5
#define FOPEN_LOAD_GAME    6
#define FOPEN_SAVE_GAME    7
#define FOPEN_SAVE_WORLD   8
#define FOPEN_LOAD_ROBOT   9
#define FOPEN_SAVE_ROBOT   10
#define FOPEN_LOAD_BC      11
#define FOPEN_SAVE_BC      12

__M_END_DECLS

#endif // __COUNTER_H
