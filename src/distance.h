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

#ifndef __DISTANCE_H
#define __DISTANCE_H

enum distance_type
{
  DISTANCE_MANHATTAN = 0,
  DISTANCE_MIN_AXIS,
  DISTANCE_X_ONLY,
  DISTANCE_Y_ONLY,
};

typedef int (*distance_fn_t)(int x0, int y0, int x1, int y1);

int distance_manhattan(int x0, int y0, int x1, int y1);
int distance_min_axis(int x0, int y0, int x1, int y1);
int distance_x_only(int x0, int y0, int x1, int y1);
int distance_y_only(int x0, int y0, int x1, int y1);
distance_fn_t get_distance_type_function(enum distance_type dtype);

#endif // __DISTANCE_H
