/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
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

#include "robot.h"

#include "../robot.h"

#include <string.h>

void create_blank_robot_direct(Robot *cur_robot, int x, int y)
{
  char *program = malloc(2);

  memset(cur_robot, 0, sizeof(Robot));

  cur_robot->robot_name[0] = 0;
  cur_robot->program = program;
  cur_robot->program_length = 2;
  program[0] = 0xFF;
  program[1] = 0x00;

  cur_robot->xpos = x;
  cur_robot->ypos = y;
  cur_robot->robot_char = 2;
  cur_robot->bullet_type = 1;
  cur_robot->used = 1;
}

void create_blank_scroll_direct(Scroll *cur_scroll)
{
  char *message = malloc(3);

  memset(cur_scroll, 0, sizeof(Scroll));

  cur_scroll->num_lines = 1;
  cur_scroll->mesg_size = 3;
  cur_scroll->mesg = message;
  cur_scroll->used = 1;
  message[0] = 0x01;
  message[1] = '\n';
  message[2] = 0x00;
}

void create_blank_sensor_direct(Sensor *cur_sensor)
{
  memset(cur_sensor, 0, sizeof(Sensor));
  cur_sensor->used = 1;
}

void clear_scroll_contents(Scroll *cur_scroll)
{
  free(cur_scroll->mesg);
}

void replace_scroll(Board *src_board, Scroll *src_scroll, int dest_id)
{
  Scroll *cur_scroll = src_board->scroll_list[dest_id];
  clear_scroll_contents(cur_scroll);
  duplicate_scroll_direct(src_scroll, cur_scroll);
}

void replace_sensor(Board *src_board, Sensor *src_sensor, int dest_id)
{
  Sensor *cur_sensor = src_board->sensor_list[dest_id];
  duplicate_sensor_direct(src_sensor, cur_sensor);
}
