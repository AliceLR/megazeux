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

#ifndef __EDITOR_ROBOT_H
#define __EDITOR_ROBOT_H

#include "compat.h"

__M_BEGIN_DECLS

#include "../board_struct.h"
#include "../robot_struct.h"

void create_blank_robot_direct(Robot *cur_robot, int x, int y);
void create_blank_scroll_direct(Scroll *cur_croll);
void create_blank_sensor_direct(Sensor *cur_sensor);
void clear_scroll_contents(Scroll *cur_scroll);
void replace_scroll(Board *src_board, Scroll *src_scroll, int dest_id);
void replace_sensor(Board *src_board, Sensor *src_sensor, int dest_id);

__M_END_DECLS

#endif // __EDITOR_ROBOT_H
