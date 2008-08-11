/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
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

//Externs for SCRDISP.CPP

#ifndef __SCRDISP_H
#define __SCRDISP_H

#include "world.h"
#include "robot.h"

// Type 0/1 to DISPLAY a scroll/sign

void scroll_edit(World *mzx_world, Scroll *scroll, int type);
void scroll_frame(World *mzx_world, Scroll *scroll, int pos);
// type == 0 scroll, 1 sign, 2 scroll edit
void scroll_edging(World *mzx_world, int type);
void help_display(World *mzx_world, char *help, int offs,
 char *file, char *label);
void help_frame(World *mzx_world, char *help, int pos);
char print(char *str);
int strlencolor(char *str);

#endif
