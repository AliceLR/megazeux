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

// Procedures to write hex strs onscreen

#include "hexchar.h"
#include "graphics.h"
#include <stdlib.h>
#include <string.h>

#define num2hex(x) ((x)>9 ? 87+(x) : 48+(x))

//DOES NOT PRESERVE MOUSE CURSOR
void write_hex_byte(char byte, char color, int x, int y)
{
  int t1,t2;
  t1=(byte&240)>>4;
  t1=num2hex(t1);
  t2=byte&15;
  t2=num2hex(t2);
  draw_char(t1,color,x,y);
  draw_char(t2,color,x+1,y);
}

//Set rightalign to print the rightmost char at xy and proceed to the left
//minlen is the minimum length to print. Pad with 0.
//DOES NOT PRESERVE MOUSE CURSOR
void write_number(int number, char color, int x, int y,
 int minlen, int rightalign, int base)
{
  char temp[7];
  int t1, t2;
  if(base == 10)
    sprintf(temp, "%d", number);
  else
    sprintf(temp, "%x", number);

  if(rightalign)
  {
    t1 = strlen(temp);
    if(minlen > t1)
      t1 = minlen;
    x -= t1 - 1;
  }

  if((t2 = strlen(temp)) < minlen)
  {
    t2 = minlen - t2;
    for(t1 = 0; t1 < t2; t1++)
      draw_char('0', color, x++, y);
  }

  write_string(temp, x, y, color, 0);
}

