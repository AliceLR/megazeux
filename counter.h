/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Declarations for COUNTER.CPP */

#ifndef __COUNTER_H
#define __COUNTER_H

#include <stdio.h>

typedef struct _Counter Counter;
typedef struct _String String;

#include "world.h"

typedef int (* gateway_write_function)(World *mzx_world,
 Counter *counter, char *name, int value, int id);

struct _Counter
{
  char counter_name[15];
  int counter_value;
  gateway_write_function counter_gateway_write;
};

// TODO - Give strings a dynamic length. It would expand
// set ends up being larger than the current size.

struct _String
{
  char string_name[15];
  char string_contents[64];
};

typedef struct _Function_counter Function_counter;

typedef int (* builtin_read_function)(World *mzx_world,
 Function_counter *counter, char *name, int id);

typedef void (* builtin_write_function)(World *mzx_world,
 Function_counter *counter, char *name, int value, int id);

struct _Function_counter
{
  char counter_name[20];
  int counter_value;
  builtin_read_function counter_function_read;
  builtin_write_function counter_function_write;
};

void set_gateway(World *mzx_world, char *name,
 gateway_write_function *function);
void initialize_gateway_functions(World *mzx_world);
// Return a pointer to the list position of the named counter
Counter *find_counter(World *mzx_world, char *name, int *next);
// Return a pointer to the named function counter
Function_counter *find_function_counter(char *name);
// Return a pointer to the list position of the named string
String *find_string(World *mzx_world, char *name, int *next);
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
void add_string(World *mzx_world, char *name, char *value, int position);
char *set_function_string(World *mzx_world, char *name, int id, char *buffer);
char *get_string(World *mzx_world, char *name, int id, char *buffer);
void set_string(World *mzx_world, char *name, char *value, int id);
// Concatenates a string with another
void inc_string(World *mzx_world, char *name, char *value, int id);
// Decreases the end of a string by N characters
void dec_string_int(World *mzx_world, char *name, int value, int id);
void load_string_board(World *mzx_world, char *expression,
 int w, int h, char l, char *src, int width, int id);
int set_counter_special(World *mzx_world, int spec_type,
 char *char_value, int value, int id);
int translate_coordinates(char *src, unsigned int *x, unsigned int *y);
int is_string(char *buffer);
void get_string_size_offset(char *name, int *ssize, int *soffset);

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

