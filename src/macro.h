/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef __MACRO_H
#define __MACRO_H

#include "compat.h"

__M_BEGIN_DECLS

typedef enum
{
  number,
  string,
  character,
  color
} variable_type;

typedef enum
{
  decimal,
  hexidecimal
} reference_mode;

typedef union
{
  int int_storage;
  char *str_storage;
} variable_storage;

typedef struct
{
  variable_storage storage;
  variable_storage def;
  char *name;
} macro_variable;

typedef struct
{
  int type_attributes[16];
  variable_type type;
  int num_variables;
  macro_variable *variables;
  macro_variable **variables_sorted;
} macro_type;

typedef struct
{
  macro_type *type;
  variable_storage *storage;
  reference_mode ref_mode;
} macro_variable_reference;

typedef struct
{
  char *name;
  char *label;
  int num_lines;
  char ***lines;
  macro_variable_reference **variable_references;
  int *line_element_count;
  int num_types;
  int total_variables;
  macro_type types[32];
  char *text;
} ext_macro;

void add_ext_macro(config_info *conf, char *name, char *line_data,
 char *label);

#ifdef CONFIG_EDITOR
ext_macro *find_macro(config_info *conf, char *name, int *next);
variable_storage *find_macro_variable(char *name, macro_type *m);
char *skip_whitespace(char *src);
char *skip_to_next(char *src, char t, char a, char b);
#endif // CONFIG_EDITOR

__M_END_DECLS

#endif // __MACRO_H
