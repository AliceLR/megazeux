/* MegaZeux
 *
 * Copyright (C) 2019 Ben Russell <thematrixeatsyou@gmail.com>
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

// Distance functions.

#include "distance.h"

#include <stdlib.h>

// Manhattan distance (adx+ady)
int distance_manhattan(int x0, int y0, int x1, int y1)
{
  int adx = abs(x0 - x1);
  int ady = abs(y0 - y1);
  return adx + ady;
}

// Min axis distance (min(adx,ady))
int distance_min_axis(int x0, int y0, int x1, int y1)
{
  int adx = abs(x0 - x1);
  int ady = abs(y0 - y1);
  return (adx < ady ? adx : ady);
}
