/* MegaZeux
 *
 * Copyright (C) 2012 Alice Lauren Rowan <petrifiedrowan@gmail.com>
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

#include "../world_struct.h"

__M_BEGIN_DECLS

EDITOR_LIBSPEC void __edit_breakpoints(struct world *mzx_world);
EDITOR_LIBSPEC int __debug_robot(struct world *mzx_world,
 struct robot *cur_robot, int id);

EDITOR_LIBSPEC void free_breakpoints(void);
EDITOR_LIBSPEC void pause_robot_debugger(void);

struct break_point
{
  char match_name[15];
  char match_string[61];
  int index[256];
};

__M_END_DECLS
