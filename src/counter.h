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

/* Declarations for COUNTER.CPP */

#ifndef __COUNTER_H
#define __COUNTER_H

#include <stdio.h>

const int STRING_INITIAL_ALLOCATE = 16;

typedef struct _counter counter;
typedef struct _mzx_string mzx_string;

#include "world.h"

typedef int (* gateway_write_function)(World *mzx_world,
 counter *counter, char *name, int value, int id);

struct _counter
{
  int value;
  gateway_write_function gateway_write;
  char name[1];
};

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

struct _mzx_string
{
  int length;
  int allocated_length;
  char *value;

  char name[1];
  char storage_space[1];
};

typedef struct _function_counter function_counter;

typedef int (* builtin_read_function)(World *mzx_world,
 function_counter *counter, char *name, int id);

typedef void (* builtin_write_function)(World *mzx_world,
 function_counter *counter, char *name, int value, int id);

struct _function_counter
{
  char name[20];
  int value;
  builtin_read_function function_read;
  builtin_write_function function_write;
};

int match_function_counter(char *dest, char *src);

void set_gateway(World *mzx_world, char *name,
 gateway_write_function *function);
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
void get_string_size_offset(char *name, int *ssize, int *soffset);

void counter_fsg();

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

#endif

