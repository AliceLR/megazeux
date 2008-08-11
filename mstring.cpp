/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
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
 
#include <stdlib.h>
#include <stdio.h>
#include "string.h"
#include "data.h"

// Returns the type of string a parameter is:
//  0: Not a string
//  1: Is a normal string
//  2: Is a character of a string

char strings[16][64];

int string_type(char far *expression)
{
  if(!strn_cmp(expression, "$string", 7))
  {
    if(expression[7])
    {
      char far *next;
      int str_num = strtol(expression + 7, &next, 10);
      if(str_num < 16)
      {
        if(*next == '.')
        {
          return(2);
        }
        else
        {
          return(1);
        }
      }
    }
  }
  return(0);
}

char far *get_string(char far *expression, char far *str_buffer)
{
  char far *next;
  int str_num = strtol((expression + 7), &next, 10);
  int off = 0;
  int cap = 63;
  // Offsetted
  if(*next == '+')
  {
    off = strtol(next + 1, &next, 10);
    if(off > 62) off = 62;
  }
  // Capped
  if(*next == '#')
  {
    cap = strtol(next + 1, NULL, 10);
    if(cap > 63) cap = 63;
  }
  if((cap + off) > 63)
  {
    cap = 63 - off;
  }
  mem_cpy(str_buffer, strings[str_num] + off, cap);
  str_buffer[cap] = '\0';
  return(str_buffer);
}

char get_str_char(char far *expression)
{
  char far *next;
  int str_num = strtol((expression + 7), &next, 10);
  int ch_num = strtol(next + 1, NULL, 10);
  if(ch_num < 63)
  {
    return(strings[str_num][ch_num]);
  }
}

int get_str_int(char far *expression)
{
  int str_num = strtol((expression + 7), NULL, 10);
  return(strtol(strings[str_num], NULL, 10));
  return(0);
}

void set_string(char far *expression, char *set)
{
  char *next;
  int str_num = strtol((expression + 7), &next, 10);
  int cap = 63;
  int off = 0;
  if(*next == '+')
  {
    off = strtol(next + 1, &next, 10);
    if(off > 62) off = 62;
  }
  if(*next == '#')
  {
    cap = strtol(next + 1, NULL, 10);
    if(cap > 63) cap = 63;
  }  
  mem_cpy(strings[str_num] + off, set, cap);
}

void set_str_char(char far *expression, char set)
{
  char far *next;
  int str_num = strtol((expression + 7), &next, 10);
  int ch_num = strtol(next + 1, NULL, 10);
  if(ch_num < 63)
  {
    strings[str_num][ch_num] = set;
  }
}

void load_string_board(char far *expression, int w, int h, char l, 
 char far *src, int width)
{
  int str_num = strtol((expression + 7), NULL, 10);
  int i;
  int wl = w;
  char t_buf[64];
  t_buf[63] = '\0';
  char *t_pos = t_buf;

  if((w * h) > 63)
  {
    if(w > 63)
    {
      w = 63;
      h = 1;
    }
    else
    {
      h = (63 / w) + 1;
      wl = 63 % w;
    }
  }
  
  for(i = 0; i < (h - 1); i++)
  {
    mem_cpy(t_pos, src, w);
    t_pos += w;
    src += width;
  }
  mem_cpy(t_pos, src, wl);
  t_pos += wl;
  *t_pos = '\0';
  if(l)
  {
    for(i = 0; i < ((w * h) + wl); i++)
    {
      if(t_buf[i] == l)
      {
        t_buf[i] = '\0';
        break;
      }
    }
  }
  set_string(expression, t_buf);
}


void set_str_int(char far *expression, int val)
{
  int str_num = strtol((expression + 7), NULL, 10);
  char temp[7];
  sprintf(temp, "%d", val);
  str_cpy(strings[str_num], temp);
}

