/* MegaZeux
 *
 * Copyright (C) 2002 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef EXPR_H
#define EXPR_H

#include "world.h"

typedef enum
{
  op_addition,
  op_subtraction,
  op_multiplication,
  op_division,
  op_modulus,
  op_exponentation,
  op_and,
  op_or,
  op_xor,
  op_bitshift_left,
  op_bitshift_right,
  op_equal,
  op_less_than,
  op_greater_than,
  op_greater_than_or_equal,
  op_less_than_or_equal,
  op_not_equal
} op;

int parse_expression(World *mzx_world, char **expression,
 int *error, int id);
int parse_argument(World *mzx_world, char **argument, int &type,
 int id);
void skip_whitespace(char **expression);
int evaluate_operation(int operand_a, op c_operator, int operand_b);

#endif

