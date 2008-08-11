/* $Id$
 * MegaZeux
 *
 * Copyright (C) 2003 Gilead Kutnick
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

#include "ems.h"
#include "data.h"

char far *vlayer_chars;
char far *vlayer_colors;
int ems_saved;
int vlayer_ems;
unsigned int vlayer_width = 256;
unsigned int vlayer_height = 128;

int vlayer_allocated = 0;

void allocate_vlayer()
{
  ems_saved = alloc_EMS(1);
  vlayer_ems = alloc_EMS(4);
  vlayer_chars = page_frame_EMS;
  vlayer_colors = page_frame_EMS + 32768;
  vlayer_allocated = 1;
}

void deallocate_vlayer()
{
  free_EMS(vlayer_ems);
}

void map_vlayer()
{
  if(!vlayer_allocated) allocate_vlayer();
  save_map_state_EMS(ems_saved);
  map_page_EMS(vlayer_ems, 0, 0);
	map_page_EMS(vlayer_ems, 1, 1);
	map_page_EMS(vlayer_ems, 2, 2);
	map_page_EMS(vlayer_ems, 3, 3);
}

void unmap_vlayer()
{
  restore_map_state_EMS(ems_saved);
}

void swap_size()
{
  int temp1, temp2;
  temp1 = board_xsiz;
  temp2 = board_ysiz;
  board_xsiz = vlayer_width;
  board_ysiz = vlayer_height;
  vlayer_width = temp1;
  vlayer_height = temp2;
}  
