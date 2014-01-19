/* MegaZeux
 *
 * Copyright (C) 2013 Alice Lauren Rowan <petrifiedrowan@gmail.com>
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

#include "event.h"
#include "error.h"
#include "counter.h"
#include "robot.h"
#include "world_struct.h"
#include "board_struct.h"
#include "robot_struct.h"

void run_robot_poll_event(struct world *mzx_world, struct robot *cur_robot)
{
  update_event_status();
  
  if(/*get_key_status(keycode_internal, IKEY_ESCAPE) || */get_key_status(keycode_internal, IKEY_BREAK))
  {
    struct board *cur_board = mzx_world->current_board;

    // End all robot programs.
    for(int i = 0; i < cur_board->num_robots + 1; i++)
    {
      struct robot *r = cur_board->robot_list[i];
      if(r)
      {
        r->status = 1;
        r->walk_dir = 0;
        r->cur_prog_line = 0;
        r->pos_within_line = 0;
      }
    }

    set_counter(mzx_world, "LIVES", 0, 0);
    set_counter(mzx_world, "HEALTH", 0, 0);

    error("Robot execution halted.", 1, 8, 0);
  }
}