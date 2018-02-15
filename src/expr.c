/* MegaZeux
 *
 * Copyright (C) 2002 Gilead Kutnick <exophase@adelphia.net>
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "expr.h"

#include "counter.h"
#include "rasm.h"
#include "robot.h"
#include "str.h"
#include "world.h"
#include "world_struct.h"

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
  OP_NOT_EQUAL,
  OP_TERNARY
};

#ifndef CONFIG_DEBYTECODE

/* This new expression parser for legacy Robotic is faster than the older
 * parser. It's not suited to the more complex debytecode (mainly thanks to
 * interpolation), so I've left the old parser intact below for debytecode.
 *
 * Error codes:
 * -1 - unknown
 * 0  - success
 * 1  - invalid operand
 * 2  - invalid operator
 * 3  - stack overflow
 * 4  - stack underflow
 */

#define EXPR_BUFFER_SIZE 512
#define EXPR_STACK_SIZE 32

#define EXPR_STATE_START_OPERAND 1
#define EXPR_STATE_PARSE_OPERAND 2
#define EXPR_STATE_AMP 4
#define EXPR_STATE_PUSH 8
#define EXPR_STATE_PUSH_INTERPOLATION 16
#define EXPR_STATE_INTERPOLATING 32
#define EXPR_STATE_TERNARY_MIDDLE 64

struct expr_stack {
  int buf_start;
  int operand;
  char operator;
  char state;
};

static char buffer[EXPR_BUFFER_SIZE];
static struct expr_stack stack[EXPR_STACK_SIZE];
static int stack_alloc = EXPR_STACK_SIZE;
static char *buf_alloc = buffer + EXPR_BUFFER_SIZE;

// Push a null onto the buffer to help with unary operator processing.

#define PUSH_STACK(st)                \
{                                     \
  if(pos >= stack_alloc)              \
  {                                   \
    *error = 3;                       \
    goto err_out;                     \
  }                                   \
                                      \
  stack[pos].buf_start = buf_start;   \
  stack[pos].operator = operator;     \
  stack[pos].operand = operand_a;     \
  stack[pos].state = st;              \
  pos++;                              \
                                      \
  *buf_pos = '\0';                    \
  buf_pos++;                          \
                                      \
  buf_start = buf_pos - buffer;       \
  operator = OP_ADDITION;             \
  operand_a = 0;                      \
}

// if err is true, error out at the bottom of the stack
// if err is false, exit successfully at the bottom of the stack

#define POP_STACK(err)                \
{                                     \
  buf_pos--;                          \
  pos--;                              \
  if(pos >= 0)                        \
  {                                   \
    buf_start = stack[pos].buf_start; \
    operator = stack[pos].operator;   \
    operand_a = stack[pos].operand;   \
    state = stack[pos].state;         \
  }                                   \
  else if(err)                        \
  {                                   \
    *error = 4;                       \
    goto err_out;                     \
  }                                   \
  else                                \
  {                                   \
    continue;                         \
  }                                   \
}

static inline void skip_spaces(char **_expression)
{
  char *expression = *_expression;

  while(isspace((int)*expression))
    expression++;

  *_expression = expression;
}

int parse_expression(struct world *mzx_world, char **_expression, int *error,
 int id)
{
  char number_buffer[16];

  // Position in stack
  int pos = 0;
  char current_char;

  // Current positions in the buffer and in the original expression
  char *buf_pos = buffer;
  char *expression = *_expression;

  // For the current level of expression:
  // Offset in the buffer where operand B's name starts
  int buf_start = 0;

  // Operator ID, values for operands
  enum op operator = OP_ADDITION;
  int operand_a = 0;
  int operand_b = 0;

  // Parsing state
  char state = 0;

  *error = 0;

  // Note: initial open paren already skipped.
  do
  {
    // Get next operand


    // Figure out what our operand actually is.
    // Also handle the prefixed unary operators.

    if(!(state & EXPR_STATE_START_OPERAND))
    {
      // We need to skip spaces, first
      skip_spaces(&expression);

      current_char = *expression;
      expression++;

      switch(current_char)
      {
        // One's compliment
        case '~':
        case '!':
          // Put it on the name buffer and handle it once we have a value.
          *buf_pos = '~';
          buf_pos++;
          continue;

        // Unary negation
        case '-':
          // Put it on the name buffer and handle it once we have a value.
          *buf_pos = '-';
          buf_pos++;
          continue;

        // Nested expression
        case '(':
          state |= EXPR_STATE_PUSH;
          break;

        // Counter
        case '\'':
          state |= EXPR_STATE_PARSE_OPERAND;
          break;

        // Also a counter
        case '&':
          state |= EXPR_STATE_PARSE_OPERAND | EXPR_STATE_AMP;
          break;

        // Otherwise, it must be an integer, or just something invalid.
        default:
        {
          if((current_char >= '0') && (current_char <= '9'))
          {
            // Integer
            char *end_p;
            operand_b = (int)strtol(expression - 1, &end_p, 0);
            expression = end_p;
            break;
          }
          else
          {
            // Invalid operand
            *error = 1;
            goto err_out;
          }
        }
      }
      state |= EXPR_STATE_START_OPERAND;
      buf_start = buf_pos - buffer;
    }


    // Parse a named operand.
    if(state & EXPR_STATE_PARSE_OPERAND)
    {
      do
      {
        current_char = *expression;
        expression++;

        if(buf_pos >= buf_alloc)
        {
          // Truncate the counter name and continue.
          buf_pos = buf_alloc - 1;
          *buf_pos = '\0';
          break;
        }

        if(current_char == '\'')
        {
          if(state & EXPR_STATE_INTERPOLATING)
          {
            // Unexpected end of interpolation!
            // Back up expression so it finds ' the next level down.
            expression--;
          }
          *buf_pos = '\0';
          break;
        }
        else

        if(current_char == '&')
        {
          if(state & EXPR_STATE_AMP)
          {
            // Already reading a &counter&? This is the end
            *buf_pos = '\0';
          }
          else

          if(*expression == '&')
          {
            // Two &&s in a counter name -> one & char
            expression++;
            *buf_pos = '&';
            buf_pos++;
          }

          else
          {
            // Interpolation
            state |= EXPR_STATE_PUSH_INTERPOLATION;
          }
          break;
        }
        else

        if(current_char == '\0')
        {
          // Invalid -- truncated operand
          *error = 1;
          goto err_out;
        }
        else

        if(current_char == '(')
        {
          state |= EXPR_STATE_PUSH;
          break;
        }

        else
        {
          *buf_pos = current_char;
          buf_pos++;
        }
      }
      while(1);
    }


    // Push state onto stack for expressions and interpolation
    if(state & (EXPR_STATE_PUSH|EXPR_STATE_PUSH_INTERPOLATION))
    {
      char push_state = state & ~EXPR_STATE_PUSH &
       ~EXPR_STATE_PUSH_INTERPOLATION;

      PUSH_STACK(push_state);

      // Set up for interpolating a counter/string
      if(state & EXPR_STATE_PUSH_INTERPOLATION)
      {
        state = EXPR_STATE_START_OPERAND | EXPR_STATE_PARSE_OPERAND
         | EXPR_STATE_AMP | EXPR_STATE_INTERPOLATING;
      }
      else
      {
        state = 0;
      }

      continue;
    }


    // Interpolate a counter or string
    if(state & EXPR_STATE_INTERPOLATING)
    {
      int len;
      char *src;
      buf_pos = buffer + buf_start;

      // Input
      if(!strcasecmp(buf_pos, "INPUT"))
      {
        src = mzx_world->current_board->input_string;
        len = strlen(src);
      }
      else

      // Counter or string
      if(is_string(buf_pos))
      {
        // Write the value of the counter name
        struct string str_src;

        get_string(mzx_world, buf_pos, &str_src, 0);

        src = str_src.value;
        len = str_src.length;
      }
      else

      // #(counter) is a hex representation.
      if(*buf_pos == '+')
      {
        sprintf(number_buffer, "%x",
         get_counter(mzx_world, buf_pos + 1, id));

        src = number_buffer;
        len = strlen(number_buffer);
      }
      else

      if(*buf_pos == '#')
      {
        sprintf(number_buffer, "%02x",
         get_counter(mzx_world, buf_pos + 1, id));

        src = number_buffer;
        len = 2;
      }
      else
      {
        sprintf(number_buffer, "%d",
         get_counter(mzx_world, buf_pos, id));

        src = number_buffer;
        len = strlen(number_buffer);
      }

      // Pop before writing to remove the unary null terminator
      POP_STACK(true);

      if(len > (buf_alloc - buf_pos))
      {
        // Truncate the interpolated value and continue
        len = buf_alloc - buf_pos;
      }

      memcpy(buf_pos, src, len);
      buf_pos += len;
      continue;
    }


    // Translate operand B
    buf_pos = buffer + buf_start;
    if(state & EXPR_STATE_PARSE_OPERAND)
    {
      operand_b = get_counter(mzx_world, buf_pos, id);
    }

    // Clear states for next operand
    state &= EXPR_STATE_TERNARY_MIDDLE;


    // Apply unary operators
    while((--buf_pos) >= buffer)
    {
      switch(*buf_pos)
      {
        case '~':
          operand_b = ~operand_b;
          continue;

        case '-':
          operand_b = -operand_b;
          continue;
      }
      break;
    }

    // Shift the buffer pos in front of the unary null, or to the beginning
    buf_pos++;


    // Perform operation
    switch(operator)
    {
      case OP_ADDITION:
        operand_a += operand_b;
        break;

      case OP_SUBTRACTION:
        operand_a -= operand_b;
        break;

      case OP_MULTIPLICATION:
        operand_a *= operand_b;
        break;

      case OP_DIVISION:
      {
        if(operand_b == 0)
          operand_a = 0;

        else
          operand_a /= operand_b;

        break;
      }

      case OP_MODULUS:
      {
        int val;

        if(operand_b == 0)
        {
          operand_a = 0;
          break;
        }

        val = operand_a % operand_b;

        // Converted C99 regulated truncated modulus to
        // the more useful (for us) floored modulus
        // Source:
        // Division and Modulus for Computer Scientists
        // DAAN LEIJEN
        // University of Utrecht

        if((val < 0) ^ (operand_b < 0))
          val += operand_b;

        operand_a = val;
        break;
      }

      case OP_EXPONENTIATION:
      {
        int i;
        int val = 1;

        // a==0 -> result = 0
        if(operand_a == 0)
          break;

        // a==1 -> result = 1
        if(operand_a == 1)
          break;

        // a==-1 -> result = 1 if b is even, -1 if b is odd
        if(operand_a == -1)
          operand_b &= 1;

        // no floating point support :(
        if(operand_b < 0)
        {
          operand_a = 0;
          break;
        }

        for(i = 0; i < operand_b; i++)
          val *= operand_a;

        operand_a = val;
        break;
      }

      case OP_AND:
        operand_a &= operand_b;
        break;

      case OP_OR:
        operand_a |= operand_b;
        break;

      case OP_XOR:
        operand_a ^= operand_b;
        break;

      case OP_BITSHIFT_LEFT:
        operand_a <<= operand_b;
        break;

      case OP_BITSHIFT_RIGHT:
        operand_a = ((unsigned int)(operand_a) >> operand_b);
        break;

      case OP_ARITHMETIC_BITSHIFT_RIGHT:
        operand_a = ((signed int)(operand_a) >> operand_b);
        break;

      case OP_EQUAL:
        operand_a = (operand_a == operand_b);
        break;

      case OP_LESS_THAN:
        operand_a = (operand_a < operand_b);
        break;

      case OP_LESS_THAN_OR_EQUAL:
        operand_a = (operand_a <= operand_b);
        break;

      case OP_GREATER_THAN:
        operand_a = (operand_a > operand_b);
        break;

      case OP_GREATER_THAN_OR_EQUAL:
        operand_a = (operand_a >= operand_b);
        break;

      case OP_NOT_EQUAL:
        operand_a = (operand_a != operand_b);
        break;

      default:
        break;
    }


    // Get next operator -- we need to skip any spaces first
    // Prefix operators are handled during the operand search, not here.
    skip_spaces(&expression);

    current_char = *expression;
    expression++;

    switch(current_char)
    {
      // Ternary operator left (2.90+)
      case '?':
      {
        if(mzx_world->version < V290)
          goto err_out;

        // True
        if(operand_a)
        {
          PUSH_STACK(state);
          state = EXPR_STATE_TERNARY_MIDDLE;
        }

        // False - seek next ':'
        else
        {
          int paren_level = 0;
          int ternary_level = 0;
          while(1)
          {
            current_char = *expression;
            expression++;

            if(current_char == '\0')
            {
              *error = 2;
              goto err_out;
            }
            else

            if(current_char == '(')
            {
              paren_level++;
            }
            else

            if(current_char == ')')
            {
              if(paren_level <= 0)
              {
                *error = 2;
                goto err_out;
              }
              paren_level--;
            }
            else

            if(current_char == '?')
            {
              if(paren_level == 0)
                ternary_level++;
            }
            else

            if(current_char == ':')
            {
              if(paren_level == 0)
              {
                if(ternary_level <= 0)
                  break;

                ternary_level--;
              }
            }
          }
          // Preserve ternary middle state in case these are nested
          state = state & EXPR_STATE_TERNARY_MIDDLE;
        }
        operator = OP_ADDITION;
        operand_a = 0;
        break;
      }

      // Ternary operator right
      case ':':
      {
        int value = operand_a;
        int ternary_level = 0;
        int paren_level = 0;

        if(mzx_world->version < V290)
          goto err_out;

        if(!(state & EXPR_STATE_TERNARY_MIDDLE))
        {
          *error = 2;
          goto err_out;
        }

        POP_STACK(true);

        // Seek next ')'
        while(1)
        {
          current_char = *expression;
          expression++;

          if(current_char == '\0')
          {
            *error = 2;
            goto err_out;
          }
          else

          // Validate ternary operators while we're at it (actually necessary)
          if(current_char == '?' && paren_level == 0)
          {
            ternary_level++;
          }
          else

          if(current_char == ':' && paren_level == 0)
          {
            if(ternary_level == 0)
            {
              // Unroll ternary stack levels as-needed
              if(state & EXPR_STATE_TERNARY_MIDDLE)
              {
                POP_STACK(true);
              }
              else
              {
                *error = 2;
                goto err_out;
              }
            }
            else
            {
              ternary_level--;
            }
          }
          else

          if(current_char == '(')
          {
            paren_level++;
          }
          else

          if(current_char == ')')
          {
            if(paren_level <= 0)
              break;

            paren_level--;
          }
        }

        operand_a = value;

        // Proceed into the ')' handler:
      }

      /* fallthrough */

      // End of expression
      case ')':
      {
        int value = operand_a;
        int len;

        // Invalid end of expression where : should exist
        if(state & EXPR_STATE_TERNARY_MIDDLE)
        {
          *error = 2;
          goto err_out;
        }

        // Pop before writing to (if necessary) overwrite the unary null
        POP_STACK(false);

        // If we're in the middle of an operand, print to the buffer
        if(state & EXPR_STATE_PARSE_OPERAND)
        {
          sprintf(number_buffer, "%d", value);
          len = strlen(number_buffer);

          if(len+1 > (buf_alloc - buf_pos))
          {
            // Truncate the interpolated value and continue.
            len = buf_alloc - buf_pos - 1;
          }

          strcpy(buf_pos, number_buffer);
          buf_pos += len;
        }

        // Otherwise, this is the new operand
        else
        {
          operand_b = value;
        }
        continue;
      }

      // Addition operator
      case '+':
      {
        operator = OP_ADDITION;
        break;
      }

      // Subtraction operator
      case '-':
      {
        operator = OP_SUBTRACTION;
        break;
      }

      // Multiplication operator
      case '*':
      {
        operator = OP_MULTIPLICATION;
        break;
      }

      // Division operator
      case '/':
      {
        operator = OP_DIVISION;
        break;
      }

      // Modulus operator
      case '%':
      {
        operator = OP_MODULUS;
        break;
      }

      // Exponent operator
      case '^':
      {
        operator = OP_EXPONENTIATION;
        break;
      }

      // Bitwise AND operator
      case 'a':
      {
        operator = OP_AND;
        break;
      }

      // Bitwise OR operator
      case 'o':
      {
        operator = OP_OR;
        break;
      }

      // Bitwise XOR operator
      case 'x':
      {
        operator = OP_XOR;
        break;
      }

      // Less than/bitshift left
      case '<':
      {
        if(*expression == '<')
        {
          expression++;
          operator = OP_BITSHIFT_LEFT;
          break;
        }
        if(*expression == '=')
        {
          expression++;
          operator = OP_LESS_THAN_OR_EQUAL;
          break;
        }
        operator = OP_LESS_THAN;
        break;
      }

      // Greater than/bitshift right
      case '>':
      {
        if(*expression == '>')
        {
          expression++;
          if(*expression == '>')
          {
            expression++;
            operator = OP_ARITHMETIC_BITSHIFT_RIGHT;
            break;
          }
          operator = OP_BITSHIFT_RIGHT;
          break;
        }
        if(*expression == '=')
        {
          expression++;
          operator = OP_GREATER_THAN_OR_EQUAL;
          break;
        }
        operator = OP_GREATER_THAN;
        break;
      }

      // Equality
      case '=':
      {
        operator = OP_EQUAL;
        break;
      }

      // Inequality
      case '!':
      {
        if(*expression == '=')
        {
          expression++;
          operator = OP_NOT_EQUAL;
          break;
        }
        // Invalid operator
        goto err_out;
      }

      // Null terminator or unrecognized symbol
      case '\0':
      default:
        // Invalid operator
        *error = 2;
        goto err_out;
    }
  }
  while(pos >= 0);

  *_expression = expression;
  return operand_a;

err_out:
  if(!*error)
    *error = -1;

  *_expression = expression;
  return 0;
}


#else /* CONFIG_DEBYTECODE */


/* This is the old expression parser in its full glory.
 *
 * This parser is slower than the new one, but generally easier to understand.
 * Rather than adapt the new parser to the more complex debytecode--so it could
 * be rewritten for compiled expressions later anyway--I'm just leaving the old
 * one intact for now.
 */


// Only pass these evaluators valid expressions. The compiler should take care
// of that part, so don't call it outside of evaluating robot code.

static int last_val;

static char *expr_skip_whitespace(char *expression)
{
  while(isspace((int)*expression))
    expression++;
  return expression;
}

static int parse_argument(struct world *mzx_world, char **_argument,
 int *type, int operand_a, int id)
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
        int t2, val = parse_argument(mzx_world, &argument, &t2, 0, id);
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

    /* fallthrough */

    // One's complement
    case '~':
    {
      int t2, val;

      argument++;
      val = parse_argument(mzx_world, &argument, &t2, 0, id);

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

    // Ternary operator left (2.90+)
    case '?':
    {
      if(mzx_world->version < V290)
      {
        *type = -1;
        *_argument = argument;
        return -1;
      }

      // True - nothing to be done here
      // False - seek next ':'
      if(!operand_a)
      {
        int paren_level = 0;
        int ternary_level = 0;
        while(1)
        {
          argument++;
          first_char = *argument;

#ifndef CONFIG_DEBYTECODE
          // It's hard to ensure these are actually valid for this parser in
          // regular MZX (and we don't, since regular MZX uses a different
          // parser now). But debytecode is validated ahead of time!
          if(first_char == '\0')
          {
            *type = -1;
            *_argument = argument;
            return -1;
          }
          else
#endif // !CONFIG_DEBYTECODE

          if(first_char == '(')
          {
            paren_level++;
          }
          else

          if(first_char == ')')
          {
            if(paren_level <= 0)
            {
              *type = -1;
              *_argument = argument;
              return -1;
            }
            paren_level--;
          }
          else

          if(first_char == '?')
          {
            if(paren_level == 0)
              ternary_level++;
          }
          else

          if(first_char == ':')
          {
            if(paren_level == 0)
            {
              if(ternary_level <= 0)
                break;

              ternary_level--;
            }
          }
        }
      }

      *type = 1;
      *_argument = argument + 1;
      return OP_TERNARY;
    }

    // Ternary operator right
    case ':':
    {
      // We're only here because we finished execution of the inner argument.
      int paren_level = 0;

      if(mzx_world->version < V290)
      {
        *type = -1;
        *_argument = argument;
        return -1;
      }

      // Seek next ')'
      while(1)
      {
        argument++;
        first_char = *argument;

#ifndef CONFIG_DEBYTECODE
        if(first_char == '\0')
        {
            *type = -1;
            *_argument = argument;
            return -1;
        }
        else
#endif // !CONFIG_DEBYTECODE

        if(first_char == '(')
        {
          paren_level++;
        }
        else

        if(first_char == ')')
        {
          if(paren_level <= 0)
            break;

          paren_level--;
        }
      }

      // This performs an addition (in this case, of zero) without requiring
      // us to seek a new operand. The parser will then find the ')' and exit.
      *type = 2;
      *_argument = argument;
      return 0;
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

    case OP_TERNARY:
      return operand_b;

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
  value = parse_argument(mzx_world, &expression, &current_arg, 0, id);

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

    c_operator = parse_argument(mzx_world, &expression, &current_arg, value, id);
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
      operand_val = parse_argument(mzx_world, &expression, &current_arg, value, id);

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
 int id, char *output, size_t output_left)
{
  char *expression = *_expression;
  size_t expression_length = output_left;
  size_t copy_length;

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
        copy_length = string.length;

        if(copy_length > output_left)
          copy_length = output_left;

        memcpy(output, string.value, copy_length);
        output += copy_length;

        output_left -= copy_length;
        break;
      }

      case '"':
      {
        expression++;

        expression = tr_msg_ext(mzx_world, expression, id, output, '"');
        copy_length = strlen(output);

        if(copy_length > output_left)
          copy_length = output_left;

        output += copy_length;
        output_left -= copy_length;
        break;
      }

      case '>':
        expression++;
        *_expression = expression;
        expression_length -= output_left;
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

        copy_length = string.length;

        if(copy_length > output_left)
          copy_length = output_left;

        memcpy(output, string.value, copy_length);
        output += copy_length;
        output_left -= copy_length;
        break;
      }
    }
  }
}

#endif // CONFIG_DEBYTECODE

#endif // outer CONFIG_DEBYTECODE
