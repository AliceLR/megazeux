/* MegaZeux
 *
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

#ifndef __ROBOT_STRUCT_H
#define __ROBOT_STRUCT_H

#include "compat.h"

__M_BEGIN_DECLS

#include "data.h"

struct label
{
  // Point this to the name in the robot
  char *name;
  int position;
  // Used for zapping.
  int cmd_position;
  // Set to 1 if zapped
  int zapped;
};

struct scroll
{
  int num_lines;

  // Pointer to scroll's message
  char *mesg;
  size_t mesg_size;

  char used;
};

struct sensor
{
  char sensor_name[15];
  char sensor_char;
  char robot_to_mesg[15];

  char used;
};

struct robot
{
  int world_version;
#ifdef CONFIG_DEBYTECODE
  int program_source_length;
  char *program_source;
#endif
  int program_bytecode_length;
  char *program_bytecode;         // Pointer to robot's program
  char robot_name[15];
  unsigned char robot_char;
  // Location of start of line (pt to FF for none)
  int cur_prog_line;
  int pos_within_line;            // Countdown for GO and WAIT
  int robot_cycle;
  int cycle_count;
  char bullet_type;
  char is_locked;
  char can_lavawalk;              // Can always travel on fire
  enum dir walk_dir;
  enum dir last_touch_dir;
  enum dir last_shot_dir;

  // Used for IF ALIGNED "robot", THISX/THISY, PLAYERDIST,
  // HORIZPLD, VERTPLD, and others. Keep udpated at all
  // times. Set to -1/-1 for global robot.
  int xpos, ypos;

  // 0 = Un-run yet, 1=Was run but only END'd, WAIT'd, or was
  // inactive, 2=To be re-run on a second robot-run due to a
  // rec'd message
  char status;

  // This is deprecated. It's only there for legacy reasons.
  char used;

  // Loop count. Loops go back to first seen LOOP
  // START, loop at first seen LOOP #, and an ABORT
  // LOOP jumps to right after first seen LOOP #.
  int loop_count;

  int num_labels;
  struct label **label_list;

  int stack_size;
  int stack_pointer;
  int *stack;

  // Local counters - store in save file
  int local[32];
};

__M_END_DECLS

#endif // __ROBOT_STRUCT_H
