/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
 * Copyright (C) 2002 Gilead Kutnick - exophase@adelphia.net
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "world.h"
#include "expr.h"
#include "counter.h"
#include "robot.h"

int last_val;

int parse_expression(World *mzx_world, char **expression,
 int *error, int id)
{
  long int operand_val;
  int current_arg;
  long int c_operator;
  long int value;

  *error = 0;

  // Skip initial whitespace..
  skip_whitespace(expression);
  value = parse_argument(mzx_world, expression, current_arg, id);
  if((current_arg != 0) && (current_arg != 2))
  {
    // First argument must be a value type.
    *error = 1;
    return -99;
  }
  skip_whitespace(expression);

  while(1)
  {
    if(**expression == ')')
    {
      (*expression)++;
      break;
    }

    c_operator = parse_argument(mzx_world, expression, current_arg, id);
    // Next arg must be an operator, unless it's a negative number,
    // in which case it's considered + num
    if(current_arg == 2)
    {
      value = evaluate_operation(value, 1, c_operator);
    }
    else
    {
      if(current_arg != 1)
      {
        *error = 2;
        return -100;
      }
      skip_whitespace(expression);
      operand_val = parse_argument(mzx_world, expression, current_arg, id);
      // And now it must be an integer.
      if((current_arg != 0) && (current_arg != 2))
      {
        *error = 3;
        return -102;
      }
      // Evaluate it.
      value = evaluate_operation(value, c_operator, operand_val);
    }
    skip_whitespace(expression);
  }

  last_val = value;

  return value;
}

int parse_argument(World *mzx_world, char **argument, int &type, int id)
{
  int first_char = **argument;

  // Test the first one.

  switch(first_char)
  {
    // Addition operator
    case '+':
    {
      type = 1;
      (*argument)++;
      return 1;
    }
    // Subtraction operator
    case '-':
    {
      (*argument)++;
      if(!isspace(**argument))
      {
        int t2;
        long int val = parse_argument(mzx_world, argument, t2, id);
        if((t2 == 0) || (t2 == 2))
        {
          val = -val;
          type = 2;
          return val;
        }
        else
        {
          type = -1;
          return -1;
        }
      }
      type = 1;
      return 2;
    }
    // Multiplication operator
    case '*':
    {
      type = 1;
      (*argument)++;
      return 3;
    }
    // Division operator
    case '/':
    {
      type = 1;
      (*argument)++;
      return 4;
    }
    // Modulus operator
    case '%':
    {
      type = 1;
      (*argument)++;
      return 5;
    }

    // Exponent operator
    case '^':
    {
      type = 1;
      (*argument)++;
      return 6;
    }

    // Bitwise AND operator
    case 'a':
    {
      type = 1;
      (*argument)++;
      return 7;
    }

    // Bitwise OR operator
    case 'o':
    {
      type = 1;
      (*argument)++;
      return 8;
    }

    // Bitwise XOR operator
    case 'x':
    {
      type = 1;
      (*argument)++;
      return 9;
    }

    // Less than/bitshift left
    case '<':
    {
      type = 1;
      (*argument)++;
      if(**argument == '<')
      {
        (*argument)++;
        return 10;
      }
      if(**argument == '=')
      {
        (*argument)++;
        return 14;
      }
      return 13;
    }
    // Greater than/bitshift right
    case '>':
    {
      type = 1;
      (*argument)++;
      if(**argument == '>')
      {
        (*argument)++;
        return 11;
      }
      if(**argument == '=')
      {
        (*argument)++;
        return 16;
      }
      return 15;
    }
    // Equality
    case '=':
    {
      type = 1;
      (*argument)++;
      return 12;
    }
    // Logical negation
    case '!':
    {
      type = 1;
      if(*(*argument + 1) == '=')
      {
        (*argument) += 2;
        return 17;
      }
    }

    // One's complement
    case '~':
    {
      (*argument)++;
      if(!isspace(**argument))
      {
        int t2;
        long int val = parse_argument(mzx_world, argument, t2, id);
        if((t2 == 0) || (t2 == 2))
        {
          val = ~val;
          type = 2;
          return val;
        }
        else
        {
          type = -1;
          return -1;
        }
      }
    }

    // # is the last expression value, 32bit.
    case '#':
    {
      type = 0;
      (*argument)++;
      return last_val;
    }

    // The evil null terminator...
    case '\0':
    {
      type = -1;
      return -1;
    }

    // Parentheses! Recursive goodness...
    // Treat a valid return as an argument.
    case '(':
    {
      int error;
      long int val;
      char *a_ptr = (*argument) + 1;
      val = parse_expression(mzx_world, &a_ptr, &error, id);
      if(error)
      {
        type = -1;
        return -1;
      }
      else
      {
        *argument = a_ptr;
        type = 0;
        return val;
      }
    }

    // Begins with a ' or a & makes it a message ('counter')
    case '&':
    case '\'':
    {
      // Find where the next ' or & is
      char t_char = first_char;
      (*argument)++;
      int count = 0;
      char temp[256];
      char temp2[256];
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
                type = -1;
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
            type = -1;
            return -1;
          }

          temp[count] = **argument;
          (*argument)++;
          count++;
        }
      }
      type = 0;
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
        long int val = strtol(*argument, &end_p, 0);
        *argument = end_p;
        type = 0;
        return val;
      }
      else
      {
        // It's not so good..
        type = -1;
        return -1;
      }
    }
  }
}

void skip_whitespace(char **expression)
{
  while(isspace(**expression))
  {
    (*expression)++;
  }
}

int evaluate_operation(int operand_a, int c_operator, int operand_b)
{
  switch(c_operator)
  {
    // Addition
    case 1:
    {
      return(operand_a + operand_b);
    }
    // Subtraction
    case 2:
    {
      return(operand_a - operand_b);
    }
    // Multiplication
    case 3:
    {
      return(operand_a * operand_b);
    }
    // Division
    case 4:
    {
      if(operand_b == 0)
      {
        return 0;
      }
      return(operand_a / operand_b);
    }
    // Modulo
    case 5:
    {
      return(operand_a % operand_b);
    }
    // Exponent
    case 6:
    {
      int i;
      long int val = 1;
      if(operand_a == 0)
      {
        return(0);
      }
      if(operand_a == 1)
      {
        return(1);
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
    // Bitwise AND
    case 7:
    {
      return (operand_a & operand_b);
    }
    // Bitwise OR
    case 8:
    {
      return (operand_a | operand_b);
    }
    // Bitwise XOR
    case 9:
    {
      return (operand_a ^ operand_b);
    }
    // Logical bitshift left
    case 10:
    {
      return (operand_a << operand_b);
    }
    // Locical bitshift right
    case 11:
    {
      return (unsigned int)(operand_a) >> operand_b;
    }
    // Equality
    case 12:
    {
      return (operand_a == operand_b);
    }
    // less than
    case 13:
    {
      return (operand_a < operand_b);
    }
    // less than or equal to
    case 14:
    {
      return (operand_a <= operand_b);
    }
    // greater than
    case 15:
    {
      return (operand_a > operand_b);
    }
    // greater than or equal to
    case 16:
    {
      return (operand_a >= operand_b);
    }
    // not equal to
    case 17:
    {
      return (operand_a != operand_b);
    }
    default:
    {
      return(operand_a);
    }

  }
}

