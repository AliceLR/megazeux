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

/* EZBOARD.H- Declarations for EZBOARD.CPP */

#ifndef __EZBOARD_H
#define __EZBOARD_H

// See EZBOARD.CPP for very detailed function descriptions.
void board_setup(void);
void store_current(void);
void clear_current_and_select(unsigned char id,char adopt_settings=1);
void select_current(unsigned char id);
void swap_with(unsigned char id);
void delete_board(unsigned char id);
void clear_current(char adopt_settings=0);
void clear_world(char clear_curr_file=1);
void clear_game_params(void);
void clear_zero_objects(void);
extern int built_in_messages; // Stupid place to put this, as it belongs to data.h

#endif