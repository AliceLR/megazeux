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
#include "rasm.h"

// Only pass these evaluators valid expressions. The compiler should take care
// of that part, so don't call it outside of evaluating robot code.

enum op
{
  OP_ADDITION,
  OP_SUBTRACTION,
  OP_MULTIPLICATION,
  OP_DIVISION,
  OP_MODULUS,
  OP_EXPONENTIATION,
  OP_AND,
  OP_OR,
  OP_XOR,
  OP_BITSHIFT_LEFT,
  OP_BITSHIFT_RIGHT,
  OP_ARITHMETIC_BITSHIFT_RIGHT,
  OP_EQUAL,
  OP_LESS_THAN,
  OP_GREATER_THAN,
  OP_GREATER_THAN_OR_EQUAL,
  OP_LESS_THAN_OR_EQUAL,
  OP_NOT_EQUAL
};

static int last_val;

static char *expr_skip_whitespace(char *expression)
{
  while(isspace((int)*expression))
    expression++;
  return expression;
}

static int parse_argument(struct world *mzx_world, char **_argument,
 int *type, int id)
{
  char *argument = *_argument;
  int first_char = *argument;

  // Test the first one.

  switch(first_char)
  {
    // Addition operator
    case '+':
    {
      *type = 1;
      *_argument = argument + 1;
      return OP_ADDITION;
    }

    // Subtraction operator
    case '-':
    {
      argument++;
      if(!isspace((int)*argument))
      {
        int t2, val = parse_argument(mzx_world, &argument, &t2, id);
#ifndef CONFIG_DEBYTECODE
        if((t2 != 0) && (t2 != 2))
        {
          *type = -1;
          *_argument = argument;
          return -1;
        }
#endif
        val = -val;
        *type = 2;
        *_argument = argument;
        return val;
      }

      *type = 1;
      *_argument = argument;
      return OP_SUBTRACTION;
    }

    // Multiplication operator
    case '*':
    {
      *type = 1;
      argument++;
#ifdef CONFIG_DEBYTECODE
      if(*argument == '*')
      {
        *_argument = argument + 1;
        return OP_EXPONENTIATION;
      }
      else
#endif
      {
        *_argument = argument;
        return OP_MULTIPLICATION;
      }
    }

    // Division operator
    case '/':
    {
      *type = 1;
      *_argument = argument + 1;
      return OP_DIVISION;
    }

    // Modulus operator
    case '%':
    {
      *type = 1;
      *_argument = argument + 1;
      return OP_MODULUS;
    }

#ifndef CONFIG_DEBYTECODE
    // Exponent operator
    case '^':
    {
      *type = 1;
      *_argument = argument + 1;
      return OP_EXPONENTIATION;
    }
#endif

    // Bitwise AND operator
#ifdef CONFIG_DEBYTECODE
    case '&':
#else
    case 'a':
#endif
    {
      *type = 1;
      *_argument = argument + 1;
      return OP_AND;
    }

    // Bitwise OR operator
#ifdef CONFIG_DEBYTECODE
    case '|':
#else
    case 'o':
#endif
    {
      *type = 1;
      *_argument = argument + 1;
      return OP_OR;
    }

    // Bitwise XOR operator
#ifdef CONFIG_DEBYTECODE
    case '^':
#else
    case 'x':
#endif
    {
      *type = 1;
      *_argument = argument + 1;
      return OP_XOR;
    }

    // Less than/bitshift left
    case '<':
    {
      *type = 1;
      argument++;
      if(*argument == '<')
      {
        *_argument = argument + 1;
        return OP_BITSHIFT_LEFT;
      }
      if(*argument == '=')
      {
        *_argument = argument + 1;
        return OP_LESS_THAN_OR_EQUAL;
      }
      *_argument = argument;
      return OP_LESS_THAN;
    }

    // Greater than/bitshift right
    case '>':
    {
      *type = 1;
      argument++;
      if(*argument == '>')
      {
        if(argument[1] == '>')
        {
          *_argument = argument + 2;
          return OP_ARITHMETIC_BITSHIFT_RIGHT;
        }
        *_argument = argument + 1;
        return OP_BITSHIFT_RIGHT;
      }
      if(*argument == '=')
      {
        *_argument = argument + 1;
        return OP_GREATER_THAN_OR_EQUAL;
      }
      *_argument = argument;
      return OP_GREATER_THAN;
    }

    // Equality
    case '=':
    {
      *type = 1;
      *_argument = argument + 1;
      return OP_EQUAL;
    }

    // Inequality (if there is no =, we do one's compliment)
    case '!':
    {
      if(*(argument + 1) == '=')
      {
        *type = 1;
        *_argument = argument + 2;
        return OP_NOT_EQUAL;
      }
    }

    // One's complement
    case '~':
    {
      int t2, val;

      argument++;
      val = parse_argument(mzx_world, &argument, &t2, id);

#ifndef CONFIG_DEBYTECODE
      if((t2 != 0) && (t2 != 2))
      {
        *type = -1;
        *_argument = argument;
        return -1;
      }
#endif

      val = ~val;
      *type = 2;
      *_argument = argument;
      return val;
    }

#ifndef CONFIG_DEBYTECODE
    // # is the last expression value, 32bit.
    case '#':
    {
      *type = 0;
      *_argument = argument + 1;
      return last_val;
    }

    // The evil null terminator...
    case '\0':
    {
      *type = -1;
      return -1;
    }
#endif

    // Parentheses! Recursive goodness...
    // Treat a valid return as an argument.
    case '(':
    {
      int val, error;
      argument++;
      val = parse_expression(mzx_world, &argument, &error, id);
#ifndef CONFIG_DEBYTECODE
      if(error)
      {
        *type = -1;
        *_argument = argument;
        return -1;
      }
#endif
      *type = 0;
      *_argument = argument;
      return val;
    }

#ifdef CONFIG_DEBYTECODE

    // Begins with a ` makes it a counter name
    case '`':
    {
      char name_translated[ROBOT_MAX_TR];
      argument++;

      *type = 0;
      *_argument = tr_msg_ext(mzx_world, argument, id, name_translated, '`');
      return get_counter(mzx_world, name_translated, id);
    }

#else // !CONFIG_DEBYTECODE

    // Begins with a ' or a & makes it a message ('counter')
    case '&':
    case '\'':
    {
      // Find where the next ' or & is
      char t_char = first_char;
      char temp[256];
      char temp2[256];
      int count = 0;

      argument++;

      // Remember, null terminator is evil; if it's hit exit completely.
      while(((*argument) != t_char) && (count < 256))
      {
        // If a nested expression is hit closing 's should be ignored
        if((*argument) == '(')
        {
          int close_paren_count = 1;
          // The number of )'s to expect.. finding one decreases it..
          // And finding a ( increases it.

          while((close_paren_count) && (count < 256))
          {
            temp[count] = *argument;
            argument++;
            count++;

            switch(*argument)
            {
              case '\0':
              {
                *type = -1;
                *_argument = argument;
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
          if(*argument == '\0')
          {
            *type = -1;
            *_argument = argument;
            return -1;
          }

          temp[count] = *argument;
          argument++;
          count++;
        }
      }

      *type = 0;
      *_argument = argument + 1;

      temp[count] = '\0';
      tr_msg(mzx_world, temp, id, temp2);
      return get_counter(mzx_world, temp2, id);
    }

#endif // !CONFIG_DEBYTECODE

    // Otherwise, it must be an integer, or just something invalid.
    default:
    {
      if((first_char >= '0') && (first_char <= '9'))
      {
        char *end_p;
        int val = (int)strtol(argument, &end_p, 0);
        *type = 0;
        *_argument = end_p;
        return val;
      }
      else
      {
#ifdef CONFIG_DEBYTECODE
        char *end_p = find_non_identifier_char(argument);
        char temp = *end_p;
        int value;

        *end_p = 0;
        value = get_counter(mzx_world, argument, id);
        *end_p = temp;

        *type = 0;
        *_argument = end_p;
        return value;
#else
        // It's not so good..
        *type = -1;
        *_argument = argument;
        return -1;
#endif
      }
    }
  }
}

static int evaluate_operation(int operand_a, enum op c_operator, int operand_b)
{
  switch(c_operator)
  {
    case OP_ADDITION:
      return operand_a + operand_b;

    case OP_SUBTRACTION:
      return operand_a - operand_b;

    case OP_MULTIPLICATION:
      return operand_a * operand_b;

    case OP_DIVISION:
    {
      if(operand_b == 0)
        return 0;

      return operand_a / operand_b;
    }

    case OP_MODULUS:
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

    case OP_EXPONENTIATION:
    {
      int i;
      int val = 1;

      if(operand_a == 0)
        return 0;

      if(operand_a == 1)
        return 1;

      if(operand_b < 0)
      {
        if(operand_a == -1)
          operand_b *= -1;
        else
          return 0;
      }

      for(i = 0; i < operand_b; i++)
        val *= operand_a;

      return val;
    }

    case OP_AND:
      return operand_a & operand_b;

    case OP_OR:
      return operand_a | operand_b;

    case OP_XOR:
      return (operand_a ^ operand_b);

    case OP_BITSHIFT_LEFT:
      return operand_a << operand_b;

    case OP_BITSHIFT_RIGHT:
      return (unsigned int)(operand_a) >> operand_b;

    case OP_ARITHMETIC_BITSHIFT_RIGHT:
      return (signed int)(operand_a) >> operand_b;

    case OP_EQUAL:
      return operand_a == operand_b;

    case OP_LESS_THAN:
      return operand_a < operand_b;

    case OP_LESS_THAN_OR_EQUAL:
      return operand_a <= operand_b;

    case OP_GREATER_THAN:
      return operand_a > operand_b;

    case OP_GREATER_THAN_OR_EQUAL:
      return operand_a >= operand_b;

    case OP_NOT_EQUAL:
      return operand_a != operand_b;

    default:
      return operand_a;
  }
}

int parse_expression(struct world *mzx_world, char **_expression, int *error,
 int id)
{
  char *expression = *_expression;
  int operand_val;
  int current_arg;
  int c_operator;
  int value;

  *error = 0;

  // Skip initial whitespace..
  expression = expr_skip_whitespace(expression);
  value = parse_argument(mzx_world, &expression, &current_arg, id);

#ifndef CONFIG_DEBYTECODE
  if((current_arg != 0) && (current_arg != 2))
  {
    // First argument must be a value type.
    *error = 1;
    value = -99;
    goto err_out;
  }
#endif

  expression = expr_skip_whitespace(expression);

  while(1)
  {
    if(*expression == ')')
    {
      expression++;
      break;
    }

    c_operator = parse_argument(mzx_world, &expression, &current_arg, id);
    // Next arg must be an operator, unless it's a negative number,
    // in which case it's considered + num
    if(current_arg == 2)
    {
      value = evaluate_operation(value, OP_ADDITION, c_operator);
    }
    else
    {
#ifndef CONFIG_DEBYTECODE
      if(current_arg != 1)
      {
        *error = 2;
        value = -100;
        goto err_out;
      }
#endif

      expression = expr_skip_whitespace(expression);
      operand_val = parse_argument(mzx_world, &expression, &current_arg, id);

#ifndef CONFIG_DEBYTECODE
      // And now it must be an integer.
      if((current_arg != 0) && (current_arg != 2))
      {
        *error = 3;
        value = -102;
        goto err_out;
      }
#endif

      value = evaluate_operation(value, (enum op)c_operator, operand_val);
    }
    expression = expr_skip_whitespace(expression);
  }

  last_val = value;

#ifndef CONFIG_DEBYTECODE
err_out:
#endif
  *_expression = expression;
  return value;
}

#ifdef CONFIG_DEBYTECODE

int parse_string_expression(struct world *mzx_world, char **_expression,
 int id, char *output)
{
  char *expression = *_expression;
  int expression_length = 0;

  while(1)
  {
    expression = expr_skip_whitespace(expression);

    switch(*expression)
    {
      case '`':
      {
        struct string string;
        char name_translated[ROBOT_MAX_TR];
        expression++;

        expression =
         tr_msg_ext(mzx_world, expression, id, name_translated, '`');

        get_string(mzx_world, name_translated, &string, id);

        memcpy(output, string.value, string.length);
        output += string.length;

        expression_length += string.length;
        break;
      }

      case '"':
      {
        int literal_length;
        expression++;

        expression = tr_msg_ext(mzx_world, expression, id, output, '"');
        literal_length = strlen(output);

        output += literal_length;
        expression_length += literal_length;
        break;
      }

      case '>':
        expression++;
        *_expression = expression;
        return expression_length;

      default:
      {
        char temp;
        char *next;
        struct string string;

        next = find_non_identifier_char(expression);

        temp = *next;
        *next = 0;
        get_string(mzx_world, expression, &string, id);
        *next = temp;

        expression = next;

        memcpy(output, string.value, string.length);
        output += string.length;
        expression_length += string.length;
        break;
      }
    }
  }
}

#endif // CONFIG_DEBYTECODE
