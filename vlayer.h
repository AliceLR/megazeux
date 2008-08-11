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

#ifndef VLAYER_H
#define VLAYER_H

extern unsigned char far *vlayer_chars;
extern unsigned char far *vlayer_colors;
extern int vlayer_ems;
extern unsigned int vlayer_width;
extern unsigned int vlayer_height;

void allocate_vlayer();
void deallocate_vlayer();
void map_vlayer();
void unmap_vlayer();
void swap_size();

#endif
