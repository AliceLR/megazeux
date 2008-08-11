/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
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

/* Structure definitions */

#ifndef __STRUCT_H
#define __STRUCT_H

struct Robot
{
  unsigned int program_length;
  unsigned int program_location;//Offsetted from start of global robot memory
  char robot_name[15];
  unsigned char robot_char;
  unsigned int cur_prog_line;//Location of start of line (pt to FF for none)
  unsigned char pos_within_line;//Countdown for GO and WAIT
  unsigned char robot_cycle;
  unsigned char cycle_count;
  unsigned char bullet_type;
  unsigned char is_locked;
  unsigned char can_lavawalk;//Can always travel on fire
  unsigned char walk_dir;//1-4, of course
  unsigned char last_touch_dir;//1-4, of course
  unsigned char last_shot_dir;//1-4, of course
  int xpos,ypos;//Used for IF ALIGNED "robot", THISX/THISY, PLAYERDIST,
            //HORIZPLD, VERTPLD, and others. Keep udpated at all
            //times. Set to -1/-1 for global robot.
  char status;//0=Un-run yet, 1=Was run but only END'd, WAIT'd, or was
          //inactive, 2=To be re-run on a second robot-run due to a
          //rec'd message
  int blank;//Always 0
  char used;//Set to 1 if used onscreen, 0 if not
  int loop_count;//Loop count. Loops go back to first seen LOOP
                  //START, loop at first seen LOOP #, and an ABORT
                  //LOOP jumps to right after first seen LOOP #.
};
typedef struct Robot Robot;

struct Scroll
{
  unsigned int num_lines;
  unsigned int mesg_location;//Offset into global robot memory
  unsigned int mesg_size;
  char used;//Set to 1 if used onscreen, 0 if not
};
typedef struct Scroll Scroll;

struct Counter
{
  char counter_name[15];
  int counter_value;
};

typedef struct Counter Counter;

struct Sensor
{
  char sensor_name[15];
  unsigned char sensor_char;
  char robot_to_mesg[15];
  char used;//Set to 1 if used onscreen, 0 if not
};
typedef struct Sensor Sensor;

#endif
