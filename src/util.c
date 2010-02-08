/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
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

#include "util.h"

#include <time.h>
#include "SDL.h"

// Determine file size of an open FILE and rewind it

long ftell_and_rewind(FILE *f)
{
  long size;

  fseek(f, 0, SEEK_END);
  size = ftell(f);
  rewind(f);

  return size;
}

// Random function, returns an integer between 0 and range

int Random(int range)
{
  static unsigned long long seed = 0;
  unsigned long long value;

  // If the seed is 0, initialise it with time and clock
  if(seed == 0)
    seed = time(NULL) + clock();

  seed = seed * 1664525 + 1013904223;

  value = (seed & 0xFFFFFFFF) * range / 0xFFFFFFFF;
  return (int)value;
}

// Currently, just does a cheap delay..

void delay(int ms)
{
  SDL_Delay(ms);
}

int get_ticks(void)
{
  return SDL_GetTicks();
}
