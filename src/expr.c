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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "expr.h"

#include "world_struct.h"
#include "counter.h"
#include "robot.h"

static int last_val;

static void expr_skip_whitespace(char **expression)
{
  while(isspace(**expression))
  {
    (*expression)++;
  }
}

static int parse_argument(World *mzx_world, char **argument, int *type, int id)
{
  int first_char = **argument;

  // Test the first one.

  switch(first_char)
  {
    // Addition operator
    case '+':
    {
      *type = 1;
      (*argument)++;
      return (int)op_addition;
    }
    // Subtraction operator
    case '-':
    {
      (*argument)++;
      if(!isspace(**argument))
      {
        int t2, val = parse_argument(mzx_world, argument, &t2, id);
        if((t2 == 0) || (t2 == 2))
        {
          val = -val;
          *type = 2;
          return val;
        }
        else
        {
          *type = -1;
          return -1;
        }
      }
      *type = 1;
      return (int)op_subtraction;
    }
    // Multiplication operator
    case '*':
    {
      *type = 1;
      (*argument)++;
      return (int)op_multiplication;
    }
    // Division operator
    case '/':
    {
      *type = 1;
      (*argument)++;
      return (int)op_division;
    }
    // Modulus operator
    case '%':
    {
      *type = 1;
      (*argument)++;
      return (int)op_modulus;
    }

    // Exponent operator
    case '^':
    {
      *type = 1;
      (*argument)++;
      return (int)op_exponentation;
    }

    // Bitwise AND operator
    case 'a':
    {
      *type = 1;
      (*argument)++;
      return (int)op_and;
    }

    // Bitwise OR operator
    case 'o':
    {
      *type = 1;
      (*argument)++;
      return (int)op_or;
    }

    // Bitwise XOR operator
    case 'x':
    {
      *type = 1;
      (*argument)++;
      return (int)op_xor;
    }

    // Less than/bitshift left
    case '<':
    {
      *type = 1;
      (*argument)++;
      if(**argument == '<')
      {
        (*argument)++;
        return (int)op_bitshift_left;
      }
      if(**argument == '=')
      {
        (*argument)++;
        return (int)op_less_than_or_equal;
      }
      return (int)op_less_than;
    }
    // Greater than/bitshift right
    case '>':
    {
      *type = 1;
      (*argument)++;
      if(**argument == '>')
      {
        (*argument)++;
        return (int)op_bitshift_right;
      }
      if(**argument == '=')
      {
        (*argument)++;
        return (int)op_greater_than_or_equal;
      }
      return (int)op_greater_than;
    }
    // Equality
    case '=':
    {
      *type = 1;
      (*argument)++;
      return (int)op_equal;
    }
    // Logical negation
    case '!':
    {
      *type = 1;
      if(*(*argument + 1) == '=')
      {
        (*argument) += 2;
        return (int)op_not_equal;
      }
    }

    // One's complement
    case '~':
    {
      (*argument)++;
      if(!isspace(**argument))
      {
        int t2, val = parse_argument(mzx_world, argument, &t2, id);
        if((t2 == 0) || (t2 == 2))
        {
          val = ~val;
          *type = 2;
          return val;
        }
        else
        {
          *type = -1;
          return -1;
        }
      }
    }

    // # is the last expression value, 32bit.
    case '#':
    {
      *type = 0;
      (*argument)++;
      return last_val;
    }

    // The evil null terminator...
    case '\0':
    {
      *type = -1;
      return -1;
    }

    // Parentheses! Recursive goodness...
    // Treat a valid return as an argument.
    case '(':
    {
      int error;
      int val;
      char *a_ptr = (*argument) + 1;
      val = parse_expression(mzx_world, &a_ptr, &error, id);
      if(error)
      {
        *type = -1;
        return -1;
      }
      else
      {
        *argument = a_ptr;
        *type = 0;
        return val;
      }
    }

    // Begins with a ' or a & makes it a message ('counter')
    case '&':
    case '\'':
    {
      // Find where the next ' or & is
      char t_char = first_char;
      char temp[256];
      char temp2[256];
      int count = 0;

      (*argument)++;

      // Remember, null terminator is evil; if it's hit exit completely.
      while(((**argument) != t_char) && (count < 256))
      {
        // If a nested expression is hit closing 's should be ignored
        if((**argument) == '(')
        {
          int close_paren_count = 1;
          // The number of )'s to expect.. finding one decreases it..
          // And finding a ( increases it.

          while((close_paren_count) && (count < 256))
          {
            temp[count] = **argument;
            (*argument)++;
            count++;

            switch(**argument)
            {
              case '\0':
              {
                *type = -1;
                return -1;
              }
              case ')':
              {
                close_paren_count--;
                break;
              }
              case '(':
              {
                close_paren_count++;
                break;
              }
            }
          }
        }
        else
        {
          if(**argument == '\0')
          {
            *type = -1;
            return -1;
          }

          temp[count] = **argument;
          (*argument)++;
          count++;
        }
      }
      *type = 0;
      (*argument)++;
      temp[count] = '\0';
      tr_msg(mzx_world, temp, id, temp2);
      return get_counter(mzx_world, temp2, id);
    }

    // Otherwise, it must be an integer, or just something invalid.
    default:
    {
      if((first_char >= '0') && (first_char <= '9'))
      {
        // It's good.
        char *end_p;
        int val = (int)strtol(*argument, &end_p, 0);
        *argument = end_p;
        *type = 0;
        return val;
      }
      else
      {
        // It's not so good..
        *type = -1;
        return -1;
      }
    }
  }
}

static int evaluate_operation(int operand_a, op c_operator, int operand_b)
{
  switch(c_operator)
  {
    case op_addition:
    {
      return operand_a + operand_b;
    }
    case op_subtraction:
    {
      return operand_a - operand_b;
    }
    case op_multiplication:
    {
      return operand_a * operand_b;
    }
    case op_division:
    {
      if(operand_b == 0)
      {
        return 0;
      }
      return operand_a / operand_b;
    }
    case op_modulus:
    {
      int val;

      if(operand_b == 0)
        return 0;

      val = operand_a % operand_b;

      // Converted C99 regulated truncated modulus to
      // the more useful (for us) floored modulus
      // Source:
      // Division and Modulus for Computer Scientists
      // DAAN LEIJEN
      // University of Utrecht

      if((val < 0) ^ (operand_b < 0))
        return val + operand_b;

      return val;
    }
    case op_exponentation:
    {
      int i;
      int val = 1;
      if(operand_a == 0)
      {
        return 0;
      }
      if(operand_a == 1)
      {
        return 1;
      }
      if(operand_b < 0)
      {
        if(operand_a == -1)
        {
          operand_b *= -1;
        }
        else
        {
          return 0;
        }
      }

      for(i = 0; i < operand_b; i++)
      {
        val *= operand_a;
      }
      return val;
    }
    case op_and:
    {
      return operand_a & operand_b;
    }
    case op_or:
    {
      return operand_a | operand_b;
    }
    case op_xor:
    {
      return (operand_a ^ operand_b);
    }
    case op_bitshift_left:
    {
      return operand_a << operand_b;
    }
    case op_bitshift_right:
    {
      return (unsigned int)(operand_a) >> operand_b;
    }
    case op_equal:
    {
      return operand_a == operand_b;
    }
    case op_less_than:
    {
      return operand_a < operand_b;
    }
    case op_less_than_or_equal:
    {
      return operand_a <= operand_b;
    }
    case op_greater_than:
    {
      return operand_a > operand_b;
    }
    case op_greater_than_or_equal:
    {
      return operand_a >= operand_b;
    }
    case op_not_equal:
    {
      return operand_a != operand_b;
    }
    default:
    {
      return operand_a;
    }
  }
}

int parse_expression(World *mzx_world, char **expression,
 int *error, int id)
{
  int operand_val;
  int current_arg;
  int c_operator;
  int value;

  *error = 0;

  // Skip initial whitespace..
  expr_skip_whitespace(expression);
  value = parse_argument(mzx_world, expression, &current_arg, id);
  if((current_arg != 0) && (current_arg != 2))
  {
    // First argument must be a value type.
    *error = 1;
    return -99;
  }
  expr_skip_whitespace(expression);

  while(1)
  {
    if(**expression == ')')
    {
      (*expression)++;
      break;
    }

    c_operator = parse_argument(mzx_world, expression, &current_arg, id);
    // Next arg must be an operator, unless it's a negative number,
    // in which case it's considered + num
    if(current_arg == 2)
    {
      value = evaluate_operation(value, op_addition, c_operator);
    }
    else
    {
      if(current_arg != 1)
      {
        *error = 2;
        return -100;
      }
      expr_skip_whitespace(expression);
      operand_val = parse_argument(mzx_world, expression, &current_arg, id);
      // And now it must be an integer.
      if((current_arg != 0) && (current_arg != 2))
      {
        *error = 3;
        return -102;
      }
      // Evaluate it.
      value = evaluate_operation(value, (op)c_operator, operand_val);
    }
    expr_skip_whitespace(expression);
  }

  last_val = value;
  return value;
}
