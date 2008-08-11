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

// Prototypes for saveload.cpp

#ifndef __SAVELOAD_H
#define __SAVELOAD_H

#include <_null.h>

//Returns 0 for save name in curr_file, 1/-1 for cancel.
char save_world_dialog(void);
char save_game_dialog(void);

void save_world(char far *file,char savegame=0,char faded=0);
//Returns non-0 for error so you don't jump to main board
char load_world(char far *file,char edit=0,char savegame=0,char *faded=NULL);

//Loading save games DOESN'T select curr_board as current!

#endif