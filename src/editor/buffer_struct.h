/* MegaZeux
 *
 * Copyright (C) 2018 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __EDITOR_BUFFER_STRUCT_H
#define __EDITOR_BUFFER_STRUCT_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "../data.h"
#include "../world_struct.h"

struct buffer_info
{
  struct robot *robot;
  struct scroll *scroll;
  struct sensor *sensor;
  enum thing id;
  int color;
  int param;
};

__M_END_DECLS

#endif /* __EDITOR_BUFFER_STRUCT_H */
